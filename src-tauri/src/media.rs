use crate::runtime::{privileged, Mounts};
use serde_json::Value;
use std::path::PathBuf;
fn mounted(node: &Value) -> Option<&str> {
    node["mountpoints"]
        .as_array()?
        .iter()
        .filter_map(Value::as_str)
        .find(|s| !s.is_empty())
}
fn ventoy(node: &Value) -> bool {
    (node["label"]
        .as_str()
        .is_some_and(|s| s.eq_ignore_ascii_case("ventoy"))
        && mounted(node).is_some_and(|s| s.starts_with("/run/media/")))
        || node["children"]
            .as_array()
            .is_some_and(|a| a.iter().any(ventoy))
}
pub fn find(tree: &Value) -> Result<(&Value, &Value), String> {
    let matches: Vec<_> = tree["blockdevices"]
        .as_array()
        .ok_or("Некорректный список дисков")?
        .iter()
        .filter(|d| d["type"] == "disk" && ventoy(d))
        .flat_map(|d| {
            d["children"]
                .as_array()
                .into_iter()
                .flatten()
                .filter(|p| {
                    p["type"] == "part"
                        && p["label"]
                            .as_str()
                            .is_some_and(|s| s.eq_ignore_ascii_case("data"))
                })
                .map(move |p| (d, p))
        })
        .collect();
    if matches.len() != 1 {
        return Err("Не найден однозначный раздел Data на диске Ventoy".into());
    }
    Ok(matches[0])
}
pub fn prepare(tree: &Value, target: &str, mounts: &mut Mounts) -> Result<PathBuf, String> {
    let (disk, data) = find(tree)?;
    if disk["name"] == target {
        return Err("Целевой диск содержит загрузочный Ventoy".into());
    }
    let path = if let Some(m) = mounted(data) {
        PathBuf::from(m)
    } else {
        let target = "/mnt/phoenix-live-data";
        privileged("mkdir", &["-p", target], None)?;
        let dev = data["path"].as_str().ok_or("Неизвестное устройство Data")?;
        if mounts
            .mount(&["-t", "ntfs3", "-o", "ro", dev, target], target)
            .is_err()
        {
            mounts.mount(&["-o", "ro", dev, target], target)?;
        }
        PathBuf::from(target)
    };
    for folder in ["Drivers", "Programs"] {
        if !path.join(folder).is_dir() {
            return Err(format!("Не найдена папка {folder}: {}", path.display()));
        }
    }
    Ok(path)
}
#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    #[test]
    fn nested_ventoy_selects_sibling_data() {
        let t = json!({"blockdevices":[{"name":"sda","type":"disk","children":[{"type":"part","children":[{"label":"Ventoy","mountpoints":["/run/media/user/Ventoy"]}]},{"type":"part","label":"Data","path":"/dev/sda3"}]}]});
        assert_eq!(find(&t).unwrap().1["path"], "/dev/sda3");
    }
}
