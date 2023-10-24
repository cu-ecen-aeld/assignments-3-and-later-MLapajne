/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "aesd" device driver, found in
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
#include <linux/slab.h>  // Include this header for kmalloc
#include "aesdchar.h"




/*
 * Our parameters which can be set at load time.
 */

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

module_param(aesd_major, int, S_IRUGO);
module_param(aesd_minor, int, S_IRUGO);




MODULE_AUTHOR("Matevz Lapajne"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");


struct aesd_dev aesd_device;



int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
	struct aesd_dev *dev; /* device information */

	// Get the device structure from the inode private data
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

    // Store the device structure in the file private data
    filp->private_data = dev;

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


loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    uint8_t index;
    struct aesd_dev *dev = filp->private_data;
    loff_t retval;
    loff_t buffer_size = 0;

    if (mutex_lock_interruptible(&(dev->lock)))
        return -ERESTARTSYS;

    switch (whence) {

    // Use specified offset as file position
    case SEEK_SET: 
        retval = off;
        PDEBUG("MAIN.c: SEEK_SET set the offset to %lld\n", retval);
        break;

    // Increment or decrement file position
    case SEEK_CUR: 
        retval = filp->f_pos + off;
        PDEBUG("MAIN.c: SEEK_CUR set the offset to %lld\n", retval);
        break;

    // Use EOF as file position
    case SEEK_END: 
        for (index = 0; index < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
             index++) {
            if (dev->circbuf.entry[index].buffptr) {
                buffer_size += dev->circbuf.entry[index].size;
            }
        }
        retval = buffer_size + off;
        PDEBUG("MAIN.c: SEEK_END set the offset to %lld\n", retval);
        break;

    default:
        retval = -EINVAL;
        mutex_unlock(&(dev->lock));
        return retval;
    }

    if (retval < 0) {
        PDEBUG("MAIN.c: Invalid arguments. Offset cannot be set to %lld\n", retval);
        retval = -EINVAL;
        mutex_unlock(&(dev->lock));
        return retval;
    }

    filp->f_pos = retval;
    mutex_unlock(&(dev->lock));
    return retval;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle read
     */
	ssize_t retval = 0;
    struct aesd_dev *s_dev_p = filp->private_data;

	if (mutex_lock_interruptible(&s_dev_p->lock))
		return -ERESTARTSYS;

    /* Find the circular buffer entry that corresponds to the specified file position */
    size_t entry_offset_byte;
    struct aesd_buffer_entry *entry = aesd_circular_buffer_find_entry_offset_for_fpos(&s_dev_p->circbuf, *f_pos, &entry_offset_byte);
    if (entry == NULL) {
        /* Handle error */
        retval = -EFAULT;
        goto out;
    }
    /* Calculate the number of bytes to read */
    size_t bytes_to_read = entry->size - entry_offset_byte; // entry_offset_byte - number of bytes that have already been read from the circular buffer entry
    if (bytes_to_read > count) {
        bytes_to_read = count;
    }
	
	if (copy_to_user(buf, &entry->buffptr[entry_offset_byte], bytes_to_read)) { 
		retval = -EFAULT;
		goto out;
	}
	/* Update the file position */
    *f_pos += bytes_to_read;
    retval = bytes_to_read;

  out:
	mutex_unlock(&s_dev_p->lock);
	return retval;
}

/*
loff_t aesd_llseek(struct file *filp, loff_t offset, int whence) {
    struct aesd_dev *dev = filp->private_data;
    loff_t retval = -EINVAL;
    PDEBUG("Attempting to adjust offset by: %lld", offset);

    mutex_lock(&dev->lock);
    retval = fixed_size_llseek(filp, offset, whence, dev->circbuf.size);
    mutex_unlock(&dev->lock);

    return retval;
}*/

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    ssize_t retval = -ENOMEM;
    char *data = NULL;
    struct aesd_dev *s_dev_p = filp->private_data;
    struct command_buffer *s_cmd_p = &(s_dev_p->cmd);

    if (mutex_lock_interruptible(&s_dev_p->lock))
        retval = -ERESTARTSYS;
        goto out;

 
    
    /* Copy the write command from user space */
    if (copy_from_user(&s_cmd_p->buf[s_cmd_p->size], buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    s_cmd_p->size += count;

    /* Check if data has newline character */
    if (s_cmd_p->buf[s_cmd_p->size - 1] != '\n') {
        /* Append data to aesd_device.cmd.buf */
        retval = count;
        *f_pos =+ count;
        goto out;
    } else {
        // Write to circular buffer 
        char *buffptr;
        buffptr = kmalloc(s_cmd_p->size, GFP_KERNEL);
        if (!buffptr) {
            retval = -ENOMEM;
            goto out;
        }
        memcpy(buffptr, s_cmd_p->buf, s_cmd_p->size);
        struct aesd_buffer_entry entry;
        entry.buffptr = buffptr;
        entry.size = s_cmd_p->size;
        aesd_circular_buffer_add_entry(&s_dev_p->circbuf, &entry);
        s_cmd_p->size = 0;
    }
    
    retval = count;
    *f_pos += count;
  out:
    if (data)
        kfree(data);
	mutex_unlock(&s_dev_p->lock);
	return retval;

}
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
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
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.circbuf);
    aesd_device.cmd.size = 0;
    mutex_init(&aesd_device.lock);


    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{   
    uint8_t index;
    struct aesd_buffer_entry *entryptr;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &aesd_device.circbuf, index) {
        if (entryptr->buffptr) {
            PDEBUG("MAIN.c: Cleanup module releasing CMD @ index %d @ address %p", index, entryptr->buffptr);
            kfree(entryptr->buffptr);
        }
    }

    /* Get rid of our char dev entries */
    //kfree(aesd_device);

    unregister_chrdev_region(devno, 1);
}





module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
