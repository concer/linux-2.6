#
# Makefile for the linux kernel.
#

obj-y							:= core.o
obj-$(CONFIG_MACH_COMCAS)		+= comcas.o comcas_console_.o getcrtcpu.o clock.o
obj-$(CONFIG_SMP)				+= platsmp.o headsmp.o localtimer.o
obj-$(CONFIG_HOTPLUG_CPU)		+= hotplug.o
