/*copied from drivers/char/amiserial.c */

#include <asm/io.h>
#include <mach/hardware.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/console.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <asm/uaccess.h>

static struct tty_driver        *serial_driver;
static char                     *serial_name = "COMCAS-builtin serial driver";
static char                     *serial_version = "1.00";
static struct tty_struct        *irq_tty = NULL;

void comcas_gic_mask_irq (unsigned int irq);
void comcas_gic_unmask_irq (unsigned int irq);

//#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
#define DPRINTK printk
#else
#define DPRINTK if (0) printk
#endif

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int rs_open (struct tty_struct *tty, struct file * filp)
{
    int                         line;

    irq_tty = tty;
    line = tty->index;

    DPRINTK ("DEBUG rs_open %s\n", tty->name);

    //enable gic
    comcas_gic_unmask_irq (IRQ_UART0);

    DPRINTK ("DEBUG rs_open %s successful...", tty->name);

    return 0;
}

/* ------------------------------------------------------------
 * rs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void rs_close (struct tty_struct *tty, struct file * filp)
{
    DPRINTK ("DEBUG rs_close %d\n", tty->index);

    comcas_gic_mask_irq (IRQ_UART0);
}

static int rs_put_char (struct tty_struct *tty, unsigned char ch)
{
    writeb (ch, __io_address (COMCAS_UART0_BASE));

    return 1;
}

static void rs_flush_chars(struct tty_struct *tty)
{
}

static int rs_write (struct tty_struct * tty, const unsigned char *buf, int count)
{
    while (count-- > 0)
    {
        writeb (*buf++, __io_address (COMCAS_UART0_BASE));
    }

    return count;
}

static int rs_write_room (struct tty_struct *tty)
{
    return 4096;
}

static int rs_chars_in_buffer (struct tty_struct *tty)
{
    return 0;
}

static void rs_flush_buffer (struct tty_struct *tty)
{
    tty_wakeup (tty);
}

struct ktermios g_termios = 
{
    .c_iflag = ICRNL | IXON | IUTF8,
    .c_oflag = ONLCR,
    .c_cflag = CREAD | CS8 | B9600,
    .c_lflag = /*IEXTEN | */ECHOKE | ECHOCTL | ECHOK | ECHOE | ECHO | ICANON | ISIG,
    .c_line = 0,
    .c_cc = {   0x03, 0x1C, 0x7F, 0x15, 
                0x04, 0x00, 0x01, 0x00,
                0x11, 0x13, 0x1A, 0x00,
                0x12, 0x0F, 0x17, 0x16,
                0x00
            },
    .c_ispeed = B9600,
    .c_ospeed = B9600
};

static int rs_ioctl (struct tty_struct *tty, struct file * file,
    unsigned int cmd, unsigned long arg)
{
    DPRINTK ("DEBUG %s - cmd=0x%x, arg=0x%lx\n", __FUNCTION__, cmd, arg);

    switch (cmd)
    {
         case TCGETS:
            if (kernel_termios_to_user_termios_1 ((struct termios __user *) arg, &g_termios))
                return -EFAULT;
            break;

//         case TCSETS:
//             if (user_termios_to_kernel_termios_1 (&g_termios, (struct termios __user *)arg))
//                 return -EFAULT;
//             break;

        default:
            return -ENOIOCTLCMD;
    }

    return 0;
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void rs_throttle (struct tty_struct * tty)
{
    char    buf[64];
    DPRINTK ("DEBUG throttle %s: %d....\n", tty_name (tty, buf),
        tty->ldisc->ops->chars_in_buffer (tty));
}

static void rs_unthrottle (struct tty_struct * tty)
{
    char    buf[64];
    DPRINTK ("DEBUG unthrottle %s: %d....\n", tty_name (tty, buf),
        tty->ldisc->ops->chars_in_buffer(tty));
}

/*
 * ------------------------------------------------------------
 * rs_stop() and rs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void rs_stop (struct tty_struct *tty)
{
    DPRINTK ("DEBUG %s - %s\n", __FUNCTION__, tty->name);

    writel (0x00, __io_address (COMCAS_UART0_BASE) + 0x04);
}

static void rs_start (struct tty_struct *tty)
{
    DPRINTK ("DEBUG %s - %s\n", __FUNCTION__, tty->name);

    writel (0x01, __io_address (COMCAS_UART0_BASE) + 0x04);
}

static void rs_set_termios (struct tty_struct *tty, struct ktermios *old_termios)
{
    DPRINTK ("DEBUG %s - %s\n", __FUNCTION__, tty->name);

    tty->termios = &g_termios;

    rs_start (tty);
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rs_hangup (struct tty_struct *tty)
{
    DPRINTK ("DEBUG %s - %s\n", __FUNCTION__, tty->name);
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
static int rs_break(struct tty_struct *tty, int break_state)
{
    DPRINTK ("DEBUG %s - %s\n", __FUNCTION__, tty->name);
	return 0;
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void rs_send_xchar (struct tty_struct *tty, char ch)
{
    DPRINTK ("DEBUG %s - %s - ch=%d\n", __FUNCTION__, tty->name, (int) ch);
}

/*
 * rs_wait_until_sent() --- wait until the transmitter is empty
 */
static void rs_wait_until_sent (struct tty_struct *tty, int timeout)
{
    DPRINTK ("DEBUG %s - %s, timeout=%d\n", __FUNCTION__, tty->name, timeout);
}

static int rs_tiocmget (struct tty_struct *tty, struct file *file)
{

    DPRINTK ("DEBUG %s - %s\n", __FUNCTION__, tty->name);

    return TIOCM_RTS;
}

static int rs_tiocmset (struct tty_struct *tty, struct file *file,
    unsigned int set, unsigned int clear)
{
    DPRINTK ("DEBUG %s - %s\n", __FUNCTION__, tty->name);

    return 0;
}

static int rs_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "serinfo:1.0 driver:%s\n", serial_version);
	return 0;
}

static int rs_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rs_proc_show, NULL);
}

static const struct file_operations rs_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= rs_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct tty_operations serial_ops =
{
    .open = rs_open,
    .close = rs_close,
    .write = rs_write,
    .put_char = rs_put_char,
    .flush_chars = rs_flush_chars,
    .write_room = rs_write_room,
    .chars_in_buffer = rs_chars_in_buffer,
    .flush_buffer = rs_flush_buffer,
    .ioctl = rs_ioctl,
    .throttle = rs_throttle,
    .unthrottle = rs_unthrottle,
    .set_termios = rs_set_termios,
    .stop = rs_stop,
    .start = rs_start,
    .hangup = rs_hangup,
    .break_ctl = rs_break,
    .send_xchar = rs_send_xchar,
    .wait_until_sent = rs_wait_until_sent,
    .tiocmget = rs_tiocmget,
    .tiocmset = rs_tiocmset,
	.proc_fops = &rs_proc_fops,
};

static void show_serial_version (void)
{
    printk (KERN_INFO "%s version %s\n", serial_name, serial_version);
}

static void receive_chars (void)
{
    unsigned long       status;
    unsigned char       ch;

    do
    {
        status = readl (__io_address (COMCAS_UART0_BASE) + 0x08);
        if ((status & 0x01) == 0)
            break;

        ch = (unsigned char) readl (__io_address (COMCAS_UART0_BASE) + 0x00);

        DPRINTK("received char 0x%x, %c\n", (unsigned int) ch, ch);

        tty_insert_flip_char (irq_tty, ch, TTY_NORMAL);

        tty_flip_buffer_push (irq_tty);
    }
    while (1);
}

static irqreturn_t ser_rx_int (int irq, void *dev_id)
{
    DPRINTK ("DEBUG %s\n", __FUNCTION__);

    receive_chars ();

    DPRINTK ("DEBUG %s - end\n", __FUNCTION__);

    return IRQ_HANDLED;
}

/*
 * The serial driver boot-time initialization code!
 */
static int __init rs_init (void)
{
    serial_driver = alloc_tty_driver (1);
    if (!serial_driver)
        return -ENOMEM;

    show_serial_version ();

    /* Initialize the tty_driver structure */
    serial_driver->owner = THIS_MODULE;
    serial_driver->driver_name = "comcasserial";
    serial_driver->name = "ttyAMA";
    serial_driver->major = 254;//TTY_MAJOR;
    serial_driver->minor_start = 0;//64;
    serial_driver->type = TTY_DRIVER_TYPE_SERIAL;
    serial_driver->subtype = SERIAL_TYPE_NORMAL;
    serial_driver->init_termios = tty_std_termios;
    serial_driver->init_termios.c_cflag =
        B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    serial_driver->flags = TTY_DRIVER_REAL_RAW;
    tty_set_operations (serial_driver, &serial_ops);

    if (tty_register_driver (serial_driver))
        panic ("Couldn't register serial driver\n");

    if (request_irq (IRQ_UART0, ser_rx_int, IRQF_DISABLED, "serial_RX", NULL))
        panic ("Couldn't register serial receive ISR!\n");

    return 0;
}

static __exit void rs_exit (void) 
{
    int             error;

    printk ("Unloading %s: version %s\n", serial_name, serial_version);

    if ((error = tty_unregister_driver (serial_driver)))
        printk ("SERIAL: failed to unregister serial driver (%d)\n", error);

    put_tty_driver (serial_driver);
}

module_init (rs_init)
module_exit (rs_exit)

static void serial_console_write (struct console *co, const char *s,
    unsigned count)
{
    while (count-- > 0)
    {
        writeb (*s++, __io_address (COMCAS_UART0_BASE));
    }
}

//early printk
static struct tty_driver *serial_console_device (struct console *c, int *index)
{
    *index = 0;
    return serial_driver;
}

static int serial_console_setup (struct console *con, char *options)
{
    return 0;
}

static struct console sercons =
{
    .name       =   "ttyAMA",
    .write      =   serial_console_write,
    .setup      =   serial_console_setup,
    .device     =   serial_console_device,
    .flags      =    CON_PRINTBUFFER,
    .index      =    0,
};

static int __init comcas_console_init (void)
{
    register_console (&sercons);
    return 0;
}

console_initcall(comcas_console_init);
MODULE_LICENSE("GPL");
