use crate::{
    deploy, disks, media,
    model::Settings,
    partition,
    runtime::{privileged, run, Mounts},
};
use serde_json::Value;
use std::path::Path;
fn num(v: &Value) -> Result<u64, String> {
    v.as_u64()
        .or_else(|| v.as_str()?.parse().ok())
        .ok_or("Неизвестная геометрия диска".into())
}
fn device(disk: &str, n: u32) -> String {
    format!(
        "{disk}{}{n}",
        if disk.ends_with(|c: char| c.is_ascii_digit()) {
            "p"
        } else {
            ""
        }
    )
}
pub fn execute(
    settings: Settings,
    resources: &Path,
    report: &dyn Fn(f32, &str),
) -> Result<(), String> {
    settings.post_install()?;
    if !cfg!(target_os = "linux") {
        return Err("Установка доступна только в Linux Live".into());
    }
    let uefi = Path::new("/sys/firmware/efi").is_dir();
    for file in [
        "bcd-sys-2.4-x86_64.AppImage",
        "assets/PSetup.exe",
        "assets/Scripts/123.cmd",
        "assets/Scripts/unattend.xml",
    ] {
        if !resources.join(file).is_file() {
            return Err(format!("Не найден ресурс: {file}"));
        }
    }
    if !uefi && !resources.join("ms-sys").is_file() {
        return Err("Не найден ms-sys".into());
    }
    if !Path::new("/Windows10.iso").is_file() {
        return Err("Не найден /Windows10.iso".into());
    }
    for tool in [
        "lsblk",
        "findmnt",
        "swapon",
        "mount",
        "umount",
        "wimlib-imagex",
        "mkfs.ntfs",
        "rsync",
        "partprobe",
        "udevadm",
    ] {
        run("which", &[tool], None)?;
    }
    if settings.mode != "reinstall_windows" {
        for tool in if uefi {
            vec!["sgdisk", "mkfs.fat"]
        } else {
            vec!["sfdisk", "wipefs"]
        } {
            run("which", &[tool], None)?;
        }
    }
    let tree = disks::enumerate()?;
    let disk = tree["blockdevices"]
        .as_array()
        .and_then(|a| {
            a.iter()
                .find(|d| d["name"].as_str() == Some(&settings.device))
        })
        .ok_or("Целевой диск исчез")?;
    if num(&disk["log-sec"])? != 512 {
        return Err("Этот установочный сценарий требует логические секторы 512 байт".into());
    }
    let target = format!("/dev/{}", settings.device);
    let children = disk["children"].as_array().cloned().unwrap_or_default();
    for node in std::iter::once(disk).chain(children.iter()) {
        if node["mountpoints"]
            .as_array()
            .is_some_and(|a| a.iter().any(|p| p.as_str().is_some_and(|s| !s.is_empty())))
        {
            return Err("На целевом диске есть смонтированные разделы".into());
        }
    }
    let swaps = run("swapon", &["--show=NAME", "--noheadings"], None)?;
    if swaps.lines().any(|s| {
        s.trim() == target
            || children
                .iter()
                .any(|p| p["path"].as_str() == Some(s.trim()))
    }) {
        return Err("Целевой диск использует swap".into());
    }
    let mut mounts = Mounts(Vec::new());
    report(0.02, "Поиск файлов на Live-флешке");
    let live = media::prepare(&tree, &settings.device, &mut mounts)?;
    // Validate WIM and edition before touching the target disk.
    privileged("mkdir", &["-p", "/mnt/phoenix-preflight"], None)?;
    mounts.mount(
        &["-o", "loop,ro", "/Windows10.iso", "/mnt/phoenix-preflight"],
        "/mnt/phoenix-preflight",
    )?;
    privileged(
        "wimlib-imagex",
        &[
            "info",
            "/mnt/phoenix-preflight/sources/install.wim",
            &settings.edition.to_string(),
        ],
        None,
    )?;
    mounts.unmount("/mnt/phoenix-preflight")?;
    let (windows, efi) = if settings.mode == "reinstall_windows" {
        let candidates: Vec<_> = children
            .iter()
            .filter(|p| {
                p["role"] == "windows"
                    || (p["fstype"] == "ntfs"
                        && p["label"]
                            .as_str()
                            .is_some_and(|s| s.eq_ignore_ascii_case("windows")))
            })
            .collect();
        if candidates.len() != 1 {
            return Err("Не найден однозначный раздел Windows".into());
        }
        let windows = candidates[0]["path"]
            .as_str()
            .ok_or("Неизвестный путь Windows")?
            .to_owned();
        let efi = if uefi {
            let candidates: Vec<_> = children.iter().filter(|p| p["role"] == "efi").collect();
            if candidates.len() != 1 {
                return Err("Не найден однозначный EFI-раздел".into());
            }
            Some(
                candidates[0]["path"]
                    .as_str()
                    .ok_or("Неизвестный путь EFI")?
                    .to_owned(),
            )
        } else {
            None
        };
        (windows, efi)
    } else if uefi {
        let mut parts = Vec::new();
        for p in &children {
            if p["type"] != "part" {
                return Err("Обнаружено вложенное блочное устройство".into());
            }
            parts.push(partition::Partition {
                number: u32::try_from(num(&p["partn"])?).map_err(|e| e.to_string())?,
                start: num(&p["start"])?,
                sectors: num(&p["size"])? / 512,
                label: p["label"].as_str().unwrap_or("").into(),
                part_label: p["partlabel"].as_str().unwrap_or("").into(),
            });
        }
        let plan = partition::plan(
            num(&disk["size"])? / 512,
            disk["pttype"] == "gpt",
            &parts,
            if settings.mode == "reinstall_windows_and_format_data" {
                120
            } else {
                settings.windows_size_gb
            },
        )?;
        report(0.1, "Создание GPT-разметки с сохранением AMDZ");
        if plan.reset_table {
            privileged("sgdisk", &["--zap-all", &target], None)?;
            privileged("sgdisk", &["-o", &target], None)?;
        } else {
            for n in &plan.delete {
                privileged("sgdisk", &["-d", &n.to_string(), &target], None)?;
            }
        }
        for p in &plan.create {
            privileged(
                "sgdisk",
                &[
                    "-n",
                    &format!("{}:{}:{}", p.number, p.start, p.end),
                    "-t",
                    &format!("{}:{}", p.number, p.kind),
                    "-c",
                    &format!("{}:{}", p.number, p.name),
                    &target,
                ],
                None,
            )?;
        }
        settle(&target)?;
        let efi = device(&target, plan.create[0].number);
        let windows = device(&target, plan.create[2].number);
        let data = device(&target, plan.create[3].number);
        privileged("mkfs.fat", &["-F32", "-n", "EFI", &efi], None)?;
        privileged("mkfs.ntfs", &["-f", "-L", "Data", &data], None)?;
        (windows, Some(efi))
    } else {
        if children.iter().any(|p| {
            ["label", "partlabel"].iter().any(|k| {
                p[k].as_str()
                    .is_some_and(|s| s.eq_ignore_ascii_case("amdz"))
            })
        }) {
            return Err("AMDZ нельзя сохранить при Legacy-очистке. Загрузитесь в UEFI".into());
        }
        let sectors = num(&disk["size"])? / 512;
        let size = if settings.mode == "reinstall_windows_and_format_data" {
            120
        } else {
            settings.windows_size_gb
        }
        .checked_mul(2097152)
        .ok_or("Слишком большой размер Windows")?;
        if sectors <= 2048 + size + 2048 || sectors > u32::MAX as u64 {
            return Err("Размер диска не подходит для MBR-разметки".into());
        }
        let table=format!("label: dos\nunit: sectors\n\nstart=2048, size={size}, type=7, bootable\nstart={}, size={}, type=7\n",2048+size,sectors-2048-size);
        report(0.1, "Создание MBR-разметки");
        privileged("wipefs", &["-a", &target], None)?;
        privileged("sfdisk", &[&target], Some(&table))?;
        settle(&target)?;
        privileged(
            "mkfs.ntfs",
            &["-f", "-L", "Data", &device(&target, 2)],
            None,
        )?;
        (device(&target, 1), None)
    };
    deploy::image(
        &settings,
        &windows,
        efi.as_deref(),
        &target,
        resources,
        &mut mounts,
        report,
    )?;
    deploy::stage(&settings, resources, &live, report)?;
    report(0.99, "Завершение установки");
    mounts.unmount("/mnt/phoenix-windows")?;
    run("sync", &[], None)?;
    drop(mounts);
    report(1.0, "Установка завершена. Перезагрузка");
    privileged("systemctl", &["reboot"], None)?;
    Ok(())
}
fn settle(disk: &str) -> Result<(), String> {
    privileged("partprobe", &[disk], None)?;
    privileged("udevadm", &["settle"], None)?;
    Ok(())
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn partition_device_names() {
        assert_eq!(device("/dev/sda", 2), "/dev/sda2");
        assert_eq!(device("/dev/nvme0n1", 2), "/dev/nvme0n1p2");
    }
}
