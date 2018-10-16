#
# USB peripheral controller drivers
#

ccflags-y			:= -I$(srctree)/drivers/usb/gadget/
ccflags-y			+= -I$(srctree)/drivers/usb/gadget/udc/

# USB Functions
usb_f_ss_lb-y			:= f_sourcesink_iso.o
obj-$(CONFIG_USB_F_SS_LB)	+= usb_f_ss_lb.o

