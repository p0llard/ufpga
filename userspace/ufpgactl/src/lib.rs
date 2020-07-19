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
                println!("{}", ufpga.status(verbose));
            }
        }
        Err(e) => {
            eprintln!("Failed to enumerate uFPGA devices: {}", e);
            process::exit(1);
        }
    }
}

pub fn enumerate_devices(class: &str) -> Result<Vec<ufpga::UFPGA>, SysfsLookupError> {
    let mut out = Vec::new();

    // TODO: Tidy this up
    for entry in sysfs::read_class(class)? {
        if let Ok(entry) = entry {
            let path = entry.path();
            if let Ok(device) = sysfs::Device::read_device(&path) {
                out.push(ufpga::UFPGA { device })
            }
        }
    }

    Ok(out)
}
