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

static struct ufpga_driver _driver = {
.major = 0,
.count = 0,
.class = 0,
.devs = LIST_HEAD_INIT(_driver.devs)
};

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

static inline struct ufpga_dev* alloc_udev(void)
{
   struct ufpga_dev *udev;

   udev = kmalloc(sizeof(struct ufpga_dev), GFP_KERNEL);

   // TODO: Do we really need to do this?
   memset(udev, 0, sizeof(struct ufpga_dev));

   // Set up list pointer
   INIT_LIST_HEAD(&(udev->devs));

   return udev;
}

// FIXME: Potential concurrency issue here
static inline int get_devno(dev_t *devno)
{
    int result;
   
    if (!_driver.major) {
        // We don't have a major number; need to alloc one
        result = alloc_chrdev_region(devno, 0, 1, NAME);
        _driver.major = MAJOR(*devno);
    }

    else {
        // Use existing major number
        *devno = MKDEV(_driver.major, _driver.count);
        result = register_chrdev_region(*devno, 1, NAME);
    }

    if (!(result < 0)) {
        _driver.count++; // Increment number of attached devices
    }
       
    return result;
}

static inline int alloc_cdev(struct ufpga_dev *udev)
{
    int err;
   
    cdev_init(&udev->cdev, &ufpga_fops);
    udev->cdev.owner = THIS_MODULE;
    udev->cdev.ops = &ufpga_fops;

    err = cdev_add(&udev->cdev, udev->devno, 1);
    if (err)
    {
        // FIXME: If this fails we need to handle deallocation of the cdev properly
        //printk(KERN_NOTICE NAME "Error %d adding cdev %d", err, index);
        return -1;
    }

    udev->device = device_create(_driver.class, NULL, udev->devno, NULL, "ufpga0"); // FIXME: hardcoded device name
    if (!udev->device) {
        printk(KERN_NOTICE "Error creating device");
        return -1;
    }

    return 0;
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    // Allocate uFPGA device
    struct ufpga_dev *udev = alloc_udev();

    // Link PCIe dev and udev
    pci_set_drvdata(dev, udev);
    udev->pdev = dev;

    if (pci_enable_device(dev) < 0) {
        dev_err(&dev->dev, "pci_enable_device\n");
        goto error;
    }

    if (pci_request_region(dev, BAR, NAME "_BAR0")) {
        dev_err(&dev->dev, "pci_request_region\n");
        goto error;
    }

    udev->mmio = pci_iomap(dev, BAR, pci_resource_len(dev, BAR));

    // Get numbers for the new device
    // FIXME: Error handling on this call
    get_devno(&udev->devno);

    // Allocate cdev
    // FIXME: Error handling on this call
    alloc_cdev(udev);

    list_add(&udev->devs, &_driver.devs);

    return 0;

error:
    return 1;
}

// FIXME: Potential concurrency issue here? Interaction with probe?
static void ufpga_destroy_dev(struct ufpga_dev *udev)
{
    list_del(&udev->devs);
    _driver.count--;

    device_destroy(_driver.class, udev->devno);

    cdev_del(&udev->cdev);
    unregister_chrdev_region(udev->devno, 1);

    pci_release_region(udev->pdev, BAR);
    pci_disable_device(udev->pdev);

    kfree(udev);
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

    // Create a device class
    _driver.class = class_create(THIS_MODULE, NAME);
    if (!_driver.class){
        printk(KERN_WARNING NAME ": can't create class");

        result = -ENOMEM; // TODO: Is there a better error code?
        goto err_return;
    }

    result = pci_register_driver(&pci_driver);
    if (result < 0) {
        printk(KERN_WARNING NAME ": can't register PCI driver");
        goto err_deallocate_class;
    }

    return 0;

err_deallocate_class:
    class_destroy(_driver.class);
err_return:
    return result;
}

static void ufpga_exit(void)
{
    struct ufpga_dev *udev = NULL ;
    struct ufpga_dev *udev_temp = NULL ;

    // NOTE: This *must* come first, otherwise we try to double free
    pci_unregister_driver(&pci_driver);

    // Catch any remenants in case a device didn't remove.
    list_for_each_entry_safe (udev, udev_temp, &_driver.devs, devs)
    {
        ufpga_destroy_dev(udev);
    }

    class_destroy(_driver.class);
}

module_init(ufpga_init);
module_exit(ufpga_exit);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, pci_ids);
