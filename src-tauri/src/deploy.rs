//! Shared WIM deployment and bootloader configuration, independent of partition creation.
use crate::{
    model::Settings,
    runtime::{privileged, Mounts},
};
use std::path::Path;
pub fn image(
    settings: &Settings,
    windows: &str,
    efi: Option<&str>,
    disk: &str,
    resources: &Path,
    mounts: &mut Mounts,
    report: &dyn Fn(f32, &str),
) -> Result<(), String> {
    let iso = "/mnt/phoenix-winiso";
    let win = "/mnt/phoenix-windows";
    let esp = "/mnt/phoenix-efi";
    privileged("mkdir", &["-p", iso, win, esp], None)?;
    report(0.2, "Форматирование Windows");
    privileged("mkfs.ntfs", &["-f", "-L", "Windows", windows], None)?;
    mounts.mount(&["-o", "loop,ro", "/Windows10.iso", iso], iso)?;
    let wim = format!("{iso}/sources/install.wim");
    if !Path::new(&wim).is_file() {
        return Err("В ISO отсутствует sources/install.wim".into());
    }
    report(0.3, "Применение образа Windows");
    privileged(
        "wimlib-imagex",
        &["apply", &wim, &settings.edition.to_string(), windows],
        None,
    )?;
    mounts.unmount(iso)?;
    mounts.mount(&["-t", "ntfs3", windows, win], win)?;
    if !Path::new(win)
        .join("Windows/System32/ntoskrnl.exe")
        .is_file()
    {
        return Err("В установленном образе отсутствует ядро Windows".into());
    }
    report(0.82, "Настройка загрузчика");
    let bcd = resources.join("bcd-sys-2.4-x86_64.AppImage");
    let bcd = bcd.to_str().ok_or("Некорректный путь BCD-SYS")?;
    if let Some(efi) = efi {
        mounts.mount(&["-t", "vfat", efi, esp], esp)?;
        privileged(
            "env",
            &[
                "LC_ALL=C",
                bcd,
                win,
                "--firmware",
                "UEFI",
                "--syspath",
                esp,
                "--clean",
                "--locale",
                "ru-RU",
                "--prodname",
                "Windows 10 Enterprise",
                "--verbose",
            ],
            None,
        )?;
        for f in ["EFI/Microsoft/Boot/bootmgfw.efi", "EFI/Microsoft/Boot/BCD"] {
            if !Path::new(esp).join(f).is_file() {
                return Err(format!("Не найден {f}"));
            }
        }
        mounts.unmount(esp)?;
    } else {
        let ms = resources.join("ms-sys");
        let ms = ms.to_str().ok_or("Некорректный путь ms-sys")?;
        privileged(ms, &["--mbr7", disk], None)?;
        privileged(ms, &["--ntfs", windows], None)?;
        privileged(
            "env",
            &[
                "LC_ALL=C",
                bcd,
                win,
                "--firmware",
                "BIOS",
                "--clean",
                "--locale",
                "ru-RU",
                "--prodname",
                "Windows 10 Enterprise",
                "--verbose",
            ],
            None,
        )?;
        for f in ["bootmgr", "Boot/BCD"] {
            if !Path::new(win).join(f).is_file() {
                return Err(format!("Не найден {f}"));
            }
        }
    }
    Ok(())
}
pub fn stage(
    settings: &Settings,
    resources: &Path,
    live: &Path,
    report: &dyn Fn(f32, &str),
) -> Result<(), String> {
    let win = Path::new("/mnt/phoenix-windows");
    let post = win.join("Programs/PostInstall");
    let startup = win.join("ProgramData/Microsoft/Windows/Start Menu/Programs/Startup");
    let scripts = win.join("Windows/Setup/Scripts");
    for path in [
        &post,
        &startup,
        &scripts,
        &win.join("Windows/Panther"),
        &win.join("Windows/System32/Sysprep"),
    ] {
        privileged(
            "mkdir",
            &["-p", path.to_str().ok_or("Некорректный путь")?],
            None,
        )?;
    }
    let copy = |source: &Path, target: &Path| -> Result<(), String> {
        privileged(
            "cp",
            &[
                source.to_str().ok_or("Некорректный путь")?,
                target.to_str().ok_or("Некорректный путь")?,
            ],
            None,
        )?;
        Ok(())
    };
    copy(
        &resources.join("assets/Scripts/123.cmd"),
        &scripts.join("123.cmd"),
    )?;
    for target in [
        "Windows/Panther/unattend.xml",
        "Windows/System32/Sysprep/unattend.xml",
    ] {
        copy(
            &resources.join("assets/Scripts/unattend.xml"),
            &win.join(target),
        )?;
    }
    copy(
        &resources.join("assets/PSetup.exe"),
        &post.join("PSetup.exe"),
    )?;
    privileged(
        "tee",
        &[post
            .join("config.json")
            .to_str()
            .ok_or("Некорректный путь")?],
        Some(&serde_json::to_string_pretty(&settings.post_install()?).map_err(|e| e.to_string())?),
    )?;
    privileged(
        "tee",
        &[startup
            .join("PSetup.cmd")
            .to_str()
            .ok_or("Некорректный путь")?],
        Some("@echo off\r\nstart \"\" \"C:\\Programs\\PostInstall\\PSetup.exe\"\r\n"),
    )?;
    for (progress, folder) in [(0.91, "Drivers"), (0.95, "Programs")] {
        report(progress, &format!("Копирование {folder}"));
        privileged(
            "rsync",
            &[
                "-a",
                "--info=progress2",
                live.join(folder).to_str().ok_or("Некорректный путь")?,
                &format!("{}/", post.display()),
            ],
            None,
        )?;
    }
    Ok(())
}
