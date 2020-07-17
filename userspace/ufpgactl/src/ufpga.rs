use crate::pci::Location;
use crate::sysfs::Device;

pub struct UFPGA {
    pub device: Device,
    pub location: Location,
}
