use std::{
    fmt::Display,
    fs::File,
    io::{Error, ErrorKind, Read, Result, Seek, SeekFrom},
    path::PathBuf,
};

use crate::sysfs::Device;

#[derive(Default, Copy, Clone, PartialEq)]
pub struct XADCTriple {
    pub current: f64,
    pub min: f64,
    pub max: f64,
}

#[derive(Default, Copy, Clone, PartialEq)]
pub struct XADCStatus {
    pub temperature: XADCTriple,
    pub vcc_int: XADCTriple,
    pub vcc_aux: XADCTriple,
}

impl Display for XADCStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Temperature: {:.1}°C, Vcc_int: {:.2}V, Vcc_aux: {:.2}V",
            self.temperature.current, self.vcc_int.current, self.vcc_aux.current
        )
    }
}

#[derive(Clone, PartialEq, Eq)]
pub struct UFPGA {
    pub device: Device,
}

impl UFPGA {
    pub fn status(&self, verbose: bool) -> String {
        if verbose {
            match (
                self.device.mount.to_str(),
                self.version(),
                self.xadc_status(),
            ) {
                (Some(mount), Ok(version), Ok(status)) => format!(
                    "uFPGA device ({}) @ {} on {}\n    ID: {}\n    Status: {}",
                    self.device.name, self.device.location, mount, version, status
                ),
                _ => format!("uFPGA device @ {}", self.device.location),
            }
        } else {
            if let Some(mount) = self.device.mount.to_str() {
                format!(
                    "uFPGA device ({}) @ {} on {}",
                    self.device.name, self.device.location, mount
                )
            } else {
                format!(
                    "uFPGA device ({}) @ {}",
                    self.device.name, self.device.location
                )
            }
        }
    }

    pub fn version(&self) -> Result<String> {
        let mut device_path = PathBuf::from("/dev");
        device_path.push(&self.device.mount);

        let mut device_file = File::open(device_path)?;
        device_file.seek(SeekFrom::Start(0x1000))?;

        let mut buf = [0; 4];
        let n = device_file.read(&mut buf[..])?;

        if n == 4 {
            let version_str = std::str::from_utf8(&buf[..])
                .map_err(|_| Error::new(ErrorKind::InvalidData, "invalid version string"))?;
            Ok(String::from(version_str))
        } else {
            Err(Error::new(
                ErrorKind::UnexpectedEof,
                "failed to read XADC data",
            ))
        }
    }

    pub fn xadc_status(&self) -> Result<XADCStatus> {
        let mut device_path = PathBuf::from("/dev");
        device_path.push(&self.device.mount);

        let mut device_file = File::open(device_path)?;
        device_file.seek(SeekFrom::Start(0x3200))?;

        let mut temperature: XADCTriple = Default::default();
        let mut vcc_int: XADCTriple = Default::default();
        let mut vcc_aux: XADCTriple = Default::default();

        enum XADCType {
            Volt,
            Temp,
        };

        let mut data_points = [
            (&mut temperature.current, 0x0, XADCType::Temp),
            (&mut vcc_int.current, 0x4, XADCType::Volt),
            (&mut vcc_aux.current, 0x8, XADCType::Volt),
            (&mut temperature.max, 0x80, XADCType::Temp),
            (&mut vcc_int.max, 0x84, XADCType::Volt),
            (&mut vcc_aux.max, 0x88, XADCType::Volt),
            (&mut temperature.min, 0x90, XADCType::Temp),
            (&mut vcc_int.min, 0x94, XADCType::Volt),
            (&mut vcc_aux.min, 0x98, XADCType::Volt),
        ];

        let mut buf = [0; 4];
        for point in &mut data_points[..] {
            device_file.seek(SeekFrom::Start(0x3200 + point.1))?;
            let n = device_file.read(&mut buf[..])?;

            if n == 4 {
                match point.2 {
                    XADCType::Temp => *point.0 = Self::convert_temp(u32::from_ne_bytes(buf)),
                    XADCType::Volt => *point.0 = Self::convert_volt(u32::from_ne_bytes(buf)),
                }
            } else {
                println!("error: temp");
                return Err(Error::new(
                    ErrorKind::UnexpectedEof,
                    "failed to read XADC temperature data",
                ));
            }
        }

        Ok(XADCStatus {
            temperature,
            vcc_int,
            vcc_aux,
        })
    }

    fn convert_temp(raw: u32) -> f64 {
        ((raw as f64) * 503.975 / 65536.0) - 273.15
    }

    fn convert_volt(raw: u32) -> f64 {
        (raw as f64) * 3.0 / 65536.0
    }
}
