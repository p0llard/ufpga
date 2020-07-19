use std::{
    error::Error,
    fmt::Display,
    fs::{read_dir, File, ReadDir},
    io::Read,
    path::{Path, PathBuf},
    str::FromStr,
};

use crate::pci::Location;

/// Currently a simple wrapper around a string representing the mount point
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Device {
    pub mount: String,
}

impl FromStr for Device {
    type Err = SysfsLookupError;
    fn from_str(loc: &str) -> Result<Self, Self::Err> {
        Ok(Device {
            mount: String::from(loc),
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
pub fn read_dev_location(dev: &Path) -> Result<Location, SysfsLookupError> {
    let mut path = PathBuf::from(dev);
    path.push("device");

    let link = path.read_link().map_err(|_| SysfsLookupError(()))?;
    let loc = link
        .file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| SysfsLookupError(()))?;

    loc.parse::<Location>().map_err(|_| SysfsLookupError(()))
}

pub fn read_dev_mount(dev: &Path) -> Result<Device, SysfsLookupError> {
    // Unfortunately we can't just look at the device name, because udev
    // (or a user) might have changed the device node; instead we read
    // the device number and then scan /dev to find the first node
    // associated with this. This is OK since the driver treats all
    // nodes with the same device number the same.
    panic!("not implemented");
    let mut path = PathBuf::from(dev);
    path.push("dev");

    let mut file = File::open(path).map_err(|_| SysfsLookupError(()))?;
    let mut dev_t = String::new();

    file.read_to_string(&mut dev_t)
        .map_err(|_| SysfsLookupError(()))?;
}

pub fn read_dev_name(dev: &Path) -> Result<Device, SysfsLookupError> {
    let link = dev.read_link().map_err(|_| SysfsLookupError(()))?;
    let loc = link
        .file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| SysfsLookupError(()))?;

    loc.parse::<Device>()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SysfsLookupError(());

impl Display for SysfsLookupError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("sysfs lookup failed")
    }
}

impl Error for SysfsLookupError {}
