/*
 * Debugging macro include header
*/

.macro	addruart, rx, tmp
    mrc	p15, 0, \rx, c1, c0
    tst	\rx, #1			@ MMU enabled?
    moveq	\rx,      #0x10000000
    movne	\rx,      #0xf0000000	@ virtual base
    orr	\rx, \rx, #0x00009000
.endm

.macro  senduart,rd,rx
    strb    \rd, [\rx, #0x00]
.endm

.macro  waituart,rd,rx
.endm

.macro  busyuart,rd,rx
.endm
