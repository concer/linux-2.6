/*
 * Debugging macro include header
*/

.macro	addruart, rp, rv
    mov		\rp,	#0x10000000	@ physical base
    mov		\rv,	#0xf0000000	@ virtual base
    orr		\rp, \rp, #0x00009000
    orr		\rv, \rv, #0x00009000
.endm

.macro  senduart,rd,rx
    strb    \rd, [\rx, #0x00]
.endm

.macro  waituart,rd,rx
.endm

.macro  busyuart,rd,rx
.endm
