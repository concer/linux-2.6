#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <mach/clkdev.h>

int clk_enable(struct clk *clk)
{
	return 1;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{}
EXPORT_SYMBOL(clk_disable);
