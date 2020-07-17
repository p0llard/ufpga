use std::fs::{read_dir, ReadDir};
use std::io::{Error, ErrorKind, Result as IOResult};
use std::path::PathBuf;

use crate::pci::Location;

/// Currently a simple wrapper around a string representing the mount point
pub struct Device {
    pub mount: String,
}

impl Device {
    pub fn from_str(mount: &str) -> Device {
        Device {
            mount: String::from(mount),
        }
    }
}

/// Helper to find all devices associated with a given kernel class
pub fn read_class(class: &str) -> IOResult<ReadDir> {
    let mut path = PathBuf::from(r"/sys/class/");
    path.push(class);
    read_dir(path)
}

/// Helper to determine the PCI location of an attached device
pub fn read_dev_location(mut dev: PathBuf) -> IOResult<Location> {
    dev.push("device");

    let link = dev.read_link()?;
    let loc = link
        .file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| Error::new(ErrorKind::InvalidData, "Invalid sysfs device link"))?;

    Location::parse_loc(loc).ok_or_else(|| {
        Error::new(
            ErrorKind::InvalidData,
            "sysfs device not attached to PCI bus",
        )
    })
}

pub fn read_dev_mount(dev: &PathBuf) -> IOResult<Device> {
    let link = dev.read_link()?;
    let loc = link
        .file_name()
        .and_then(|s| s.to_str())
        .ok_or_else(|| Error::new(ErrorKind::InvalidData, "Invalid sysfs device link"))?;

    Ok(Device::from_str(loc))
}
