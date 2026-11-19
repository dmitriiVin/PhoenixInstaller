use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
#[derive(Debug, Deserialize, Serialize)]
pub struct Settings {
    pub device: String,
    pub mode: String,
    pub windows_size_gb: u64,
    pub edition: u32,
    pub drivers: bool,
    pub change_name: bool,
    pub computer_name: String,
    pub packages: Vec<String>,
}
impl Settings {
    pub fn validate(&self) -> Result<(), String> {
        if self.device.is_empty() || self.device.contains('/') || self.device.contains("..") {
            return Err("Выберите физический диск".into());
        }
        if ![
            "reinstall_windows",
            "reinstall_windows_and_format_data",
            "clean_disk",
        ]
        .contains(&self.mode.as_str())
        {
            return Err("Неизвестный режим".into());
        }
        if self.edition == 0 {
            return Err("Укажите индекс редакции Windows".into());
        }
        if self.mode == "clean_disk" && self.windows_size_gb < 100 {
            return Err("Минимальный размер Windows — 100 ГиБ".into());
        }
        if self.change_name
            && (self.computer_name.is_empty()
                || self.computer_name.len() > 15
                || !self
                    .computer_name
                    .bytes()
                    .all(|b| b.is_ascii_alphanumeric() || b == b'-'))
        {
            return Err("Имя компьютера: 1–15 латинских букв, цифр или дефисов".into());
        }
        Ok(())
    }
    pub fn post_install(&self) -> Result<Value, String> {
        self.validate()?;
        let catalog: Vec<Value> =
            serde_json::from_str(include_str!("../../packages.json")).map_err(|e| e.to_string())?;
        let mut packages = Vec::new();
        for id in &self.packages {
            let p = catalog
                .iter()
                .find(|p| p["id"].as_str() == Some(id))
                .ok_or_else(|| format!("Не найден пакет: {id}"))?;
            packages.push(json!({"id":p["id"],"folder":p["folder"],"installer":p["installer"],"arguments":p["arguments"]}));
        }
        Ok(
            json!({"computer":{"change_name":self.change_name,"name":self.computer_name},"drivers":self.drivers,"packages":packages}),
        )
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    fn settings() -> Settings {
        Settings {
            device: "nvme0n1".into(),
            mode: "clean_disk".into(),
            windows_size_gb: 150,
            edition: 2,
            drivers: true,
            change_name: true,
            computer_name: "PC-01".into(),
            packages: vec!["7z".into()],
        }
    }
    #[test]
    fn config_matches_setup_contract() {
        let c = settings().post_install().unwrap();
        assert_eq!(c["packages"][0]["installer"], "SILENT.cmd");
        assert_eq!(c["computer"]["name"], "PC-01");
        assert_eq!(c["drivers"], true);
    }
    #[test]
    fn rejects_unknown_package() {
        let mut s = settings();
        s.packages.push("missing".into());
        assert!(s.post_install().is_err());
    }
    #[test]
    fn rejects_invalid_settings() {
        let mut s = settings();
        s.computer_name = "bad&name".into();
        assert!(s.validate().is_err());
        s = settings();
        s.windows_size_gb = 99;
        assert!(s.validate().is_err());
    }
}
