#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/uaccess.h>

#include "ufpga.h"

static struct ufpga_driver _driver;

static int ufpga_open(struct inode *inode, struct file *filp)
{
    struct ufpga_dev *dev;

    // Find ufpga_dev for other access methods
    dev = container_of(inode->i_cdev, struct ufpga_dev, cdev);
    filp->private_data = dev;

    return 0;
}

static ssize_t ufpga_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    ssize_t ret;
    u32 kbuf;
    void __iomem *mmio;
    struct ufpga_dev *dev;

    // Find mmio region for this device
    dev = filp->private_data;
    mmio = dev->mmio;

    if (*off % 4 || len == 0) {
        ret = 0;
    }

    else {
        kbuf = ioread32(mmio + *off);
        if (copy_to_user(buf, (void *)&kbuf, 4)) {
            ret = -EFAULT;
        }

        else {
            ret = 4;
            (*off)++;
        }
    }

    return ret;
}

static ssize_t ufpga_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    ssize_t ret;
    u32 kbuf;
    void __iomem *mmio;
    struct ufpga_dev *dev;

    // Find mmio region for this device
    dev = filp->private_data;
    mmio = dev->mmio;

    ret = len;
    if (!(*off % 4)) {
        if (copy_from_user((void *)&kbuf, buf, 4) || len != 4) {
            ret = -EFAULT;
        } else {
            pr_info("write: %x", kbuf);
            iowrite32(kbuf, mmio + *off);
        }
    }
    return ret;
}

static loff_t ufpga_llseek(struct file *filp, loff_t off, int whence)
{
    filp->f_pos = off;
    return off;
}

static struct file_operations ufpga_fops = {
    .owner   = THIS_MODULE,
    .open    = ufpga_open,
    .llseek  = ufpga_llseek,
    .read    = ufpga_read,
    .write   = ufpga_write,
};

// FIXME: This may have concurrency issues if (unlikely) two devices are
// probed simultaneously.
static void ufpga_setup_cdev(struct ufpga_dev *dev, unsigned int index)
{
    int err;
    dev_t devno = MKDEV(_driver.major, index);
    struct device *device;

    cdev_init(&dev->cdev, &ufpga_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &ufpga_fops;

    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        // FIXME: If this fails we need to handle deallocation of the cdev properly
        printk(KERN_NOTICE NAME "Error %d adding cdev %d", err, index);
        return;
    }

    device = device_create(_driver.class, NULL, devno, NULL, "ufpga0"); // FIXME: hardcoded device name
    if (!device) {
        printk(KERN_NOTICE "Error creating device");
        return;
    }
    dev->device = device;
    dev->dev = devno;
    dev->active = true;
}

static void ufpga_destroy_dev(struct ufpga_dev *dev)
{
    if (!dev->active) {
        return;
    }

    device_destroy(_driver.class, dev->dev);
    cdev_del(&(dev->cdev));

    pci_release_region(dev->pdev, BAR);
    pci_disable_device(dev->pdev);

    dev->active = false;
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    unsigned int index = _driver.next_dev;
    struct ufpga_dev *udev = _driver.devs + index;

    pci_set_drvdata(dev, udev);
    udev->pdev = dev;

    if (pci_enable_device(dev) < 0) {
        dev_err(&(dev->dev), "pci_enable_device\n");
        goto error;
    }

    if (pci_request_region(dev, BAR, NAME "_BAR0")) {
        dev_err(&(dev->dev), "pci_request_region\n");
        goto error;
    }

    udev->mmio = pci_iomap(dev, BAR, pci_resource_len(dev, BAR));
    ufpga_setup_cdev(udev, index);

    return 0;

error:
    return 1;
}

static void pci_remove(struct pci_dev *dev)
{
    ufpga_destroy_dev(pci_get_drvdata(dev));
}

static struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(XILINX_VENDOR_ID, FPGA_DEVICE_ID), },
    { 0, }
};

static struct pci_driver pci_driver = {
    .name     = NAME,
    .id_table = pci_ids,
    .probe    = pci_probe,
    .remove   = pci_remove,
};

static int ufpga_init(void)
{
    int result;
    dev_t dev;
   
    // Create a device class
    _driver.class = class_create(THIS_MODULE, NAME);
    if (!_driver.class){
        printk(KERN_WARNING NAME ": can't create class");

        result = -ENOMEM; // TODO: Is there a better error code?
        goto err_return;
    }

    // Allocate numbers
    result = alloc_chrdev_region(&dev, 0, MAX_DEVICES, NAME);
    if (result < 0) {
        printk(KERN_WARNING NAME ": can't allocate device numbers");
        goto err_deallocate_class;
    }
    _driver.major = MAJOR(dev);

    // Allocate devices
    _driver.devs = kmalloc(MAX_DEVICES * sizeof(struct ufpga_dev), GFP_KERNEL);
    if (!_driver.devs) {
        result = -ENOMEM;
        goto err_deallocate_chrdev_region;  /* Make this more graceful */
    }
    memset(_driver.devs, 0, MAX_DEVICES * sizeof(struct ufpga_dev));
    _driver.next_dev = 0;

    result = pci_register_driver(&pci_driver);
    if (result < 0) {
        printk(KERN_WARNING NAME ": can't register PCI driver");
        goto err_deallocate_devices;
    }

    return 0;

err_deallocate_devices:
    kfree(_driver.devs);
err_deallocate_chrdev_region:
    unregister_chrdev_region(dev, MAX_DEVICES);
err_deallocate_class:
    class_destroy(_driver.class);
err_return:
    return result;
}

static void ufpga_exit(void)
{
    int i;

    pci_unregister_driver(&pci_driver);

    for (i = 0; i < MAX_DEVICES; i++) {
        ufpga_destroy_dev(_driver.devs + i);
    }

    kfree(_driver.devs);
    unregister_chrdev_region(MKDEV(_driver.major, 0), MAX_DEVICES);
    class_destroy(_driver.class);
}

module_init(ufpga_init);
module_exit(ufpga_exit);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, pci_ids);
