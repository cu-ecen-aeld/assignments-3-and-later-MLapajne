/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"

#define BUF_SIZE  (1024 * 1024)

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("hyunjin-chung-woowahan"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static long aesd_adjust_file_offset(struct file* filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    long retval = -EINVAL;
    unsigned int entry_index;
    struct aesd_dev *pdev = filp->private_data;

    if (write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        return -EINVAL;

    entry_index = pdev->circular_buf.out_offs + write_cmd;
    if (entry_index >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        entry_index -= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (mutex_lock_interruptible(&pdev->lock))
        return -ERESTARTSYS;

    if (!pdev->circular_buf.entry[entry_index].buffptr)
        goto out;

    if (write_cmd_offset >= pdev->circular_buf.entry[entry_index].size)
        goto out;

    retval = aesd_circular_buffer_find_offset_for_entry_index(&pdev->circular_buf,
        entry_index) + write_cmd_offset;

    PDEBUG("write_cmd %u with offset %u: total_offset %lu",write_cmd,
        write_cmd_offset, retval);

    filp->f_pos = retval;

out:
    mutex_unlock(&pdev->lock);
    return retval;
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *pdev = filp->private_data;
    struct aesd_buffer_entry *pbuf = NULL;
    size_t offset_in_buf = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    if (pdev == NULL) {
        retval = -EFAULT;
        goto out;
    }

    if (mutex_lock_interruptible(&pdev->lock))
        return -ERESTARTSYS;

    
    size_t copy_size;
    pbuf =
        aesd_circular_buffer_find_entry_offset_for_fpos(&pdev->circular_buf, *f_pos, &offset_in_buf);

    if (!pbuf)
        goto out;

    size_t kernel_data_size = pbuf->size - offset_in_buf;
    if (kernel_data_size > count)
        kernel_data_size = count;


    if (copy_to_user(buf, pbuf->buffptr + offset_in_buf, kernel_data_size))
    {
        retval = -EFAULT;
        goto out;
    }

    retval = kernel_data_size;
    *f_pos += kernel_data_size;

out:
    mutex_unlock(&pdev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    char* ker_buf;
    ssize_t retval = -ENOMEM;
    struct aesd_dev *pdev = filp->private_data;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    if (mutex_lock_interruptible(&pdev->lock))
        return -ERESTARTSYS;


    if (!pdev->cur_buf.buffptr) 
    {
        pdev->cur_buf.buffptr = kmalloc(BUF_SIZE, GFP_KERNEL);
        if (!pdev->cur_buf.buffptr){
            goto out;
        }
        //memset(aesd_device.cur_buf.buffptr, 0, BUF_SIZE);
    }

    if ((pdev->cur_buf.size + count) > BUF_SIZE)
        goto out;

    ker_buf = (char *)pdev->cur_buf.buffptr + pdev->cur_buf.size;

    if (copy_from_user(ker_buf, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    pdev->cur_buf.size += count;
    *f_pos += count;

    if (ker_buf[count - 1] == '\n') {
        if (pdev->circular_buf.entry[pdev->circular_buf.in_offs].buffptr)
            kfree(pdev->circular_buf.entry[pdev->circular_buf.in_offs].buffptr);
        pdev->cur_buf.buffptr = ker_buf;
        aesd_circular_buffer_add_entry(&pdev->circular_buf, &pdev->cur_buf);
        pdev->cur_buf.buffptr = NULL;
        pdev->cur_buf.size = 0;
        PDEBUG("add entry");
    }

    retval = count;

out:
    mutex_unlock(&pdev>lock);
    return retval;
}

loff_t aesd_llseek (struct file *filp, loff_t offset, int whence)
{
    PDEBUG("llseek");
    loff_t buffer_size = 0;
    struct aesd_dev *dev = filp->private_data;
    uint8_t index;
    loff_t retval;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;


    //retval = fixed_size_llseek(filp, offset, whence, aesd_device.circular_buf.size);

    switch (whence) {

    // Use specified offset as file position
    case SEEK_SET: 
        retval = offset;
        PDEBUG("MAIN.c: SEEK_SET set the offset to %lld\n", retval);
        break;

    // Increment or decrement file position
    case SEEK_CUR: 
        retval = filp->f_pos + offset;
        PDEBUG("MAIN.c: SEEK_CUR set the offset to %lld\n", retval);
        break;

    // Use EOF as file position
    case SEEK_END: 
        for (index = 0; index < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
             index++) {
            if (dev->circular_buf.entry[index].buffptr) { 
                buffer_size += dev->circular_buf.entry[index].size;
            }
        }
        retval = buffer_size + offset;
        PDEBUG("MAIN.c: SEEK_END set the offset to %lld\n", retval);
        break;

    default:
        retval = -EINVAL;
        goto out;
    }

    if (retval < 0) {
        PDEBUG("MAIN.c: Invalid arguments. Offset cannot be set to %lld\n", retval);
        retval = -EINVAL;
        goto out;
    }

   

    PDEBUG("llseek");
    filp->f_pos = retval;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval = -EFAULT;
    switch (cmd)
    {
        case AESDCHAR_IOCSEEKTO:
        {
            struct aesd_seekto seekto;
            if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0)
                retval = -EFAULT;
            else
                retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
            break;
        }
        default:
            retval = -EFAULT;
    }

    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.circular_buf);

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);
    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.circular_buf, index) {
        if (entry->buffptr)
            kfree(entry->buffptr);
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
