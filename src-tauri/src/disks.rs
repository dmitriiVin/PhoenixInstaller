use serde_json::{json, Value};
const EFI: &str = "c12a7328-f81f-11d2-ba4b-00a0c93ec93b";
const MSR: &str = "e3c9e316-0b5c-4db8-817d-f92df00215ae";
fn text<'a>(v: &'a Value, key: &str) -> &'a str {
    v[key].as_str().unwrap_or("")
}
fn role(p: &Value, layout: bool) -> &'static str {
    if p["mountpoints"].as_array().is_some_and(|a| {
        a.iter().filter_map(Value::as_str).any(|m| {
            std::path::Path::new(m)
                .join("Windows/System32/Config/SYSTEM")
                .exists()
        })
    }) {
        return "windows";
    }
    match text(p, "parttype").to_ascii_lowercase().as_str() {
        EFI => "efi",
        MSR => "msr",
        "de94bba4-06d1-4d40-a16a-bfd50179d6ac" => "recovery",
        _ if text(p, "label").eq_ignore_ascii_case("data") => "data",
        "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7"
            if layout && text(p, "fstype").eq_ignore_ascii_case("ntfs") =>
        {
            "windows"
        }
        _ => "unknown",
    }
}
pub fn enumerate() -> Result<Value, String> {
    if !cfg!(target_os = "linux") {
        return Err("Обнаружение дисков доступно в Linux Live.".into());
    }
    let output=std::process::Command::new("lsblk").args(["--json","--bytes","--output","NAME,PATH,SIZE,MODEL,TYPE,FSTYPE,LABEL,MOUNTPOINTS,TRAN,ROTA,PTTYPE,PARTTYPE,PARTN,START,PARTLABEL"]).output().map_err(|e|e.to_string())?;
    if !output.status.success() {
        return Err(String::from_utf8_lossy(&output.stderr).into_owned());
    }
    let mut data: Value = serde_json::from_slice(&output.stdout).map_err(|e| e.to_string())?;
    let disks = data["blockdevices"]
        .as_array_mut()
        .ok_or("Некорректный ответ lsblk")?;
    disks.retain(|d| text(d, "type") == "disk");
    disks.sort_by_key(|d| text(d, "name").to_owned());
    for d in disks {
        let children = d["children"].as_array().cloned().unwrap_or_default();
        let layout = text(d, "pttype") == "gpt"
            && [EFI, MSR].iter().all(|guid| {
                children
                    .iter()
                    .any(|p| text(p, "parttype").eq_ignore_ascii_case(guid))
            });
        if let Some(parts) = d["children"].as_array_mut() {
            for p in parts {
                p["role"] = json!(role(p, layout));
            }
        }
    }
    Ok(data)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn data_label_wins_over_windows_heuristic() {
        assert_eq!(
            role(
                &json!({"parttype":"ebd0a0a2-b9e5-4433-87c0-68b6b72699c7","fstype":"ntfs","label":"Data"}),
                true
            ),
            "data"
        );
    }
    #[test]
    fn ntfs_alone_is_not_windows() {
        assert_eq!(role(&json!({"fstype":"ntfs"}), false), "unknown");
    }
}
