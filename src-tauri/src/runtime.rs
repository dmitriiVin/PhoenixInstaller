//! Process execution shared by the installer and cleanup guards.
use std::{
    io::{Read, Write},
    process::{Command, Stdio},
};
pub fn run(program: &str, args: &[&str], input: Option<&str>) -> Result<String, String> {
    let mut command = Command::new(program);
    command
        .args(args)
        .stdin(if input.is_some() {
            Stdio::piped()
        } else {
            Stdio::null()
        })
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    let mut child = command.spawn().map_err(|e| format!("{program}: {e}"))?;
    let mut stderr = child.stderr.take().ok_or("stderr unavailable")?;
    let reader = std::thread::spawn(move || {
        let mut s = String::new();
        let _ = stderr.read_to_string(&mut s);
        s
    });
    if let Some(data) = input {
        child
            .stdin
            .take()
            .ok_or("stdin unavailable")?
            .write_all(data.as_bytes())
            .map_err(|e| e.to_string())?;
    }
    let mut stdout = String::new();
    child
        .stdout
        .take()
        .ok_or("stdout unavailable")?
        .read_to_string(&mut stdout)
        .map_err(|e| e.to_string())?;
    let status = child.wait().map_err(|e| e.to_string())?;
    let error = reader.join().map_err(|_| "stderr reader failed")?;
    if !status.success() {
        return Err(format!("{program} ({status}): {error}\n{stdout}"));
    }
    Ok(stdout)
}
pub fn privileged(program: &str, args: &[&str], input: Option<&str>) -> Result<String, String> {
    let root = run("id", &["-u"], None)?.trim() == "0";
    if root {
        run(program, args, input)
    } else {
        let mut elevated = vec![program];
        elevated.extend_from_slice(args);
        run("pkexec", &elevated, input)
    }
}
pub struct Mounts(pub Vec<String>);
impl Mounts {
    pub fn mount(&mut self, args: &[&str], target: &str) -> Result<(), String> {
        privileged("mount", args, None)?;
        self.0.push(target.into());
        Ok(())
    }
    pub fn unmount(&mut self, target: &str) -> Result<(), String> {
        privileged("umount", &[target], None)?;
        self.0.retain(|p| p != target);
        Ok(())
    }
}
impl Drop for Mounts {
    fn drop(&mut self) {
        for target in self.0.iter().rev() {
            let _ = privileged("umount", &[target], None);
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn captures_errors() {
        assert!(run("/usr/bin/false", &[], None).is_err());
    }
    #[test]
    fn pipes_input() {
        assert_eq!(run("/bin/cat", &[], Some("payload")).unwrap(), "payload");
    }
}

pub fn progress(program: &str, args: &[&str], report: &dyn Fn(f32)) -> Result<(), String> {
    let root = run("id", &["-u"], None)?.trim() == "0";
    let mut command = Command::new(if root { program } else { "pkexec" });
    if !root {
        command.arg(program);
    }
    let mut child = command
        .args(args)
        .env("LC_ALL", "C")
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| e.to_string())?;
    let mut stderr = child.stderr.take().ok_or("stderr unavailable")?;
    let reader = std::thread::spawn(move || {
        let mut s = String::new();
        let _ = stderr.read_to_string(&mut s);
        s
    });
    let mut output = child.stdout.take().ok_or("stdout unavailable")?;
    let mut byte = [0];
    let mut line = Vec::new();
    while output.read(&mut byte).map_err(|e| e.to_string())? > 0 {
        if byte[0] == b'\r' || byte[0] == b'\n' {
            if let Some(p) = percentage(&String::from_utf8_lossy(&line)) {
                report(p);
            }
            line.clear();
        } else {
            line.push(byte[0]);
            if line.len() > 65536 {
                line.clear();
            }
        }
    }
    let status = child.wait().map_err(|e| e.to_string())?;
    let stderr = reader.join().map_err(|_| "stderr reader failed")?;
    if !status.success() {
        return Err(format!("{program} ({status}): {stderr}"));
    }
    report(1.0);
    Ok(())
}
fn percentage(line: &str) -> Option<f32> {
    let end = line.find('%')?;
    let start = line[..end]
        .rfind(|c: char| !c.is_ascii_digit() && c != '.')
        .map_or(0, |i| i + 1);
    line[start..end]
        .parse::<f32>()
        .ok()
        .map(|p| (p / 100.0).clamp(0.0, 1.0))
}
#[cfg(test)]
mod progress_tests {
    use super::*;
    #[test]
    fn reads_wim_and_rsync_percentages() {
        assert_eq!(percentage("Applying: 24%"), Some(0.24));
        assert_eq!(percentage(" 1,234  99%  1MB/s"), Some(0.99));
    }
}
