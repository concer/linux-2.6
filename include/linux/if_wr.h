/*
 * White Rabbit MCH user-space interface
 *
 *  Copyright (c) 2009 Emilio G. Cota <cota@braap.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_IF_WR_H
#define _LINUX_IF_WR_H

#include <linux/sockios.h>

#define WR_MCH_IOCGTXHWTSTAMP	SIOCDEVPRIVATE
#define WR_MCH_IOCGRXHWTSTAMP	((SIOCDEVPRIVATE) + 1)

#endif /* _LINUX_IF_WR_H */
