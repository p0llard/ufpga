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

   if (udev) {
       // TODO: Do we really need to do this?
       memset(udev, 0, sizeof(struct ufpga_dev));

       // Set up list pointer
       INIT_LIST_HEAD(&(udev->devs));
   }

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

// TODO: Error recovery is potentially dodgy here wrt. when cdev_del
// should be used; after cdev_init, or only after cdev_add?
static inline int init_cdev(struct ufpga_dev *udev)
{
    int result;
   
    if(get_devno(&udev->devno)) {
        printk(KERN_ERR NAME ": failed to allocate device number\n");
        result = -ENOMEM; // TODO: Is there a better error code?
        goto err_return;
    }
   
    cdev_init(&udev->cdev, &ufpga_fops);
    udev->cdev.owner = THIS_MODULE;
    udev->cdev.ops = &ufpga_fops;

    if(cdev_add(&udev->cdev, udev->devno, 1)) {
        printk(KERN_ERR NAME ": failed to add character device\n");
        result = -1;
        goto err_delete_cdev;
    }

    // NOTE: From this point onwards the character device is live; we can no longer
    // handle errors safely and must continue.

    printk(KERN_NOTICE NAME ": created character device %d:%d\n", MAJOR(udev->devno), MINOR(udev->devno));

    udev->device = device_create(_driver.class, NULL, udev->devno, NULL, "ufpga0"); // FIXME: hardcoded device name
    if (!udev->device) {
        printk(KERN_WARNING NAME ": failed to create device\n");
    }

    return 0;

err_delete_cdev:
    cdev_del(&udev->cdev);
//err_deallocate_number:
    unregister_chrdev_region(udev->devno, 1);
err_return:
    return result;
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    int result;
    struct ufpga_dev *udev;

    dev_info(&dev->dev, "attaching device\n");

    // Allocate uFPGA device
    udev = alloc_udev();
    if (!udev) {
        printk(KERN_ERR "Error allocating uFPGA device\n");
        result = -ENOMEM;
        goto err;
    }

    // Link PCIe dev and uFPGA device
    pci_set_drvdata(dev, udev);
    udev->pdev = dev;

    result = pci_enable_device(dev);
    if (result) {
        dev_err(&dev->dev, "failed to enable device\n");
        goto err_free;
    }

    result = pci_request_region(dev, BAR, NAME "_BAR0");
    if (result) {
        dev_err(&dev->dev, "failed to request BAR\n");
        goto err_disable;
    }

    udev->mmio = pci_iomap(dev, BAR, pci_resource_len(dev, BAR));

    // Allocate cdev
    result = init_cdev(udev);
    if(result) {
        dev_err(&dev->dev, "failed to initialise character device\n");
        goto err_unlock_region;
    }

    // NOTE: From this point onwards the character device is live; we can no longer
    // handle errors safely and must continue.

    list_add(&udev->devs, &_driver.devs);

    return 0;

err_unlock_region:
    pci_release_region(udev->pdev, BAR);
err_disable:
    pci_disable_device(udev->pdev);
err_free:
    printk(KERN_NOTICE "Deallocating uFPGA device and aborting\n");
    kfree(udev);
err:
    return result;
}

// FIXME: Potential concurrency issue here? Interaction with probe?
static void ufpga_destroy_dev(struct ufpga_dev *udev)
{
    list_del(&udev->devs);
    _driver.count--;

    // This is always safe; even if creation failed
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

    printk(KERN_INFO NAME ": loading module...\n");

    // Create a device class
    _driver.class = class_create(THIS_MODULE, NAME);
    if (!_driver.class){
        printk(KERN_ERR NAME ": can't create class\n");

        result = -ENOMEM; // TODO: Is there a better error code?
        goto err_return;
    }

    result = pci_register_driver(&pci_driver);
    if (result < 0) {
        printk(KERN_ERR NAME ": can't register PCI driver\n");
        goto err_deallocate_class;
    }

    // NOTE: From this point onwards the PCI driver is live

    printk(KERN_INFO NAME ": module loaded\n");

    return 0;

err_deallocate_class:
    class_destroy(_driver.class);
err_return:
    printk(KERN_NOTICE NAME ": aborting module load with result: %d\n", result);
    return result;
}

static void ufpga_exit(void)
{
    struct ufpga_dev *udev = NULL ;
    struct ufpga_dev *udev_temp = NULL ;

    // NOTE: This *must* come first, otherwise we try to double free
    pci_unregister_driver(&pci_driver);

    // Catch any remenants in case a device didn't remove.
    list_for_each_entry_safe (udev, udev_temp, &_driver.devs, devs) {
        ufpga_destroy_dev(udev);
    }

    class_destroy(_driver.class);

    printk(KERN_INFO NAME ": module unloaded\n");
}

module_init(ufpga_init);
module_exit(ufpga_exit);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, pci_ids);
