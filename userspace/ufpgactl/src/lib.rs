use std::io::Result as IOResult;
use structopt::StructOpt;

pub mod pci;
pub mod sysfs;
pub mod ufpga;

#[derive(StructOpt)]
#[structopt(name = "ufpgactl", about = "uFPGA management utility")]
pub enum Cli {
    /// List attached uFPGA devices
    List,

    /// Flash the onboard memory of an attached uFPGA device
    Flash,

    /// Manipulate I/O space of an attached uFPGA device
    Poke,

    /// Reconfigure an attached uFPGA device from on-board memory
    Reset,
}

pub fn enumerate_devices(class: &str) -> IOResult<Vec<ufpga::UFPGA>> {
    let mut out = Vec::new();

    for entry in sysfs::read_class(class)? {
        let path = entry?.path();
        let loc = sysfs::read_dev_location(path.clone())?; // TODO: Eliminate clone?
        let mount = sysfs::read_dev_mount(&path)?;
        out.push(ufpga::UFPGA {
            device: mount,
            location: loc,
        })
    }

    Ok(out)
}

fn list() {
    println!("Attached uFPGA Devices");
    println!("======================");
    let ufpgas = enumerate_devices("ufpga").unwrap();
    for ufpga in ufpgas {
        println!(
            "uFPGA device @ {} on /dev/{}",
            ufpga.location.to_string(),
            ufpga.device.mount
        );
    }
}

pub fn run(args: &Cli) {
    match args {
        Cli::List => list(),
        Cli::Flash => panic!("Not implemented!"),
        Cli::Poke => panic!("Not implemented!"),
        Cli::Reset => panic!("Not implemented!"),
    }
}
