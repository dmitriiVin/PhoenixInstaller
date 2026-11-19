mod install;
use std::sync::atomic::{AtomicBool, Ordering};
use tauri::{Emitter, Manager};
static INSTALLING: AtomicBool = AtomicBool::new(false);
mod deploy;
mod disks;
mod media;
mod model;
mod partition;
mod runtime;
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
fn preview_partition_plan(settings: model::Settings) -> Result<serde_json::Value, String> {
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
    if cfg!(target_os = "linux") && !std::path::Path::new("/sys/firmware/efi").exists() {
        if parts.iter().any(|p| {
            p.label.eq_ignore_ascii_case("amdz") || p.part_label.eq_ignore_ascii_case("amdz")
        }) {
            return Err("AMDZ нельзя сохранить при Legacy-очистке".into());
        }
        let sectors = number(&disk["size"]).ok_or("Неизвестный размер диска")? / 512;
        let size = if settings.mode == "reinstall_windows_and_format_data" {
            120
        } else {
            settings.windows_size_gb
        }
        .checked_mul(2097152)
        .ok_or("Слишком большой размер Windows")?;
        if sectors <= 2048 + size + 2048 || sectors > u32::MAX as u64 {
            return Err("Недостаточно места или диск слишком велик для MBR".into());
        }
        return Ok(
            serde_json::json!({"table":"dos","windows":{"start":2048,"sectors":size},"data":{"start":2048+size,"sectors":sectors-2048-size}}),
        );
    }
    serde_json::to_value(partition::plan(
        number(&disk["size"]).ok_or("Неизвестный размер диска")? / 512,
        disk["pttype"].as_str() == Some("gpt"),
        &parts,
        if settings.mode == "reinstall_windows_and_format_data" {
            120
        } else {
            settings.windows_size_gb
        },
    )?)
    .map_err(|e| e.to_string())
}
#[tauri::command]
async fn start_install(settings: model::Settings, app: tauri::AppHandle) -> Result<(), String> {
    settings.post_install()?;
    if INSTALLING.swap(true, Ordering::SeqCst) {
        return Err("Установка уже запущена".into());
    }
    let result = tauri::async_runtime::spawn_blocking(move || {
        let resources = app.path().resource_dir().map_err(|e| e.to_string())?;
        install::execute(settings, &resources, &|progress, status| {
            let _ = app.emit(
                "install-progress",
                serde_json::json!({"progress":progress,"status":status}),
            );
        })
    })
    .await
    .map_err(|e| e.to_string());
    INSTALLING.store(false, Ordering::SeqCst);
    result?
}
fn main() {
    tauri::Builder::default()
        .on_window_event(|_, event| {
            if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                if INSTALLING.load(Ordering::SeqCst) {
                    api.prevent_close();
                }
            }
        })
        .invoke_handler(tauri::generate_handler![
            packages,
            disks,
            preview_config,
            preview_partition_plan,
            start_install
        ])
        .run(tauri::generate_context!())
        .expect("Unable to run Phoenix Installer");
}
