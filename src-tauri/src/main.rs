mod disks;
mod model;
mod partition;
#[tauri::command]
fn preview_config(settings: model::Settings) -> Result<serde_json::Value, String> {
    settings.post_install()
}
#[tauri::command]
fn packages() -> Result<serde_json::Value, String> {
    serde_json::from_str(include_str!("../../packages.json")).map_err(|e| e.to_string())
}
#[tauri::command]
fn disks() -> Result<serde_json::Value, String> {
    disks::enumerate()
}
#[tauri::command]
fn preview_partition_plan(settings: model::Settings) -> Result<partition::Plan, String> {
    settings.validate()?;
    if settings.mode == "reinstall_windows" {
        return Err("Переустановка использует существующие разделы".into());
    }
    let inventory = disks::enumerate()?;
    let disk = inventory["blockdevices"]
        .as_array()
        .and_then(|a| {
            a.iter()
                .find(|d| d["name"].as_str() == Some(&settings.device))
        })
        .ok_or("Выбранный диск больше не доступен")?;
    fn number(v: &serde_json::Value) -> Option<u64> {
        v.as_u64().or_else(|| v.as_str()?.parse().ok())
    }
    let mut parts = Vec::new();
    if let Some(children) = disk["children"].as_array() {
        for p in children {
            if p["type"].as_str() != Some("part") {
                continue;
            }
            parts.push(partition::Partition {
                number: u32::try_from(number(&p["partn"]).ok_or("Неизвестный номер раздела")?)
                    .map_err(|e| e.to_string())?,
                start: number(&p["start"]).ok_or("Неизвестное начало раздела")?,
                sectors: number(&p["size"]).ok_or("Неизвестный размер раздела")? / 512,
                label: p["label"].as_str().unwrap_or("").into(),
                part_label: p["partlabel"].as_str().unwrap_or("").into(),
            });
        }
    }
    partition::plan(
        number(&disk["size"]).ok_or("Неизвестный размер диска")? / 512,
        disk["pttype"].as_str() == Some("gpt"),
        &parts,
        if settings.mode == "reinstall_windows_and_format_data" {
            120
        } else {
            settings.windows_size_gb
        },
    )
}
fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            packages,
            disks,
            preview_config,
            preview_partition_plan
        ])
        .run(tauri::generate_context!())
        .expect("Unable to run Phoenix Installer");
}
