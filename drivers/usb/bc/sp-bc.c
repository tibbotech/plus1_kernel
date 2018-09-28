#include <linux/delay.h>
#include <linux/module.h>
#include <mach/io_map.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/usb/sp_usb.h>

struct bc_hcd {
	int irq;
	int port_num;
	struct task_struct *bc_polling_thread;
	wait_queue_head_t bc_polling_wq;
	bool wake_up_condition;
};

#define BC_MODULE_NAME		"sp_bc"

#define bc_debug(fmt, args...)

#define CDP_OFFSET		0
#define BC_NUM			(USB_HOST_NUM - 1)

#define USB0_PORT_NUM		0
#define USB1_PORT_NUM		1
#define USB2_PORT_NUM		2
#define	EHCI_STA_REG_PORT_0	0x9C102164
#define	EHCI_STA_REG_PORT_1	0x9C103164
#define EHCI_STA_REG_PORT_2	0x9C104164
#define	EHCI_STA_REG_UDC_0	0x9C102B10
#define	EHCI_STA_REG_UDC_1	0x9C103B10


u32 bc_switch = 0;
EXPORT_SYMBOL_GPL(bc_switch);

static int cdp[USB_HOST_NUM] = { 1, 1, 1 };

static struct bc_hcd sp_bc_hcd[USB_HOST_NUM];
static u32 *hw_port_sta[USB_HOST_NUM];
static u32 *udc_port_sta[USB_HOST_NUM];

static bool bc_polling_enable = true;
module_param(bc_polling_enable, bool, 0644);

u32 cdp_cfg16_value = 0x19;
u32 cdp_cfg17_value = 0x92;
u32 dcp_cfg16_value = 0x00;
u32 dcp_cfg17_value = 0x93;
u32 sdp_cfg16_value = 0x00;
u32 sdp_cfg17_value = 0x00;

module_param(cdp_cfg16_value, uint, 0644);
module_param(cdp_cfg17_value, uint, 0644);
module_param(dcp_cfg16_value, uint, 0644);
module_param(dcp_cfg17_value, uint, 0644);
module_param(sdp_cfg16_value, uint, 0644);
module_param(sdp_cfg17_value, uint, 0644);

EXPORT_SYMBOL_GPL(cdp_cfg16_value);
EXPORT_SYMBOL_GPL(cdp_cfg17_value);
EXPORT_SYMBOL_GPL(dcp_cfg16_value);
EXPORT_SYMBOL_GPL(dcp_cfg17_value);
EXPORT_SYMBOL_GPL(sdp_cfg16_value);
EXPORT_SYMBOL_GPL(sdp_cfg17_value);

/*
 * call the func when disconnect the port
 */
static void battery_charging_state_init(int port)
{
	volatile u32 *bc_cdp_addr;
	volatile u32 *bc_ref_addr;
	u32 val;

	bc_cdp_addr = (u32 *)VA_IOB_ADDR((149 + port) * 32 * 4) + 16;
	bc_ref_addr = (u32 *)VA_IOB_ADDR((149 + port) * 32 * 4) + 17;

	val = ioread32(bc_cdp_addr);
	val |= (3 << 3);
	val &= ~(1u << CDP_OFFSET);
	iowrite32(val, bc_cdp_addr);

	val = ioread32(bc_ref_addr);
	val = 0x92;
	iowrite32(val, bc_ref_addr);

	iowrite32(0x17, bc_cdp_addr - 16 + 3);
}

static void battery_charging_disinit(int port)
{
	volatile u32 *bc_cdp_addr;
	u32 val;

	bc_cdp_addr = (u32 *)VA_IOB_ADDR((149 + port) * 32 * 4) + 16;

	val = ioread32(bc_cdp_addr);
	val &= ~(1u << CDP_OFFSET);
	iowrite32(val, bc_cdp_addr);
}

static int is_udc_connect(int port)
{
	u32 sta_val[2];

	if (BC_NUM < port) {
		printk(KERN_NOTICE "USB BC port from 0, max is %d\n", BC_NUM);
		return 0;
	}

	if (USB2_PORT_NUM == port)
		return 0;

	sta_val[0] = ioread32(udc_port_sta[port]);
	msleep(3);
	sta_val[1] = ioread32(udc_port_sta[port]);

	if (sta_val[1] == sta_val[0])
		return 0;
	else
		return 1;
}

static int is_hw_connect(int port)
{
	u32 sta_val;

	if (BC_NUM < port) {
		printk(KERN_NOTICE "USB BC port from 0, max is %d\n", BC_NUM);
		return 0;
	}

	sta_val = ioread32(hw_port_sta[port]);

	return sta_val & 0x01;
}

/* Make sure BC status back to normal. Call the func every 100 ms */
static void battery_charging_reset(int port)
{
	volatile u32 *bc_cdp_addr;
	u32 val;

	if (BC_NUM < port) {
		printk(KERN_NOTICE "USB BC port from 0, max is %d\n", BC_NUM);
		return;
	}

	bc_cdp_addr = (u32 *)VA_IOB_ADDR((149 + port) * 32 * 4) + 16;
	/* disable & enable CDP to reset BC status */
	val = ioread32(bc_cdp_addr);
	val &= ~(0x01);
	iowrite32(val, bc_cdp_addr);

	msleep(1);

	val |= (0x01);
	iowrite32(val, bc_cdp_addr);
}

static int battery_charging_enable(int port, int enable)
{
	volatile u32 *bc_cdp_addr;
	u32 val;

	if (BC_NUM < port) {
		printk(KERN_NOTICE "USB BC port from 0, max is %d\n", BC_NUM);
		return 0;
	}

	bc_cdp_addr = (u32 *)VA_IOB_ADDR((149 + port) * 32 * 4) + 16;
	/* disable & enable CDP to reset BC status */
	val = ioread32(bc_cdp_addr);

	if (enable)
		val |= (0x01);
	else
		val &= ~(0x01);

	iowrite32(val, bc_cdp_addr);

	return 1;
}

/*
 * 1:change 0:empty
 */
static int battery_charging_current_state(int port)
{
	volatile u32 *bc_status_fsm;
	u32 val;

	if (BC_NUM < port) {
		printk(KERN_NOTICE "USB BC port from 0, max is %d\n", BC_NUM);
		return 0;
	}

	bc_status_fsm = (u32 *)VA_IOB_ADDR((149 + port) * 32 * 4);

	val = ioread32(bc_status_fsm + 12);

	if (((val & 0x07) == 0x04)
	    && (is_hw_connect(port) == 0)
	    && (is_udc_connect(port) == 0)) {
		battery_charging_reset(port);
		val = ioread32(bc_status_fsm + 12);
	}

	if ((val & 0x07) == 0x03) {
		bc_debug("-- BC[%d] change 0x%x\n", port, val & 0x07);
		return 1;
	} else
		return 0;
}

static int bc_reset_thread(void *arg)
{
	struct bc_hcd *bc_hcd = (struct bc_hcd *)arg;
	int port;

	port = bc_hcd->port_num;
	printk(KERN_NOTICE "port %d BC reset thread start\n", port);

	do {
#ifdef CONFIG_USB_LOGO_TEST
		wait_event_interruptible(bc_hcd->bc_polling_wq, bc_hcd->wake_up_condition);
		bc_hcd->wake_up_condition = false;
		if (cdp[port])
			battery_charging_enable(port, 1);

		while (bc_polling_enable) {
			u32 ret;

			ret = battery_charging_current_state(port);
			if (cdp[port] && ret) {
				msleep(100);
				battery_charging_enable(port, 0);
				break;
			}

			msleep(5);
		}

		msleep(150);
		continue;
#endif
	} while (!kthread_should_stop());

	return 0;
}

static ssize_t show_bc_switch(struct module_attribute *mattr,
			      struct module_kobject *mk, char *buffer)
{
	return 0;
}

static ssize_t store_bc_switch(struct module_attribute *mattr,
			       struct module_kobject *mk,
			       const char *buffer, size_t count)
{
	/*
	 * buffer[0] is port num,
	 * buffer[1] is BC mode,
	 * buffer[2] is certification mode.
	 */
	u8 bc_mode;
	u8 certification_mode;
	volatile u32 *uphy;

	if (kstrtouint(buffer, 0, &bc_switch) == 0) {
		if (0 == ((sp_port_enabled >> (bc_switch & 0x03)) & 0x01)) {
			printk(KERN_NOTICE "%s,port %d not support\n",
			       __FUNCTION__, bc_switch & 0x03);
			return count;
		}

		bc_mode = (bc_switch >> 8) & 0xFF;
		certification_mode = (bc_switch >> 16) & 0xFF;

		printk(KERN_NOTICE "BC switch %x %x\n", bc_switch, bc_mode);
		uphy = (u32 *)VA_IOB_ADDR((149 + (bc_switch & 0x03)) * 32 * 4);

		switch ((bc_switch >> 8) & 0xFF) {
		case 0:
			printk(KERN_NOTICE "Enable port %d CDP\n",
			       bc_switch & 0x03);
			cdp[bc_switch & 0x03] = 1;
			iowrite32(cdp_cfg16_value, uphy + CDP_REG_OFFSET);
			iowrite32(cdp_cfg17_value, uphy + DCP_REG_OFFSET);

			/* 0x4F is 'O', 0x6F is 'o', Both said general mode. */
			if ((0x4F != certification_mode)
			    && (0x6F != certification_mode)) {
				printk(KERN_NOTICE "wake up BC reset thread");
				sp_bc_hcd[bc_switch & 0x03].wake_up_condition = true;
				wake_up_interruptible(&sp_bc_hcd[bc_switch & 0x03].bc_polling_wq);
			}
			break;
		case 1:
			printk(KERN_NOTICE "Enable port %d DCP\n",
			       bc_switch & 0x03);
			cdp[bc_switch & 0x03] = 0;
			iowrite32(dcp_cfg16_value, uphy + CDP_REG_OFFSET);
			iowrite32(dcp_cfg17_value, uphy + DCP_REG_OFFSET);
			break;
		case 2:
			cdp[bc_switch & 0x03] = 0;
			printk(KERN_NOTICE "Enable port %d SDP\n",
			       bc_switch & 0x03);
			iowrite32(sdp_cfg16_value, uphy + CDP_REG_OFFSET);
			iowrite32(sdp_cfg17_value, uphy + DCP_REG_OFFSET);
			break;
		default:
			break;
		}
	}

	return count;
}

static struct module_attribute bc_switch_enable =
	__ATTR(bc_switch, 0644, show_bc_switch, store_bc_switch);

static int __init sp_bc_init(void)
{
	struct bc_hcd *bc_hcd = NULL;
	struct kobject *kobj;
	size_t remap_size = 1;
	int retval;
	int i;

	down(&uphy_init_sem);
	for (i = 0; i < USB_HOST_NUM; i++) {
		if (0 == ((sp_port_enabled >> i) & 0x01)) {
			continue;
		}

		if (USB2_PORT_NUM == i) {
			hw_port_sta[i] =
				ioremap_nocache(EHCI_STA_REG_PORT_2, remap_size);
		} else if (USB1_PORT_NUM == i) {
			hw_port_sta[i] =
				ioremap_nocache(EHCI_STA_REG_PORT_1, remap_size);
			udc_port_sta[i] =
				ioremap_nocache(EHCI_STA_REG_UDC_1, remap_size);
		} else {
			hw_port_sta[i] =
				ioremap_nocache(EHCI_STA_REG_PORT_0, remap_size);
			udc_port_sta[i] =
				ioremap_nocache(EHCI_STA_REG_UDC_0, remap_size);
		}

		printk(KERN_NOTICE "Init BC controller %d\n", i);
		bc_hcd = &sp_bc_hcd[i];
		bc_hcd->port_num = i;

		bc_hcd->wake_up_condition = false;
		init_waitqueue_head(&bc_hcd->bc_polling_wq);
		bc_hcd->bc_polling_thread =
			kthread_create(bc_reset_thread, bc_hcd, "bc_kthread");
		if (IS_ERR(bc_hcd->bc_polling_thread)) {
			pr_err("Create BC(%d) thread fail!\n", i);
		}

		battery_charging_state_init(i);
		wake_up_process(bc_hcd->bc_polling_thread);
	}

	kobj = kset_find_obj(module_kset, BC_MODULE_NAME);
	retval = sysfs_create_file(kobj, &bc_switch_enable.attr);
	if (retval) {
		printk(KERN_NOTICE "warn, create bc_switch sysfs file fail(%d)\n", retval);
	}

	return 0;
}

static void __exit sp_bc_cleanup(void)
{
	int i;
	int retval;
	struct bc_hcd *bc_hcd = NULL;

	for (i = 0; i < USB_HOST_NUM; i++) {
		if (0 == ((sp_port_enabled >> i) & 0x01)) {
			continue;
		}

		if (i != USB2_PORT_NUM) {
			iounmap(hw_port_sta[i]);
			iounmap(udc_port_sta[i]);
		} else {
			iounmap(hw_port_sta[i]);
		}

		bc_hcd = &sp_bc_hcd[i];
		battery_charging_disinit(i);
		retval = kthread_stop(bc_hcd->bc_polling_thread);
		printk(KERN_DEBUG "port %d BC polling thread exit status %d\n", i, retval);
	}
}

module_init(sp_bc_init);
module_exit(sp_bc_cleanup);
