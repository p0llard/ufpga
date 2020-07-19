use std::{
    error::Error,
    fmt::Display,
    fs::{read_dir, File, ReadDir},
    io::Read,
    os::linux::fs::MetadataExt,
    path::{Path, PathBuf},
};

use crate::pci::Location;
use regex::Regex;

/// Currently a simple wrapper around a string representing the mount point
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Device {
    pub name: String,
    pub mount: PathBuf,
    pub location: Location,
}

impl Device {
    pub fn read_device(dev: &Path) -> Result<Device, SysfsLookupError> {
        let location = read_dev_location(dev)?;
        let mount = read_dev_mount(dev)?;
        let name = read_file_name(dev)?;

        Ok(Device {
            name,
            mount,
            location,
        })
    }
}

/// Helper to find all devices associated with a given kernel class
pub fn read_class(class: &str) -> Result<ReadDir, SysfsLookupError> {
    let mut path = PathBuf::from(r"/sys/class/");
    path.push(class);
    read_dir(path).map_err(|_| SysfsLookupError(()))
}

/// Helper to determine the PCI location of an attached device
fn read_dev_location(dev: &Path) -> Result<Location, SysfsLookupError> {
    let mut path = PathBuf::from(dev);
    path.push("device");

    let link = path.read_link().map_err(|_| SysfsLookupError(()))?;
    let loc = link
        .file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| SysfsLookupError(()))?;

    loc.parse::<Location>().map_err(|_| SysfsLookupError(()))
}

fn read_dev_mount(dev: &Path) -> Result<PathBuf, SysfsLookupError> {
    // Unfortunately we can't just look at the device name, because udev (or a
    // user) might have changed the device node; instead we read the device
    // number and then scan /dev to find the first node associated with this.
    // This is OK since the driver treats all nodes with the same device number
    // the same.
    let mut path = PathBuf::from(dev);
    path.push("dev");

    let mut file = File::open(path).map_err(|_| SysfsLookupError(()))?;
    let mut buf = String::new();
    file.read_to_string(&mut buf)
        .map_err(|_| SysfsLookupError(()))?;

    let rdev = parse_rdev(&buf).ok_or_else(|| SysfsLookupError(()))?;

    let dev = PathBuf::from(r"/dev/");
    for entry in read_dir(dev).map_err(|_| SysfsLookupError(()))? {
        if let Ok(entry) = entry {
            if let Ok(metadata) = entry.metadata() {
                if rdev == metadata.st_rdev() {
                    return Ok(entry.path());
                }
            }
        }
    }

    // Fallthrough case
    Err(SysfsLookupError(()))
}

fn parse_rdev(dev: &str) -> Option<u64> {
    let re = Regex::new(r"(\d+):(\d+)").ok()?;
    let cap = re.captures(dev)?;

    let major = cap.get(1).and_then(|s| s.as_str().parse::<u8>().ok())?;
    let minor = cap.get(2).and_then(|s| s.as_str().parse::<u8>().ok())?;

    Some(((major as u64) << 8) + (minor as u64))
}

/// Helper to read a file name and return it as a new String
fn read_file_name(dev: &Path) -> Result<String, SysfsLookupError> {
    dev.file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| SysfsLookupError(()))
        .map(|s| String::from(s))
}

// TODO: Flesh this out a bit rather than having a single generic error
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SysfsLookupError(());

impl Display for SysfsLookupError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("sysfs lookup failed")
    }
}

impl Error for SysfsLookupError {}
