#[tauri::command]
fn packages() -> Result<serde_json::Value, String> {
    serde_json::from_str(include_str!("../../packages.json")).map_err(|e| e.to_string())
}
#[tauri::command]
fn disks() -> Result<serde_json::Value, String> {
    if !cfg!(target_os = "linux") {
        return Err("Обнаружение дисков доступно в Linux Live.".into());
    }
    let output = std::process::Command::new("lsblk")
        .args([
            "--json",
            "--bytes",
            "--output",
            "NAME,PATH,SIZE,MODEL,TYPE,FSTYPE,LABEL,MOUNTPOINTS,TRAN,ROTA,PTTYPE",
        ])
        .output()
        .map_err(|e| e.to_string())?;
    if !output.status.success() {
        return Err(String::from_utf8_lossy(&output.stderr).into_owned());
    }
    serde_json::from_slice(&output.stdout).map_err(|e| e.to_string())
}
fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![packages, disks])
        .run(tauri::generate_context!())
        .expect("Unable to run Phoenix Installer");
}
