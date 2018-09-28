#ifndef __SP_USB_H
#define __SP_USB_H

#include <asm/io.h>
#include <mach/irqs.h>
#include <mach/io_map.h>
#include <mach/gpio_drv.h>
#include <linux/semaphore.h>
#include <linux/io.h>

#ifdef CONFIG_SUPPORT_RTTRACK
#define RUNTIME_TRACKING_USB
#endif

#ifdef RUNTIME_TRACKING_USB
#include <mach/sp_runtime_tracking.h>
#endif

#ifdef RUNTIME_TRACKING_USB
#define PLATFORM_HOST_MODE			0
#define PLATFORM_DEVICE_MODE			1
#endif

#define ACC_PORT0				0
#define ACC_PORT1				1

#define USB_PORT0_ID				0
#define USB_PORT1_ID				1
#define USB_PORT2_ID				2

#define USB_HOST_NUM				3

#define WAIT_TIME_AFTER_RESUME			25
#define ELAPSE_TIME_AFTER_SUSPEND		15000
#define SEND_SOF_TIME_BEFORE_SUSPEND		15000
#define SEND_SOF_TIME_BEFORE_SEND_IN_PACKET	15000

#define SP_IC_A_VERSION				0x0630
#define SP_IC_B_VERSION				0x0631
#define DCP_INIT_VALUE				0x92
#define BC_DEBUG_SIGNAL_VALUE			0x17
#define	VBUS_GPIO_CTRL_1			87
#define	VBUS_GPIO_CTRL_0			7

#define GET_BC_MODE				0xFF00
#define CDP_MODE_VALUE				0
#define DCP_MODE_VALUE				1
#define SDP_MODE_VALUE				2
#define OTP_DISC_LEVEL_TEMP			0x16
#define DISC_LEVEL_DEFAULT			0x0B
#define OTP_DISC_LEVEL_BIT			0x1F
#define PORT1_SWING_DEFAULT_VALUE		0x86
#define PORT0_PLL_POWER_OFF			0x88
#define PORT0_PLL_POWER_ON			0x80
#define PORT1_PLL_POWER_OFF			0x8800
#define PORT1_PLL_POWER_ON			0x8000
#define PLL_CTRL_DEFAULT_VALUE			0x00

#define PORT0_ENABLED				(1 << 0)
#define PORT1_ENABLED				(1 << 1)
#define PORT2_ENABLED				(1 << 2)

#define POWER_SAVING_SET			(1 << 5)
#define ECO_PATH_SET				(1 << 6)

#define	VBUS_POWER_MASK				(1 << 0)
#define	VBUS_RESET				(1 << 1)
#define	UPHY_DISC_0				(1 << 2)

#define UPHY0_NEGEDGE_CLK			(1 << 6)
#define UPHY1_NEGEDGE_CLK			(1 << 14)
#define UPHY2_NEGEDGE_CLK			(1 << 22)

#define PORT0_OTG_EN_PINMUX			(1 << 2)
#define PORT1_OTG_EN_PINMUX			(1 << 3)

#define PORT0_MODE_CTRL				(3 << 4)
#define PORT0_SWITCH_HOST			(7 << 4)

#define PORT1_SWITCH_HOST			(7 << 12)
#define PORT1_MODE_CTRL				(3 << 12)

#define OTG0_SELECTED_BY_HW			(1 << 4)
#define OTG1_SELECTED_BY_HW			(1 << 12)

#define ENABLE_UPHY0_RESET			(1 << 13)
#define ENABLE_UPHY1_RESET			(1 << 14)

#define ENABLE_USBC0_RESET			(1 << 10)
#define ENABLE_USBC1_RESET			(1 << 11)

#define UPHY_HANDLE_DELAY_TIME			1
#define VBUS_HANDLE_DELAY_TIME			100

#define CDP_INIT_SET				((3 << 3) | (1u))

#define REG_BASE_PARAM				(32 * 4)

#define MONG0_REG_BASE				0
#define MONG1_REG_BASE				1
#define MONG2_REG_BASE				2
#define UPHY0_REG_BASE				149
#define UPHY1_REG_BASE				150
#define UPHY2_REG_BASE				151
#define OTP_REG_BASE				350

#define	STAMP_ID_OFFSET				0
#define	POWER_SAVING_OFFSET			1
#define SWING_REG_OFFSET			2
#define USB_UPHY_OTG_REG			3
#define	DEBUG_TABLE_OFFSET			3
#define OTP_DISC_LEVEL_OFFSET			6

#define PINMUX_CTRL7_OFFSET			8

#define DISC_LEVEL_OFFSET			7
#define	ECO_PATH_OFFSET				9
#define	UPHY_DISC_OFFSET			10

#define CDP_REG_OFFSET				16
#define	DCP_REG_OFFSET				17

#define	UPHY0_CTRL_OFFSET			14
#define	UPHY1_CTRL_OFFSET			15
#define	UPHY2_CTRL_OFFSET			18

#define	HW_RESET_CTRL_OFFSET			18

#define PLL_CTRL_OFFSET				21
#define	UPHY_CLOCK_OFFSET			19

#define UPHY1_OTP_DISC_LEVEL_OFFSET		5
#define UPHY2_OTP_DISC_LEVEL_OFFSET		10

#define	UPHY0_CTRL_DEFAULT_VALUE		0x87474002
#define	UPHY1_CTRL_DEFAULT_VALUE		0x87474004
#define	UPHY2_CTRL_DEFAULT_VALUE		0x87474006

#define EHCI_CONNECT_STATUS_CHANGE		0x00000002
#define OHCI_CONNECT_STATUS_CHANGE		0x00010000
#define CURRENT_CONNECT_STATUS			0x00000001
#define PORT_OWNERSHIP				0x00002000

#ifdef RUNTIME_TRACKING_USB
enum USB_RTTrack_Param_e {
	USB_RTTRACK_ENUM_STATE = 0,
	USB_RTTRACK_PLATFORM_MODE,
	USB_RTTRACK_SPEED,
	USB_RTTRACK_MAX,
};

extern int usb_dTrackIdx;
#endif

extern u32 bc_switch;
extern u32 cdp_cfg16_value;
extern u32 cdp_cfg17_value;
extern u32 dcp_cfg16_value;
extern u32 dcp_cfg17_value;
extern u32 sdp_cfg16_value;
extern u32 sdp_cfg17_value;

extern u8 max_topo_level;
extern bool tid_test_flag;
extern u8 sp_port_enabled;
extern uint accessory_port_id;
extern struct semaphore uphy_init_sem;
extern bool enum_rx_active_flag[USB_HOST_NUM];
extern bool platform_device_mode_flag[USB_HOST_NUM];
extern struct semaphore enum_rx_active_reset_sem[USB_HOST_NUM];

typedef enum {
	eHW_GPIO_FIRST_FUNC = 0,
	eHW_GPIO_FIRST_GPIO = 1,
	eHW_GPIO_FIRST_NULL
} eHW_GPIO_FIRST;

typedef enum {
	eHW_GPIO_IOP = 0,
	eHW_GPIO_RISC = 1,
	eHW_GPIO_MASTER_NULL
} eHW_GPIO_Master;

typedef enum {
	eHW_GPIO_IN = 0,
	eHW_GPIO_OUT = 1,
	eHW_GPIO_OE_NULL
} eHW_GPIO_OE;

typedef enum {
	eHW_GPIO_STS_LOW = 0,
	eHW_GPIO_STS_HIGH = 1,
	eHW_GPIO_STS_NULL
} eHW_IO_STS;


#define SET_USB_VBUS(gpio, Val) do {			\
	GPIO_F_SET((gpio), eHW_GPIO_FIRST_GPIO);	\
	GPIO_M_SET((gpio), eHW_GPIO_RISC);		\
	GPIO_E_SET((gpio), eHW_GPIO_OUT);		\
	GPIO_O_SET((gpio), (Val) & 0x01);		\
} while (0)

const static u8 USB_PORT_NUM[2] = {VBUS_GPIO_CTRL_0, VBUS_GPIO_CTRL_1};

#define	ENABLE_VBUS_POWER(port) SET_USB_VBUS(USB_PORT_NUM[port], eHW_GPIO_STS_HIGH)

#define	DISABLE_VBUS_POWER(port) SET_USB_VBUS(USB_PORT_NUM[port], eHW_GPIO_STS_LOW)

static inline void uphy_force_disc(int en, int port)
{
	volatile u32 *uphy_disc;
	u32 uphy_val;

	if (port > USB_PORT1_ID)
		printk("-- err port num %d\n", port);

	uphy_disc = (u32 *)VA_IOB_ADDR((UPHY0_REG_BASE + port) * 32 * 4);
	uphy_val = ioread32(uphy_disc + UPHY_DISC_OFFSET);
	if (en) {
		uphy_val |= UPHY_DISC_0;
	} else {
		uphy_val &= ~UPHY_DISC_0;
	}

	iowrite32(uphy_val, uphy_disc + UPHY_DISC_OFFSET);
}

#define Reset_Usb_Power(port) do {	\
	DISABLE_VBUS_POWER(port);	\
	uphy_force_disc(1, (port));	\
	msleep(500);			\
	uphy_force_disc(0, (port));	\
	ENABLE_VBUS_POWER(port);	\
} while (0)

static inline int get_uphy_swing(int port)
{
	u32 *swing_reg;
	u32 val;

	if (port > USB_PORT1_ID) {
		return -1;
	}
	swing_reg = (u32 *)VA_IOB_ADDR(MONG2_REG_BASE * 32 * 4);
	val = ioread32(swing_reg + SWING_REG_OFFSET);

	return (val >> (port * 8)) & 0xFF;
}

static inline int set_uphy_swing(u32 swing, int port)
{
	u32 *swing_reg;
	u32 val;

	if (port > USB_PORT1_ID) {
		return -1;
	}
	swing_reg = (u32 *)VA_IOB_ADDR(MONG2_REG_BASE * 32 * 4);
	val = ioread32(swing_reg + SWING_REG_OFFSET);
	val &= ~(0x3F << (port * 8));
	val |= (swing & 0x3F) << (port * 8);
	val |= 1 << (port * 8 + 7);
	iowrite32(val, swing_reg + SWING_REG_OFFSET);

	return 0;
}

static inline int get_disconnect_level(int port)
{
	u32 *disconnect_reg;
	u32 val;

	if (port > USB_PORT1_ID) {
		return -1;
	}

	disconnect_reg = (u32 *)VA_IOB_ADDR((UPHY0_REG_BASE + port) * 32 * 4);
	val = ioread32(disconnect_reg + DISC_LEVEL_OFFSET);

	return val & 0x1F;
}

static inline int set_disconnect_level(u32 disc_level, int port)
{
	u32 *disconnect_reg;
	u32 val;

	if (port > USB_PORT1_ID) {
		return -1;
	}

	disconnect_reg = (u32 *)VA_IOB_ADDR((UPHY0_REG_BASE + port) * 32 * 4);
	val = ioread32(disconnect_reg + DISC_LEVEL_OFFSET);
	val = (val & ~0x1F) | disc_level;
	iowrite32(val, disconnect_reg + DISC_LEVEL_OFFSET);

	return 0;
}

/*
 * return: 0 = device, 1 = host
 */
static inline int get_uphy_owner(int port)
{
	volatile u32 *group = (u32 *)VA_IOB_ADDR(MONG2_REG_BASE * REG_BASE_PARAM);
	u32 val;

	val = ioread32(group + USB_UPHY_OTG_REG);

	return (val & (1 << (port * 8 + 5))) ? 1 : 0;
}

static inline void reinit_uphy(int port)
{
	volatile u32 *uphy_base = (u32 *)VA_IOB_ADDR((UPHY0_REG_BASE + port) * REG_BASE_PARAM);
	u32 val;

	val = ioread32(uphy_base + ECO_PATH_OFFSET);
	val &= ~(ECO_PATH_SET);
	iowrite32(val, uphy_base + ECO_PATH_OFFSET);
	val = ioread32(uphy_base + POWER_SAVING_OFFSET);
	val &= ~(POWER_SAVING_SET);
	iowrite32(val, uphy_base + POWER_SAVING_OFFSET);

#ifdef CONFIG_USB_BC
	switch (bc_switch & GET_BC_MODE) {
	case CDP_MODE_VALUE:
		iowrite32(cdp_cfg16_value, uphy_base + CDP_REG_OFFSET);
		iowrite32(cdp_cfg17_value, uphy_base + DCP_REG_OFFSET);
		break;
	case DCP_MODE_VALUE:
		iowrite32(dcp_cfg16_value, uphy_base + CDP_REG_OFFSET);
		iowrite32(dcp_cfg17_value, uphy_base + DCP_REG_OFFSET);
		break;
	case SDP_MODE_VALUE:
		iowrite32(sdp_cfg16_value, uphy_base + CDP_REG_OFFSET);
		iowrite32(sdp_cfg17_value, uphy_base + DCP_REG_OFFSET);
		break;
	default:
		break;
	}
#endif
}

#endif	/* __SP_USB_H */
