#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>       /* printk() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/aio.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>

//#define DEBUG_GET_CRT_CPU

#ifdef DEBUG_GET_CRT_CPU
#define PRINTKD(...)            printk (...)
#else
#define PRINTKD(...)
#endif

#define GETCRTCPU_IOC_MAGIC  'y'
#define GETCRTCPU_IOCS_GET_CPU          _IOR (GETCRTCPU_IOC_MAGIC, 0, unsigned char *)

#define COPY_TO_USER(dst,src,size)                                     \
do {                                                                   \
  int cpy;                                                             \
  cpy = copy_to_user(dst,src,size);                                    \
  if (cpy < 0)                                                         \
    {                                                                  \
      PRINTKD (KERN_ALERT "ioctl() invalid arguments in copy_to_user\n");             \
      return cpy;                                                      \
    }                                                                  \
} while (0)

static int                      major = 0;
module_param (major, int, 0);

MODULE_AUTHOR("Marius Gligor");
MODULE_DESCRIPTION("Get Current CPU");
MODULE_LICENSE("Dual BSD/GPL");

typedef struct
{
    struct cdev                 dev;
} getcrtcpu_t;
getcrtcpu_t                     getcrtcpu;

extern struct file_operations   getcrtcpu_fops;

/*
 * init and cleanup
 */

int getcrtcpu_init (void)
{
    int                         result, err;
    dev_t                       devID = MKDEV (major, 0);

    PRINTKD(KERN_ALERT "getcrtcpu_init\n");

    // register major/accept a dynamic number
    if (major)
        result = register_chrdev_region (devID, 1, "getcrtcpu");
    else
    {
        result = alloc_chrdev_region (&devID, 0, 1, "getcrtcpu");
        major = MAJOR(devID);
    }
    if (result < 0)
        return result;
    
    memset (&getcrtcpu, 0, sizeof (getcrtcpu_t));
    cdev_init (&getcrtcpu.dev, &getcrtcpu_fops);
    getcrtcpu.dev.owner = THIS_MODULE;
    getcrtcpu.dev.ops = &getcrtcpu_fops;
    err = cdev_add (&getcrtcpu.dev, devID, 1);
    if (err)
    {
        PRINTKD (KERN_NOTICE "Error %d adding the driver.", err);
        unregister_chrdev_region (devID, 1);
    }

    return 0;
}

void getcrtcpu_cleanup (void)
{
    PRINTKD (KERN_ALERT "getcrtcpu_cleanup\n");

    cdev_del (&getcrtcpu.dev);
    unregister_chrdev_region (MKDEV (major, 0), 1);
}

/*
 * open and close
 */

int getcrtcpu_open (struct inode *inode, struct file *filp)
{
    getcrtcpu_t                 *dev;
    dev = container_of (inode->i_cdev, getcrtcpu_t, dev);

    PRINTKD (KERN_ALERT "getcrtcpu_open\n");

    filp->private_data = dev;

    return 0;
}

int getcrtcpu_release (struct inode *inode, struct file *filp)
{
    PRINTKD (KERN_ALERT "getcrtcpu_release\n");

    return 0;
}

#define smp_CPU()                                   \
    ({                                              \
        register unsigned int cpunum;               \
        __asm__("mrc p15, 0, %0, c0, c0, 5"         \
            : "=r" (cpunum));                       \
        cpunum &= 0x0F;                             \
    })

static int
getcrtcpu_chr_ioctl (struct inode *inode, struct file *file, unsigned int code, unsigned long buffer)
{
    int                         ret = 0;
    char                        cpuid;

    switch (code)
    {
    case GETCRTCPU_IOCS_GET_CPU:
        cpuid = (char) smp_CPU () + '0';
        COPY_TO_USER ((void *) buffer, (void *) &cpuid, sizeof (unsigned char));
        ret = 0;
        break;
    default:
        PRINTKD (KERN_ALERT " ioctl(0x%x, 0x%lx) not implemented\n", code, buffer);
        ret = -ENOTTY;
    }

    return ret;
}
struct file_operations getcrtcpu_fops =
{
    .owner =                THIS_MODULE,
    .open =                 getcrtcpu_open,
    .release =              getcrtcpu_release,
    .ioctl =                getcrtcpu_chr_ioctl,
};

module_init(getcrtcpu_init);
module_exit(getcrtcpu_cleanup);

