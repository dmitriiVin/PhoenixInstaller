use serde::{Deserialize, Serialize};
use std::collections::BTreeSet;
const MIB: u64 = 2048;
const GIB: u64 = 1024 * MIB;
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct Partition {
    pub number: u32,
    pub start: u64,
    pub sectors: u64,
    pub label: String,
    pub part_label: String,
}
#[derive(Debug, Serialize)]
pub struct NewPartition {
    pub number: u32,
    pub start: u64,
    pub end: u64,
    pub kind: String,
    pub name: String,
}
#[derive(Debug, Serialize)]
pub struct Plan {
    pub preserved: BTreeSet<u32>,
    pub delete: Vec<u32>,
    pub create: Vec<NewPartition>,
    pub reset_table: bool,
}
pub fn plan(
    disk_sectors: u64,
    gpt: bool,
    parts: &[Partition],
    windows_gb: u64,
) -> Result<Plan, String> {
    let fail = || "Некорректная или неоднозначная геометрия разделов".to_string();
    if disk_sectors < 4096 || windows_gb < 100 {
        return Err(fail());
    }
    for p in parts {
        if p.number == 0
            || p.sectors == 0
            || p.start
                .checked_add(p.sectors)
                .is_none_or(|end| end > disk_sectors)
        {
            return Err(fail());
        }
    }
    let mut sorted = parts.to_vec();
    sorted.sort_by_key(|p| p.start);
    if sorted
        .windows(2)
        .any(|p| p[0].start + p[0].sectors > p[1].start)
    {
        return Err(fail());
    }
    let amdz: Vec<_> = parts
        .iter()
        .filter(|p| {
            p.label.eq_ignore_ascii_case("amdz") || p.part_label.eq_ignore_ascii_case("amdz")
        })
        .collect();
    if amdz.len() > 1 {
        return Err("Найдено несколько AMDZ".into());
    }
    let mut preserved = BTreeSet::new();
    let mut start = 2048;
    let mut end = disk_sectors - 34;
    if let Some(a) = amdz.first() {
        if !gpt || a.start + a.sectors > disk_sectors - 33 {
            return Err(fail());
        }
        let ae = a.start + a.sectors - 1;
        let beginning = a.start <= 3 * GIB && ae < disk_sectors / 2;
        let ending = ae >= disk_sectors.saturating_sub(3 * GIB) && a.start >= disk_sectors / 2;
        if beginning == ending {
            return Err("AMDZ должен находиться у начала или конца диска".into());
        }
        preserved.insert(a.number);
        let neighbor = parts
            .iter()
            .filter(|p| p.number != a.number && (64 * MIB..=192 * MIB).contains(&p.sectors))
            .filter_map(|p| {
                let pe = p.start + p.sectors - 1;
                let distance = if pe < a.start {
                    a.start - pe - 1
                } else if p.start > ae {
                    p.start - ae - 1
                } else {
                    return None;
                };
                (distance <= MIB).then_some((distance, p))
            })
            .min_by_key(|(d, _)| *d)
            .map(|(_, p)| p);
        if let Some(p) = neighbor {
            preserved.insert(p.number);
        }
        if beginning {
            start = ae + 1;
            if let Some(p) = neighbor {
                if p.start > ae {
                    start = p.start + p.sectors;
                }
            }
            start = start.div_ceil(2048) * 2048;
        } else {
            end = a.start.saturating_sub(1);
            if let Some(p) = neighbor {
                if p.start < a.start {
                    end = p.start.saturating_sub(1);
                }
            }
            end = ((end + 1) / 2048) * 2048 - 1;
        }
    }
    let windows = windows_gb.checked_mul(GIB).ok_or_else(fail)?;
    let needed = (512 * MIB + 16 * MIB)
        .checked_add(windows)
        .and_then(|n| n.checked_add(GIB))
        .ok_or_else(fail)?;
    if end <= start || end - start < needed {
        return Err("Недостаточно места для EFI, Windows и Data".into());
    }
    let numbers: Vec<_> = (1..=128)
        .filter(|n| !preserved.contains(n))
        .take(4)
        .collect();
    let mut create = Vec::new();
    let mut pos = start;
    for (i, (size, kind, name)) in [
        (512 * MIB, "EF00", "EFI"),
        (16 * MIB, "0C01", "MSR"),
        (windows, "0700", "Windows"),
        (
            end - start + 1 - 512 * MIB - 16 * MIB - windows,
            "0700",
            "Data",
        ),
    ]
    .into_iter()
    .enumerate()
    {
        create.push(NewPartition {
            number: numbers[i],
            start: pos,
            end: pos + size - 1,
            kind: kind.into(),
            name: name.into(),
        });
        pos += size;
    }
    Ok(Plan {
        delete: parts
            .iter()
            .filter(|p| !preserved.contains(&p.number))
            .map(|p| p.number)
            .collect(),
        reset_table: preserved.is_empty(),
        preserved,
        create,
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn clean_layout_fits() {
        let p = plan(500 * GIB, true, &[], 150).unwrap();
        assert_eq!(p.create[2].end - p.create[2].start + 1, 150 * GIB);
        assert_eq!(p.create[3].end, 500 * GIB - 34);
    }
    #[test]
    fn amdz_at_start_and_neighbor_survive() {
        let parts = vec![
            Partition {
                number: 1,
                start: 2048,
                sectors: 128 * MIB,
                label: "".into(),
                part_label: "".into(),
            },
            Partition {
                number: 2,
                start: 2048 + 128 * MIB,
                sectors: GIB,
                label: "AMDZ".into(),
                part_label: "".into(),
            },
        ];
        let p = plan(500 * GIB, true, &parts, 150).unwrap();
        assert_eq!(p.preserved.len(), 2);
        assert!(p.create[0].start >= parts[1].start + parts[1].sectors);
        assert_eq!(p.create[0].number, 3);
    }
    #[test]
    fn amdz_in_middle_rejected() {
        assert!(plan(
            500 * GIB,
            true,
            &[Partition {
                number: 1,
                start: 250 * GIB,
                sectors: GIB,
                label: "AMDZ".into(),
                part_label: "".into()
            }],
            150
        )
        .is_err());
    }
    #[test]
    fn insufficient_space_rejected() {
        assert!(plan(100 * GIB, true, &[], 150).is_err());
    }
}

#[cfg(test)]
mod edge_tests {
    use super::*;
    fn amdz(start: u64) -> Partition {
        Partition {
            number: 4,
            start,
            sectors: GIB,
            label: "AMDZ".into(),
            part_label: String::new(),
        }
    }
    #[test]
    fn end_amdz_is_outside_new_partitions() {
        let a = amdz(499 * GIB - 2048);
        let p = plan(500 * GIB, true, &[a.clone()], 150).unwrap();
        assert!(p.preserved.contains(&4));
        assert!(p.create.iter().all(|p| p.end < a.start));
    }
    #[test]
    fn duplicate_amdz_rejected() {
        assert!(plan(500 * GIB, true, &[amdz(2048), amdz(499 * GIB - 2048)], 150).is_err());
    }
    #[test]
    fn overlapping_partitions_rejected() {
        let a = amdz(2048);
        let mut b = a.clone();
        b.number = 5;
        b.label.clear();
        assert!(plan(500 * GIB, true, &[a, b], 150).is_err());
    }
    #[test]
    fn legacy_amdz_rejected() {
        assert!(plan(500 * GIB, false, &[amdz(2048)], 150).is_err());
    }
}
