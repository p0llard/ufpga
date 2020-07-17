use structopt::StructOpt;

use ufpgactl::{run, Cli};

fn main() {
    let args = Cli::from_args();
    run(&args);
}

#[cfg(not(target_os = "linux"))]
compile_error!("ufpgactl only supports Linux");
