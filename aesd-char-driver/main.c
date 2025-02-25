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
#include "aesdchar.h"
#include "aesd-circular-buffer.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
int aesd_init_module(void);
void aesd_cleanup_module(void);

MODULE_AUTHOR("comerts"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    
    struct aesd_dev *dev;
    
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    
    filp->private_data = dev;
    
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */

    PDEBUG("release");

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle read
     */

    struct aesd_dev *dev = filp->private_data;

    size_t entry_offset_byte = 0;
    size_t entry_size = 0;
    size_t bytes_to_copy = 0;
    
    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("mutex_lock_interruptible failed");
        return -ERESTARTSYS;
    }

    struct aesd_buffer_entry *entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->circular_buffer, *(size_t*)f_pos, &entry_offset_byte);
    if (entry == NULL)
    {
        PDEBUG("entry is NULL");
        retval = 0;
        goto out;
    }

    entry_size = entry->size;
    if (entry_size == 0)
    {
        PDEBUG("entry_size is 0");
        retval = 0;
        goto out;
    }
    
    PDEBUG("entry_size: %zu", entry_size);
    PDEBUG("entry_offset_byte: %zu", entry_offset_byte);

    bytes_to_copy = entry_size - entry_offset_byte;
    if (bytes_to_copy == 0)
    {
        PDEBUG("bytes_to_copy is 0");
        retval = 0;
        goto out;
    }

    PDEBUG("bytes_to_copy: %zu", bytes_to_copy);

    if (bytes_to_copy > count)
    {
        bytes_to_copy = count;
    }

    if (bytes_to_copy > 0)
    {
        PDEBUG("copying %s to user", &entry->buffptr[entry_offset_byte]);

        if (copy_to_user(buf, &entry->buffptr[entry_offset_byte], bytes_to_copy) != 0)
        {
            PDEBUG("copy_to_user failed");
            retval = -EFAULT;
            goto out;
        }

        *f_pos += bytes_to_copy;
    }

    retval = bytes_to_copy;

    PDEBUG("copied %zu bytes", retval);

out:
    mutex_unlock(&dev->lock);
    
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    struct aesd_dev *dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("mutex_lock_interruptible failed");
        return -ERESTARTSYS;
    }

    if (count >= AESDCHAR_MAX_WRITE_SIZE)
    {
        retval = -EINVAL;
        goto out;
    }

    if (dev->write_buffer_index + count >= AESDCHAR_MAX_WRITE_SIZE)
    {
        retval = -EINVAL;
        goto out;
    }

    if (copy_from_user(&dev->write_buffer[dev->write_buffer_index], buf, count) != 0)
    {
        retval = -EFAULT;
        goto out;
    }

    PDEBUG("dev->write_buffer 2: %s", dev->write_buffer);

    if (dev->write_buffer[dev->write_buffer_index + count - 1] == '\n')
    {
        dev->write_buffer[dev->write_buffer_index + count] = '\0';

        struct aesd_buffer_entry add_entry;
        add_entry.size = count;
        add_entry.buffptr = kmalloc(count, GFP_KERNEL);
        if (add_entry.buffptr == NULL)
        {
            retval = -ENOMEM;
            goto out;
        }

        PDEBUG("dev->write_buffer 3: %s", dev->write_buffer);

        const char *overwritten_entry = aesd_circular_buffer_add_entry(dev->circular_buffer, &add_entry);
        if (overwritten_entry != NULL)
        {
            kfree(overwritten_entry);
            PDEBUG("Overwritten entry");
        }

        dev->write_buffer_index = 0;
    }
    else
    {
        dev->write_buffer_index += count;
        PDEBUG("dev->write_buffer_index 2: %d", dev->write_buffer_index);
    }

    retval = count;

out:
    mutex_unlock(&dev->lock);

    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
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
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
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

    PDEBUG("Initializing aesd_device");

    PDEBUG("Initializing circular_buffer");

    aesd_device.circular_buffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    if (aesd_device.circular_buffer == NULL)
    {
        PDEBUG("circular_buffer kmalloc failed");
        aesd_cleanup_module();
        return -ENOMEM;
    }

    PDEBUG("Initializing write_buffer");

    aesd_device.write_buffer = kmalloc(AESDCHAR_MAX_WRITE_SIZE, GFP_KERNEL);
    if (aesd_device.write_buffer == NULL)
    {
        PDEBUG("write_buffer kmalloc failed");
        aesd_cleanup_module();
        return -ENOMEM;
    }

    aesd_circular_buffer_init(aesd_device.circular_buffer);

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
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    PDEBUG("Cleaning up aesd_device");

    if (aesd_device.circular_buffer != NULL)
    {
        PDEBUG("Free circular buffer");

        for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
        {
            if (aesd_device.circular_buffer->entry[i].buffptr != NULL)
            {
                kfree(aesd_device.circular_buffer->entry[i].buffptr);
            }
        }

        if (aesd_device.circular_buffer != NULL)
        {
            kfree(aesd_device.circular_buffer);
        }

        PDEBUG("Free write buffer");
        if (aesd_device.write_buffer != NULL)
        {
            kfree(aesd_device.write_buffer);
        }
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
