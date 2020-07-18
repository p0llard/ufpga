use std::process;

use structopt::StructOpt;

use sysfs::SysfsLookupError;

pub mod pci;
pub mod sysfs;
pub mod ufpga;

#[derive(StructOpt)]
#[structopt(name = "ufpgactl", about = "uFPGA management utility")]
pub enum Cli {
    /// List attached uFPGA devices
    List {
        #[structopt(short, long)]
        verbose: bool,
    },

    /// Flash the onboard memory of an attached uFPGA device
    Flash,

    /// Manipulate I/O space of an attached uFPGA device
    Poke,

    /// Reconfigure an attached uFPGA device from on-board memory
    Reset,
}

pub fn run(args: &Cli) {
    match args {
        Cli::List { verbose } => list(*verbose),
        Cli::Flash => panic!("Not implemented!"),
        Cli::Poke => panic!("Not implemented!"),
        Cli::Reset => panic!("Not implemented!"),
    }
}

fn list(verbose: bool) {
    match enumerate_devices("ufpga") {
        Ok(ufpgas) => {
            println!("Attached uFPGA Devices");
            println!("======================");
            for ufpga in ufpgas {
                println!("{}", ufpga_status(ufpga, verbose));
            }
        }
        Err(e) => {
            eprintln!("Failed to enumerate uFPGA devices: {}", e);
            process::exit(1);
        }
    }
}

pub fn ufpga_status(ufpga: ufpga::UFPGA, verbose: bool) -> String {
    if verbose {
        match (ufpga.version(), ufpga.xadc_status()) {
            (Ok(version), Ok(status)) => format!(
                "uFPGA device @ {} on /dev/{}\n    ID: {}\n    Status: {}",
                ufpga.location, ufpga.device.mount, version, status
            ),
            _ => ufpga.to_string(),
        }
    } else {
        ufpga.to_string()
    }
}

pub fn enumerate_devices(class: &str) -> Result<Vec<ufpga::UFPGA>, SysfsLookupError> {
    let mut out = Vec::new();

    // TODO: Tidy this up
    for entry in sysfs::read_class(class)? {
        if let Ok(entry) = entry {
            let path = entry.path();

            if let Ok(loc) = sysfs::read_dev_location(&path) {
                if let Ok(mount) = sysfs::read_dev_mount(&path) {
                    out.push(ufpga::UFPGA {
                        device: mount,
                        location: loc,
                    });
                }
            }
        }
    }

    Ok(out)
}
