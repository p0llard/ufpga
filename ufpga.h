#ifndef __UFPGA_H_
#define __UFPGA_H_

#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/types.h>

#define NAME "ufpga" // Driver name used for *everything*

#define MAX_DEVICES 1 // Maximum number of attached devices

// PCIe Config
#define BAR 0
#define XILINX_VENDOR_ID 0x10ee // TODO: Stop using Xilinx's vendor ID
#define FPGA_DEVICE_ID 0x7021 // TODO: Stop using this random device ID

struct ufpga_dev {
    bool active; // HACK: We shouldn't waste space with useless structs.

    void __iomem *mmio;

    dev_t dev;

    struct device *device;
    struct pci_dev *pdev;
    struct cdev cdev;
};

struct ufpga_driver {
    unsigned int major; // Character device major number

    unsigned int next_dev;

    struct ufpga_dev *devs;

    struct class *class;
};

#endif // __UFPGA_H_
