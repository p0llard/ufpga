use regex::Regex;
use std::{error::Error, fmt::Display, str::FromStr};

#[derive(Copy, Clone, PartialEq, Eq)]
pub struct Location {
    pub domain: u8,
    pub bus: u8,
    pub device: u8,
    pub function: u8,
}

impl Display for Location {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{:04}:{:02}:{:02}.{}",
            self.domain, self.bus, self.device, self.function
        )
    }
}

impl FromStr for Location {
    type Err = LocationParseError;
    fn from_str(loc: &str) -> Result<Self, Self::Err> {
        let re =
            Regex::new(r"(\d{4}):(\d{2}):(\d{2}).(\d{1})").map_err(|_| LocationParseError(()))?;
        let cap = re.captures(loc).ok_or_else(|| LocationParseError(()))?;

        let domain = cap
            .get(1)
            .ok_or_else(|| LocationParseError(()))?
            .as_str()
            .parse::<u8>()
            .map_err(|_| LocationParseError(()))?;

        let bus = cap
            .get(2)
            .ok_or_else(|| LocationParseError(()))?
            .as_str()
            .parse::<u8>()
            .map_err(|_| LocationParseError(()))?;

        let device = cap
            .get(3)
            .ok_or_else(|| LocationParseError(()))?
            .as_str()
            .parse::<u8>()
            .map_err(|_| LocationParseError(()))?;

        let function = cap
            .get(4)
            .ok_or_else(|| LocationParseError(()))?
            .as_str()
            .parse::<u8>()
            .map_err(|_| LocationParseError(()))?;

        Ok(Location {
            domain,
            bus,
            device,
            function,
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LocationParseError(());

impl Display for LocationParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("invalid PCI location string")
    }
}

impl Error for LocationParseError {}
