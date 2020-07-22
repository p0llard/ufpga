# uFPGA

uFPGA aims to be simple framework for communicating with FPGAs over PCIe; it is
in its infancy, but currently it consists of a simple Linux kernel module to map
PCIe BARs into kernel address space enabling memory-mapped registers to be
accessed through a device file, and a simple userspace utility (`ufpgactl`) for
managing attached devices.

## Planned Functionality

* Generic shell hardware designs with user configurable regions to easily use
  the UFPGA software components in user designs;
* FPGA configuration via PCIe;
* Interactive userspace tool for manipulating device registers in real-time;
* Kernel interfaces for automatic hardware function detection.

The overall aim is to produce an open-source ecosystem for PCIe enabled FPGA
designs allowing hobbyists to produce their own high performance FPGA
accelerator designs.
