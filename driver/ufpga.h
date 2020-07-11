#ifndef __UFPGA_H_
#define __UFPGA_H_

#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/types.h>

#define NAME "ufpga" // Driver name used for *everything*

// PCIe Config
#define BAR 0
#define XILINX_VENDOR_ID 0x10ee // TODO: Stop using Xilinx's vendor ID
#define FPGA_DEVICE_ID 0x7021 // TODO: Stop using this random device ID

struct ufpga_dev {
    void __iomem *mmio; // IO region for this device

    dev_t devno; // Device numbers for this device

    struct device *device; // Device file
    struct pci_dev *pdev; // PCIe device
    struct cdev cdev; // Character device

    struct list_head devs;
};

struct ufpga_driver {
    unsigned int major; // Character device major number
    unsigned int count; // Count of attached devices

    struct class *class; // Device class

    struct list_head devs; // List of uFPGA devices
};

#endif // __UFPGA_H_
