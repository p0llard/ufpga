use regex::Regex;

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct Location {
    pub domain: u8,
    pub bus: u8,
    pub device: u8,
    pub function: u8,
}

impl Location {
    pub fn to_string(&self) -> String {
        format!(
            "{:04}:{:02}:{:02}.{}",
            self.domain, self.bus, self.device, self.function
        )
    }

    pub fn parse_loc(loc: &str) -> Option<Location> {
        let re = Regex::new(r"(\d{4}):(\d{2}):(\d{2}).(\d{1})").ok()?;
        let cap = re.captures(loc)?;

        let domain = cap.get(1)?.as_str().parse::<u8>().ok()?;
        let bus = cap.get(2)?.as_str().parse::<u8>().ok()?;
        let device = cap.get(3)?.as_str().parse::<u8>().ok()?;
        let function = cap.get(4)?.as_str().parse::<u8>().ok()?;

        Some(Location {
            domain,
            bus,
            device,
            function,
        })
    }
}
