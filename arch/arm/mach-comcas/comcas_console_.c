#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/module.h>
#include <linux/console.h>
#include <asm/io.h>
#include <mach/hardware.h>

#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "TIMA Laboratory"
#define DRIVER_DESC "Comcas serial driver"

/* Module information */
MODULE_AUTHOR (DRIVER_AUTHOR);
MODULE_DESCRIPTION (DRIVER_DESC);
MODULE_LICENSE ("GPL");


#define UART_GET_IRQ_ENABLE(port)       readl  (__io_address (COMCAS_UART0_BASE) + 0x0C)
#define UART_GET_RX_STATUS(port)        readl  (__io_address (COMCAS_UART0_BASE) + 0x08)
#define UART_GET_CHAR(port)	            readl  (__io_address (COMCAS_UART0_BASE) + 0x00)
#define UART_PUT_CHAR(port,v)           writeb ((v),__io_address (COMCAS_UART0_BASE) + 0x00)
#define UART_PUT_IRQ_ENABLE(port,v)     writel ((v),__io_address (COMCAS_UART0_BASE) + 0x04)

#define COMCAS_SERIAL_MAJOR     254     /* experimental range */
#define COMCAS_SERIAL_MINORS    0       /* only have one minor */
#define UART_NR                 1       /* only use one port */

#define COMCAS_SERIAL_NAME      "ttyAMA"
#define MY_NAME                 COMCAS_SERIAL_NAME

#define DELAY_TIME              2

//#define DEBUG_COMCAS_CONSOLE

#ifdef DEBUG_COMCAS_CONSOLE
    #define dbg(fmt, arg...)                                                            \
        do {                                                                            \
            printk (KERN_DEBUG "%s: %s: " fmt "\n", MY_NAME , __FUNCTION__ , ## arg);   \
        } while (0)
#else
    #define dbg(fmt, arg...) 
#endif

#define err(format, arg...) printk(KERN_ERR "%s: " format "\n" , MY_NAME , ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format "\n" , MY_NAME , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format "\n" , MY_NAME , ## arg)

static struct timer_list *timer = NULL;

void comcas_gic_mask_irq (unsigned int irq);
void comcas_gic_unmask_irq (unsigned int irq);

static void comcas_stop_tx (struct uart_port *port)
{
    dbg ();
}

static void comcas_stop_rx (struct uart_port *port)
{
    dbg ();

//    UART_PUT_IRQ_ENABLE (port, 0);
}

static void comcas_enable_ms (struct uart_port *port)
{
    dbg ();
}

static void comcas_tx_chars (struct uart_port *port)
{
    struct circ_buf     *xmit = &port->state->xmit;

    dbg ();

    if (port->x_char)
    {
        UART_PUT_CHAR (port, port->x_char);
        port->icount.tx++;
        port->x_char = 0;
        return;
    }
    
    if (uart_circ_empty (xmit) || uart_tx_stopped (port))
    {
        comcas_stop_tx (port);
        return;
    }

    do
    {
        UART_PUT_CHAR (port, xmit->buf[xmit->tail]);
        xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
        port->icount.tx++;
        if (uart_circ_empty (xmit))
            break;
    } while (1);

	if (uart_circ_chars_pending (xmit) < WAKEUP_CHARS)
        uart_write_wakeup (port);

    if (uart_circ_empty (xmit))
        comcas_stop_tx (port);
}

static void
comcas_rx_chars (struct uart_port *port)
{
	struct tty_struct       *tty = port->state->port.tty;
	unsigned long           status, ch;

	while ((status = UART_GET_RX_STATUS (port)) & 0x01)
	{
		ch = UART_GET_CHAR (port);

		port->icount.rx++;

		if (uart_handle_sysrq_char (port, ch))
			continue;

		uart_insert_char (port, status, 0, ch, TTY_NORMAL);
	}

	tty_flip_buffer_push (tty);
}

static void comcas_timer (unsigned long data)
{
	struct uart_port *port;
	struct tty_struct *tty;

	dbg ();

	port = (struct uart_port *) data;
	if (!port)
        return;

	tty = port->state->port.tty;
	if (!tty)
        return;

    comcas_rx_chars (port);

	/* resubmit the timer again */
	//timer->expires = jiffies + DELAY_TIME;
	//add_timer (timer);
}

static void comcas_start_tx (struct uart_port *port)
{
    dbg ();

    comcas_tx_chars (port);
}

static unsigned int comcas_tx_empty (struct uart_port *port)
{
    return TIOCSER_TEMT;
}

static unsigned int comcas_get_mctrl (struct uart_port *port)
{
    return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;;
}

static void comcas_set_mctrl (struct uart_port *port, unsigned int mctrl)
{
}

static void comcas_break_ctl (struct uart_port *port, int break_state)
{
}

static int comcas_startup (struct uart_port *port)
{
    /* this is the first time this port is opened */
    /* do any hardware initialization needed here */

    //enable gic
    comcas_gic_unmask_irq (IRQ_UART0);
    
    //enable tty
    UART_PUT_IRQ_ENABLE (port, 1);
    
	/* create our timer and submit it */
	if (!timer)
	{
		timer = kmalloc (sizeof (*timer), GFP_KERNEL);
		if (!timer)
			return -ENOMEM;
		init_timer(timer);
	}
	timer->data = (unsigned long) port;
	timer->function = comcas_timer;
	
	//timer->expires = jiffies + DELAY_TIME;
	//add_timer (timer);

    return 0;
}

static void comcas_shutdown (struct uart_port *port)
{
    /* The port is being closed by the last user. */
    /* Do any hardware specific stuff here */
    
    //disable tty
    UART_PUT_IRQ_ENABLE (port, 0);

    
    /* shut down our timer */
	del_timer (timer);
}

static void comcas_set_termios (struct uart_port *port,
    struct ktermios *termios, struct ktermios *old_termios)
{
}

static const char *comcas_type (struct uart_port *port)
{
    return "comcas-tty";
}

static void comcas_release_port (struct uart_port *port)
{
}

static int comcas_request_port (struct uart_port *port)
{
    return 0;
}

static void comcas_config_port (struct uart_port *port, int flags)
{
}

static int comcas_verify_port (struct uart_port *port, struct serial_struct *ser)
{
    return 0;
}

static struct uart_ops comcas_ops = 
{
    .tx_empty       = comcas_tx_empty,
    .set_mctrl      = comcas_set_mctrl,
    .get_mctrl      = comcas_get_mctrl,
    .stop_tx        = comcas_stop_tx,
    .start_tx       = comcas_start_tx,
    .stop_rx        = comcas_stop_rx,
    .enable_ms      = comcas_enable_ms,
    .break_ctl      = comcas_break_ctl,
    .startup        = comcas_startup,
    .shutdown       = comcas_shutdown,
    .set_termios    = comcas_set_termios,
    .type           = comcas_type,
    .release_port   = comcas_release_port,
    .request_port   = comcas_request_port,
    .config_port    = comcas_config_port,
    .verify_port    = comcas_verify_port,
};

static struct uart_port comcas_port = 
{
	.fifosize       =   4096,
    .ops            =   &comcas_ops,
    .type           =   95,
};

static struct console comcas_console;
static struct uart_driver comcas_reg = 
{
    .owner          = THIS_MODULE,
    .driver_name    = COMCAS_SERIAL_NAME,
    .dev_name       = COMCAS_SERIAL_NAME,
    .major          = COMCAS_SERIAL_MAJOR,
    .minor          = COMCAS_SERIAL_MINORS,
    .nr             = UART_NR,
    .cons           = &comcas_console,
};

static irqreturn_t comcas_rx_int (int irq, void *dev_id)
{
    dbg ();

    comcas_rx_chars (&comcas_port);

    return IRQ_HANDLED;
}

static int __init comcas_init (void)
{
    int result;

    info ("COMCAS serial driver");

    result = uart_register_driver (&comcas_reg);
    if (result)
        return result;

    result = uart_add_one_port (&comcas_reg, &comcas_port);
    if (result)
        uart_unregister_driver (&comcas_reg);

    if (request_irq (IRQ_UART0, comcas_rx_int, IRQF_DISABLED, "serial_RX", NULL))
        panic ("Couldn't register serial receive ISR!\n");

    return result;
}

module_init (comcas_init);

static void
comcas_console_write (struct console *co, const char *s, unsigned int count)
{
    while (count-- > 0)
    {
        UART_PUT_CHAR (&comcas_port, *s);
        s++;
    }
}

static int __init
comcas_console_setup (struct console *co, char *options)
{
	if (co->index == -1 || co->index >= 1)
		co->index = 0;

	return 0;
}

static struct console comcas_console = 
{
	.name		= COMCAS_SERIAL_NAME,
	.write		= comcas_console_write,
	.device		= uart_console_device,
	.setup		= comcas_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &comcas_reg,
};

static int __init comcas_console_init (void)
{
	register_console (&comcas_console);

	return 0;
}

console_initcall (comcas_console_init);

