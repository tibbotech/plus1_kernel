// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Sunplus MIPI CSI-2 Receiver
 *
 * Copyright Sunplus Technology Co., Ltd.
 * 	  All rights reserved.
 *
 * Based on Renesas R-Car MIPI CSI-2 Receiver driver
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/sys_soc.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>

struct csi2_dev;

/* Compliler switch */
//#define MIPI_CSI_BIST		/* Enable MIPI-CSI BIST function */

/* Max number on CSI instances that can be in a system */
#define CSI_MAX_NUM 6

/* Register offsets and bits */
#define MIPICSI_STATUS			0x00	/* 165.0 MIPICSI key signal status (mipicsi_status) */
#define MIPI_WC_CH01			0x04	/* 165.1 MIPICSI CH0 and CH1 word count value (mipi_wc_ch01) */
#define MIPI_WC_CH23			0x08	/* 165.2 MIPICSI CH2 and CH3 word count value (mipi_wc_ch23) */
#define MIPI_ANALOG_CFG0		0x0C	/* 165.3 MIPICSI analog config register 0 (mipi_analog_cfg0) */
#define MIPI_ANALOG_CFG1		0x10	/* 165.4 MIPICSI analog config register 1 (mipi_analog_cfg1) */
#define MIPI_ANALOG_CFG2		0x14	/* 165.5 MIPICSI analog config register 2 (mipi_analog_cfg2) */
#define MIPI_ANALOG_CFG3		0x18	/* 165.6 MIPICSI analog config register 3 (mipi_analog_cfg3) */
#define MIPICSI_ENABLE			0x1C	/* 165.7 Enable MIPICSI (mipicsi_enable) */
#define MIPICSI_MIX_CFG			0x20	/* 165.8 MIPICSI digital and analog config register (mipicsi_mix_cfg) */
#define MIPICSI_DELAY_CTL		0x24	/* 165.9 Signal delay setting (mipicsi_delay_ctl) */
#define MIPICSI_PACKET_SIZE		0x28	/* 165.10 Long packet size and number config (mipicsi_packet_size) */
#define MIPICSI_SOT_SYNCWORD	0x2C	/* 165.11 SOT sync word (mipicsi_sot_syncword) */
#define MIPICSI_SOF_SYNCWORD	0x30	/* 165.12 SOF sync word (mipicsi_sof_syncword) */
#define MIPICSI_SOL_SYNCWORD	0x34	/* 165.13 SOL sync word (mipicsi_sol_syncword) */
#define MIPICSI_EOF_SYNCWORD	0x38	/* 165.14 EOF sync word (mipicsi_eof_syncword) */
#define MIPICSI_LSLE_SYNCWORD	0x3C	/* 165.15 Line start and line end sync word (mipicsi_lsle_syncword) */
#define MIPICSI_ECC_ERROR		0x40	/* 165.16 ECC error status (mipicsi_ecc_error) */
#define MIPICSI_CRC_ERROR		0x44	/* 165.17 CRC error status (mipicsi_crc_error) */
#define MIPICSI_ECC_CFG			0x48	/* 165.18 ECC config (mipicsi_ecc_cfg) */
#define MIPI_ANALOG_CFG4		0x4C	/* 165.19 MIPICSI analog config register 4 (mipi_analog_cfg4) */
#define MIPI_ANALOG_CFG5		0x50	/* 165.20 MIPICSI analog config register 5 (mipi_analog_cfg5) */
#define MIPI_CH0_BIST_SIZE		0x54	/* 165.21 CH0 horizontal and vertical bist size (mipi_ch0_bist_size) */
#define MIPI_CH1_BIST_SIZE		0x58	/* 165.22 CH1 horizontal and vertical bist size (mipi_ch1_bist_size) */
#define MIPI_CH2_BIST_SIZE		0x5C	/* 165.23 CH2 horizontal and vertical bist size (mipi_ch2_bist_size) */
#define MIPI_CH3_BIST_SIZE		0x60	/* 165.24 CH3 horizontal and vertical bist size (mipi_ch3_bist_size) */
#define MIPI_CH0_CONFIG			0x64	/* 165.25 CH0 function configuration (mipi_ch0_config) */
#define MIPI_CH1_CONFIG			0x68	/* 165.26 CH1 function configuration (mipi_ch1_config) */
#define MIPI_CH2_CONFIG			0x6C	/* 165.27 CH2 function configuration (mipi_ch2_config) */
#define MIPI_CH3_CONFIG			0x70	/* 165.28 CH3 function configuration (mipi_ch3_config) */
#define VERSION_ID				0x74	/* 165.29 Version ID (Version_ID) */

#ifdef MIPI_CSI_BIST
static unsigned int bist_mode = 0;
module_param(bist_mode, uint, 0444);
MODULE_PARM_DESC(bist_mode, " Internal pattern format selection, default is 0.\n"
				"\t    0 == Color bar for YUY2 format\n"
				"\t    1 == Border for YUY2 format\n"
				"\t    1 == Gray bar for RAW12 format\n");

static unsigned int bist_ch = 0;
module_param(bist_ch, uint, 0444);
MODULE_PARM_DESC(bist_ch, " Internal pattern output channel, default is 0.");
#endif

struct csi2_format {
	u32 code;
	unsigned int datatype;
	unsigned int bpp;
};

static const struct csi2_format csi2_formats[] = {
	{ .code = MEDIA_BUS_FMT_UYVY8_2X8, .datatype = 0x1e, .bpp = 16 },
	{ .code = MEDIA_BUS_FMT_VYUY8_2X8, .datatype = 0x1e, .bpp = 16 },
	{ .code = MEDIA_BUS_FMT_YUYV8_2X8, .datatype = 0x1e, .bpp = 16 },
	{ .code = MEDIA_BUS_FMT_YVYU8_2X8, .datatype = 0x1e, .bpp = 16 },
	{ .code = MEDIA_BUS_FMT_SBGGR8_1X8, .datatype = 0x2a, .bpp = 8 },
	{ .code = MEDIA_BUS_FMT_SGBRG8_1X8, .datatype = 0x2a, .bpp = 8 },
	{ .code = MEDIA_BUS_FMT_SGRBG8_1X8, .datatype = 0x2a, .bpp = 8 },
	{ .code = MEDIA_BUS_FMT_SRGGB8_1X8, .datatype = 0x2a, .bpp = 8 },
	{ .code = MEDIA_BUS_FMT_SBGGR10_1X10, .datatype = 0x2b, .bpp = 10 },
	{ .code = MEDIA_BUS_FMT_SGBRG10_1X10, .datatype = 0x2b, .bpp = 10 },
	{ .code = MEDIA_BUS_FMT_SGRBG10_1X10, .datatype = 0x2b, .bpp = 10 },
	{ .code = MEDIA_BUS_FMT_SRGGB10_1X10, .datatype = 0x2b, .bpp = 10 },
	{ .code = MEDIA_BUS_FMT_SBGGR12_1X12, .datatype = 0x2c, .bpp = 12 },
	{ .code = MEDIA_BUS_FMT_SGBRG12_1X12, .datatype = 0x2c, .bpp = 12 },
	{ .code = MEDIA_BUS_FMT_SGRBG12_1X12, .datatype = 0x2c, .bpp = 12 },
	{ .code = MEDIA_BUS_FMT_SRGGB12_1X12, .datatype = 0x2c, .bpp = 12 },
};

static const struct csi2_format *csi2_code_to_fmt(unsigned int code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(csi2_formats); i++)
		if (csi2_formats[i].code == code)
			return &csi2_formats[i];

	return NULL;
}

enum csi2_pads {
	CSI2_SINK,
	CSI2_SOURCE_VC0,
	CSI2_SOURCE_VC1,
	CSI2_SOURCE_VC2,
	CSI2_SOURCE_VC3,
	NR_OF_CSI2_PAD,
};

struct csi2_info {
	int (*init_phtw)(struct csi2_dev *priv, unsigned int mbps);
	int (*phy_post_init)(struct csi2_dev *priv);
	const struct rcsi2_mbps_reg *hsfreqrange;
	unsigned int csi0clkfreqrange;
	unsigned int num_channels;
	bool clear_ulps;
};

struct csi2_dev {
	struct device *dev;
	void __iomem *base;
	void __iomem *base2;
	const struct csi2_info *info;
	struct clk *clk;
	struct reset_control *rstc;
	unsigned int id;

	struct v4l2_subdev subdev;
	struct media_pad pads[NR_OF_CSI2_PAD];

	struct v4l2_async_notifier notifier;
	struct v4l2_subdev *remote;
	unsigned int remote_pad;

	struct v4l2_mbus_framefmt mf;

	struct mutex lock;
	int stream_count;

	unsigned short lanes;
	unsigned char lane_swap[4];

	/* Virtual channel support */
	unsigned int max_channels;
	unsigned int num_channels;
};

static inline struct csi2_dev *sd_to_csi2(struct v4l2_subdev *sd)
{
	return container_of(sd, struct csi2_dev, subdev);
}

static inline struct csi2_dev *notifier_to_csi2(struct v4l2_async_notifier *n)
{
	return container_of(n, struct csi2_dev, notifier);
}

static inline u32 csi_readl(struct csi2_dev *priv, u32 reg)
{
	return readl(priv->base + reg);
}

static inline void csi_writel(struct csi2_dev *priv, u32 reg, u32 value)
{
	writel(value, priv->base + reg);
}

static inline void set_field(u32 *valp, u32 field, u32 mask)
{
	u32 val = *valp;

	val &= ~mask;
	val |= (field << __ffs(mask)) & mask;
	*valp = val;
}

static void csi2_enter_standby(struct csi2_dev *priv)
{
	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	// Configure PHY for standby
#if 0 // CCHo: Reset MIPI-CSI once.
	reset_control_assert(priv->rstc);
#endif
#ifdef CONFIG_PM_RUNTIME_MIPICSI
	usleep_range(100, 150);
	pm_runtime_put(priv->dev);
#endif
}

static void csi2_exit_standby(struct csi2_dev *priv)
{
	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

#ifdef CONFIG_PM_RUNTIME_MIPICSI
	pm_runtime_get_sync(priv->dev);
#endif
#if 0 // CCHo: Reset MIPI-CSI once.
	reset_control_deassert(priv->rstc);
#endif
}

static int csi2_wait_phy_start(struct csi2_dev *priv,
				unsigned int lanes)
{
	unsigned int timeout;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	/* Wait for the clock and data lanes to enter LP-11 state. */
	for (timeout = 0; timeout <= 20; timeout++) {
		// Check PHY status
		return 0;
	}

	dev_err(priv->dev, "Timeout waiting for LP-11 state\n");

	return -ETIMEDOUT;
}

static int csi2_set_phypll(struct csi2_dev *priv, unsigned int mbps)
{
	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	// Configure PHY PLL for HS speed

	return 0;
}

static int csi2_calc_mbps(struct csi2_dev *priv, unsigned int bpp,
			   unsigned int lanes)
{
	struct v4l2_subdev *source;
	struct v4l2_ctrl *ctrl;
	u64 mbps;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

#if defined(MIPI_CSI_BIST)
	return 100;		// Return 100MHz for BIST mode
#endif

	if (!priv->remote)
		return -ENODEV;

	source = priv->remote;

	/* Read the pixel rate control from remote. */
	ctrl = v4l2_ctrl_find(source->ctrl_handler, V4L2_CID_PIXEL_RATE);
	if (!ctrl) {
		dev_err(priv->dev, "no pixel rate control in subdev %s\n",
			source->name);
		return -EINVAL;
	}

	/*
	 * Calculate the phypll in mbps.
	 * link_freq = (pixel_rate * bits_per_sample) / (2 * nr_of_lanes)
	 * bps = link_freq * 2
	 */
	mbps = v4l2_ctrl_g_ctrl_int64(ctrl) * bpp;
	do_div(mbps, lanes * 1000000);

	dev_dbg(priv->dev, "%s, mbps: %lld\n", __func__, mbps);

	return mbps;
}

static int csi2_get_active_lanes(struct csi2_dev *priv,
				  unsigned int *lanes)
{
	struct v4l2_mbus_config mbus_config = { 0 };
	unsigned int num_lanes = UINT_MAX;
	int ret;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	*lanes = priv->lanes;

#if defined(MIPI_CSI_BIST)
	dev_dbg(priv->dev, "Skip sending 'get_mbus_config' command\n");
	return 0;
#endif

	ret = v4l2_subdev_call(priv->remote, pad, get_mbus_config,
			       priv->remote_pad, &mbus_config);
	if (ret == -ENOIOCTLCMD) {
		dev_dbg(priv->dev, "No remote mbus configuration available\n");
		return 0;
	}

	if (ret) {
		dev_err(priv->dev, "Failed to get remote mbus configuration\n");
		return ret;
	}

	if (mbus_config.type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(priv->dev, "Unsupported media bus type %u\n",
			mbus_config.type);
		return -EINVAL;
	}

	if (mbus_config.flags & V4L2_MBUS_CSI2_1_LANE)
		num_lanes = 1;
	else if (mbus_config.flags & V4L2_MBUS_CSI2_2_LANE)
		num_lanes = 2;
	else if (mbus_config.flags & V4L2_MBUS_CSI2_3_LANE)
		num_lanes = 3;
	else if (mbus_config.flags & V4L2_MBUS_CSI2_4_LANE)
		num_lanes = 4;

	if (num_lanes > priv->lanes) {
		dev_err(priv->dev,
			"Unsupported mbus config: too many data lanes %u\n",
			num_lanes);
		return -EINVAL;
	}

	*lanes = num_lanes;

	return 0;
}

static void csi2_lane_config(struct csi2_dev *priv, u8 lanes)
{
	u32 mix_cfg, ana_cfg5;

	mix_cfg = csi_readl(priv, MIPICSI_MIX_CFG);
	ana_cfg5 = csi_readl(priv, MIPI_ANALOG_CFG5);

	set_field(&mix_cfg, 0x1, 0x1<<15);		/* When detect EOF control word, generate EOF */
	set_field(&mix_cfg, 0x1, 0x1<<8);		/* For bit sequence of a word, transfer MSB bit first */
	set_field(&mix_cfg, 0x1, 0x1<<2);		/* Set PHY to normal mode */

	dev_dbg(priv->dev, "%s, lanes: %d\n", __func__, lanes);

	/* Set LANE_NUM and ENLP_DATA_LANE fields according lane number */
	switch (lanes) {
		default:
		case 1: /* 1 lane */
			set_field(&mix_cfg, 0x0, 0x3<<20);	/* 0x0: 1 lane */
			set_field(&ana_cfg5, 0x1, 0xf<<4);	/* Enable data lane of LP mode circuit for data lane 0 */
			break;

		case 2: /* 2 lanes */
			set_field(&mix_cfg, 0x1, 0x3<<20);	/* 0x1: 2 lanes */
			set_field(&ana_cfg5, 0x3, 0xf<<4);	/* Enable data lane of LP mode circuit for data lane 0/1 */
			break;

		case 4: /* 4 lanes */
			set_field(&mix_cfg, 0x2, 0x3<<20);	/* 0x2: 4 lanes */
			set_field(&ana_cfg5, 0xf, 0xf<<4);	/* Enable data lane of LP mode circuit for data lane 0/1/2/3 */
			break;
	}

	set_field(&ana_cfg5, 0x1, 0x1<<0);		/* Enable clock lane of LP mode circuit */

	dev_dbg(priv->dev, "%s, %d: mix_cfg: %08x, ana_cfg5: %08x\n",
		 __func__, __LINE__, mix_cfg, ana_cfg5);

	csi_writel(priv, MIPICSI_MIX_CFG, mix_cfg);
	csi_writel(priv, MIPI_ANALOG_CFG5, ana_cfg5);
}

static void csi2_vc_config(struct csi2_dev *priv)
{
	u32 sof_syncword, sol_syncword, eof_syncword;
	u32 i;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	sof_syncword = csi_readl(priv, MIPICSI_SOF_SYNCWORD);
	eof_syncword = csi_readl(priv, MIPICSI_EOF_SYNCWORD);
	sol_syncword = csi_readl(priv, MIPICSI_SOL_SYNCWORD);

	/* Set SOL_SYNCWORD field
	 * CH3_SOL_SYNCWORD, Bit[31:24]
	 * CH2_SOL_SYNCWORD, Bit[23:16]
	 * CH1_SOL_SYNCWORD, Bit[15:8]
	 * CH0_SOL_SYNCWORD, Bit[7:0]
	 */
	for (i = 0; i < priv->max_channels; i++) {
		if (i < priv->num_channels) {
			/* Set VCx to the VC field of the DI byte to enable Virtua Channel x */
			set_field(&sof_syncword, (i<<6|0x00), 0xff<<(i*8));		/* Set CHx_SOF_SYNCWORD field */
			set_field(&eof_syncword, (i<<6|0x01), 0xff<<(i*8));		/* Set CHx_EOF_SYNCWORD field */
			set_field(&sol_syncword, (i)        , 0xc0<<(i*8));		/* Set CHx_SOL_SYNCWORD field */
		} else {
			/* Set 0 to the VC field of the DI byte to disable Virtual Channel x */
			set_field(&sof_syncword, (0<<6|0x00), 0xff<<(i*8));		/* Set CHx_SOF_SYNCWORD field */
			set_field(&eof_syncword, (0<<6|0x01), 0xff<<(i*8));		/* Set CHx_EOF_SYNCWORD field */
			set_field(&sol_syncword, (0)        , 0xc0<<(i*8));		/* Set CHx_SOL_SYNCWORD field */
		}
	}

	dev_dbg(priv->dev, "sof_syncword: %08x, eof_syncword: %08x, sol_syncword: %08x\n",
		 sof_syncword, eof_syncword, sol_syncword);

	/* Set the DI byte of long/short packets */
	csi_writel(priv, MIPICSI_SOF_SYNCWORD, sof_syncword);
	csi_writel(priv, MIPICSI_EOF_SYNCWORD, eof_syncword);
	csi_writel(priv, MIPICSI_SOL_SYNCWORD, sol_syncword);

	/* Configure the connection between CSI-IW channels and Virtual channels
	 * Note: This connection shoud be based on the routing.
	 */
	csi_writel(priv, MIPI_CH0_CONFIG, 0x00<<30);		/* Connect CSI-IW0 to VC0 */
	csi_writel(priv, MIPI_CH1_CONFIG, 0x01<<30);		/* Connect CSI-IW1 to VC1 */
	csi_writel(priv, MIPI_CH2_CONFIG, 0x02<<30);		/* Connect CSI-IW2 to VC2 */
	csi_writel(priv, MIPI_CH3_CONFIG, 0x03<<30);		/* Connect CSI-IW3 to VC3 */

#if 1 /* CCHo: Get MIPICSI23_SEL G164 resource */
	/* MIPI-CSI2 and MIPI-CSI3 ports share VI23-CSIIW2 and VI23-CSIIW3. 
	 * Configure MIPICSI23_SEL (G164) to select the virtual channel source
	 * of VI23-CSIIW2 AND VI23-CSIIW3.
	 */
	if ((priv->id == 2) || (priv->id == 3)) {
		if (priv->base2 == NULL) {
			dev_warn(priv->dev, "No MIPICSI23_SEL resource\n");
		} else {
			if (((priv->id == 2) && (priv->num_channels <= 2)) || (priv->id == 3))
				writel(1, priv->base2);		/* VI23-CSIIW2/3 source from MIPI-CSI3 */
			else if ((priv->id == 2) && (priv->num_channels > 2))
				writel(0, priv->base2);		/* VI23-CSIIW2/3 source from MIPI-CSI2 */

		#if 1 /* CCHo: For debugging*/
			dev_info(priv->dev, "mipicsi23_sel: %08x, \n", readl(priv->base2));
		#else
			dev_dbg(priv->dev, "mipicsi23_sel: %08x, \n", readl(priv->base2));
		#endif
		}
	}
#endif
}

static void csi2_dt_config(struct csi2_dev *priv, unsigned int dt)
{
	u32 mix_cfg, sol_syncword;
	u32 i;

	mix_cfg = csi_readl(priv, MIPICSI_MIX_CFG);
	sol_syncword = csi_readl(priv, MIPICSI_SOL_SYNCWORD);

	dev_dbg(priv->dev, "%s, %d, dt: %02x\n", __func__, __LINE__ ,dt);

	/* Set DEC_MODE field according to Data Type */
	switch (dt) {
	default:
	case 0x1e:	/* YUY2 */
	case 0x2a:	/* RAW8 */
		set_field(&mix_cfg, 2, 0x3<<16);	/* Source is 8 bits per pixel */
		break;

	case 0x2b:	/* RAW10 */
		set_field(&mix_cfg, 1, 0x3<<16);	/* Source is 10 bits per pixel */
		break;

	case 0x2c:	/* RAW12 */
		set_field(&mix_cfg, 0, 0x3<<16);	/* Source is 12 bits per pixel */
		break;
	}

	/* FIXME: Should we set different data type for separate channels?
	 * If yes, we need to modify CSI driver to record the data type of
	 * separate channels.
	 */
	/* Set SOL_SYNCWORD field
	 * CH3_SOL_SYNCWORD, Bit[31:24]
	 * CH2_SOL_SYNCWORD, Bit[23:16]
	 * CH1_SOL_SYNCWORD, Bit[15:8]
	 * CH0_SOL_SYNCWORD, Bit[7:0]
	 */
	for (i = 0; i < priv->max_channels; i++) {
		/* Set the DT field of the DI for Virtual Channel x */
		set_field(&sol_syncword, dt, 0x3f<<(i*8));		/* Set CHx_SOL_SYNCWORD field */
	}

	dev_dbg(priv->dev, "%s, %d: mix_cfg: %08x, sol_syncword: %08x\n",
		 __func__, __LINE__, mix_cfg, sol_syncword);

	csi_writel(priv, MIPICSI_MIX_CFG, mix_cfg);
	csi_writel(priv, MIPICSI_SOL_SYNCWORD, sol_syncword);
}

#if defined(MIPI_CSI_BIST)
static void csi2_bist_control(struct csi2_dev *priv, u8 on)
{
	u32 val = 0;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	val = csi_readl(priv, MIPICSI_ENABLE);
	set_field(&val, on, 0x1<<16); 			/* Enable BIST function */
	csi_writel(priv, MIPICSI_ENABLE, val);
}

static void csi2_bist_config(struct csi2_dev *priv, unsigned int dt, u8 ch)
{
	u32 height = priv->mf.height;
	u32 width = priv->mf.width;
	u32 val;
	u8 mode = bist_mode;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	/* Use internal pattern to test MIPI-CSI */
	val = 0;
	if (dt == 0x2c) width = width/2;		/* One pixel is output twice for RAW12 */
	set_field(&val, height, 0xfff<<16);		/* Vertical size of internal pattern */
	set_field(&val, width, 0x1fff<<0);		/* Horizontal size of internal pattern */
	csi_writel(priv, MIPI_CH0_BIST_SIZE, val);
	csi_writel(priv, MIPI_CH1_BIST_SIZE, val);
	csi_writel(priv, MIPI_CH2_BIST_SIZE, val);
	csi_writel(priv, MIPI_CH3_BIST_SIZE, val);

	val = 0;
	if (dt == 0x2c) mode = 2;				/* Gray bar for RAW12 */
	val = csi_readl(priv, MIPICSI_ENABLE);
	set_field(&val, ch, 0x3<<24);			/* Select BIST pattern output to channel x */
	set_field(&val, mode, 0x3<<20);			/* Gray bar for RAW12 format */
	set_field(&val, 1, 0x1<<17);			/* Use internal clock for BIST */
	csi_writel(priv, MIPICSI_ENABLE, val);

	dev_info(priv->dev, "BIST mode: %d, BIST channel: %d \n",mode, ch);
}
#endif

static void csi2_reset_s2p_ff(struct csi2_dev *priv)
{
	u32 ana_cfg1;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	ana_cfg1 = csi_readl(priv, MIPI_ANALOG_CFG1);

	/* Reset Serial-to-parallel Flip-flop */
	set_field(&ana_cfg1, 0x1, 0x1<<0);		/* RSTS2P = 1: Reset mode */
	udelay(1);
	csi_writel(priv, MIPI_ANALOG_CFG1, ana_cfg1);
	udelay(1);
	set_field(&ana_cfg1, 0x0, 0x1<<0);		/* RSTS2P = 0: Normal mode (default) */
	csi_writel(priv, MIPI_ANALOG_CFG1, ana_cfg1);
}

static void csi2_init(struct csi2_dev *priv)
{
	u32 val;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	val = 0;
	set_field(&val, 0x1, 0x1<<15);		/* When detect EOF control word, generate EOF */
	set_field(&val, 0x1, 0x1<<8);		/* For bit sequence of a word, transfer MSB bit first */
	set_field(&val, 0x1, 0x1<<2);		/* Set PHY to normal mode */
	csi_writel(priv, MIPICSI_MIX_CFG, val);

	val = 0;
	set_field(&val, (0x3<<6|0x00), 0xff<<24);	/* Set CH3_SOF_SYNCOWRD field */
	set_field(&val, (0x2<<6|0x00), 0xff<<16);	/* Set CH2_SOF_SYNCOWRD field */
	set_field(&val, (0x1<<6|0x00), 0xff<<8);	/* Set CH1_SOF_SYNCOWRD field */
	set_field(&val, (0x0<<6|0x00), 0xff<<0);	/* Set CH0_SOF_SYNCOWRD field */
	csi_writel(priv, MIPICSI_SOF_SYNCWORD, val);

	val = 0;
	set_field(&val, (0x3<<6|0x1e), 0xff<<24);	/* Set CH3_SOL_SYNCOWRD field */
	set_field(&val, (0x2<<6|0x1e), 0xff<<16);	/* Set CH2_SOL_SYNCOWRD field */
	set_field(&val, (0x1<<6|0x1e), 0xff<<8);	/* Set CH1_SOL_SYNCOWRD field */
	set_field(&val, (0x0<<6|0x1e), 0xff<<0);	/* Set CH0_SOL_SYNCOWRD field */
	csi_writel(priv, MIPICSI_SOL_SYNCWORD, val);


	val = 0;
	set_field(&val, 0x1, 0xf<<4);		/* Enable data lane of LP mode circuit for data lane 0 */
	set_field(&val, 0x1, 0x1<<0);		/* Enable clock lane of LP mode circuit */
	csi_writel(priv, MIPI_ANALOG_CFG5, val);

	csi_writel(priv, MIPICSI_ECC_CFG, 0x110);
	csi_writel(priv, MIPI_ANALOG_CFG1, 0x1000);
	csi_writel(priv, MIPI_ANALOG_CFG1, 0x1001);
	udelay(1);
	csi_writel(priv, MIPI_ANALOG_CFG1, 0x1000);
	csi_writel(priv, MIPICSI_ENABLE, 0x1);	/* Enable MIPICSI, disable FSM reset */
}

static int csi2_start_receiver(struct csi2_dev *priv)
{
	const struct csi2_format *format;
	unsigned int lanes;
	int mbps, ret;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	dev_dbg(priv->dev, "Input size (%ux%u%c)\n",
		priv->mf.width, priv->mf.height,
		priv->mf.field == V4L2_FIELD_NONE ? 'p' : 'i');

	/* Code is validated in set_fmt. */
	format = csi2_code_to_fmt(priv->mf.code);

	/*
	 * Enable all supported CSI-2 channels with virtual channel and
	 * data type matching.
	 *
	 * NOTE: It's not possible to get individual datatype for each
	 *       source virtual channel. Once this is possible in V4L2
	 *       it should be used here.
	 */
	csi2_vc_config(priv);
#if defined(MIPI_CSI_BIST)
	csi2_bist_config(priv, format->datatype, bist_ch);
#endif

	/*
	 * Get the number of active data lanes inspecting the remote mbus
	 * configuration.
	 */
	ret = csi2_get_active_lanes(priv, &lanes);
	if (ret)
		return ret;

	mbps = csi2_calc_mbps(priv, format->bpp, lanes);
	if (mbps < 0)
		return mbps;

	/* Configure */
	csi2_dt_config(priv, format->datatype);

	/* Lanes are zero indexed. */
	csi2_lane_config(priv, lanes);

	/* Start */
	if (priv->info->init_phtw) {
		ret = priv->info->init_phtw(priv, mbps);
		if (ret)
			return ret;
	}

	if (priv->info->hsfreqrange) {
		ret = csi2_set_phypll(priv, mbps);
		if (ret)
			return ret;
	}

	ret = csi2_wait_phy_start(priv, lanes);
	if (ret)
		return ret;

	/* Run post PHY start initialization, if needed. */
	if (priv->info->phy_post_init) {
		ret = priv->info->phy_post_init(priv);
		if (ret)
			return ret;
	}

	/* Reset Serial-to-parallel Flip-flop */
	csi2_reset_s2p_ff(priv);

	return 0;
}

static int csi2_start(struct csi2_dev *priv)
{
	int ret;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	csi2_exit_standby(priv);

	ret = csi2_start_receiver(priv);
	if (ret) {
		csi2_enter_standby(priv);
		return ret;
	}

#if defined(MIPI_CSI_BIST)
	csi2_bist_control(priv, 1);

	return 0;
#elif defined(MIPI_CSI_XTOR)
	return 0;
#endif

	ret = v4l2_subdev_call(priv->remote, video, s_stream, 1);
	if (ret) {
		csi2_enter_standby(priv);
		return ret;
	}

	return 0;
}

static void csi2_stop(struct csi2_dev *priv)
{
	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	csi2_enter_standby(priv);
#if defined(MIPI_CSI_BIST)
	csi2_bist_control(priv, 0);
#else
	v4l2_subdev_call(priv->remote, video, s_stream, 0);
#endif

}

static int csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct csi2_dev *priv = sd_to_csi2(sd);
	int ret = 0;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	mutex_lock(&priv->lock);

#if !defined(MIPI_CSI_BIST)
	if (!priv->remote) {
		ret = -ENODEV;
		goto out;
	}
#endif

	if (enable && priv->stream_count == 0) {
		ret = csi2_start(priv);
		if (ret)
			goto out;
	} else if (!enable && priv->stream_count == 1) {
		csi2_stop(priv);
	}

	priv->stream_count += enable ? 1 : -1;
out:
	mutex_unlock(&priv->lock);

	return ret;
}

static int csi2_set_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct csi2_dev *priv = sd_to_csi2(sd);
	struct v4l2_mbus_framefmt *framefmt;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);
	dev_dbg(priv->dev, "%s, format->which: %d, format->pad: %d\n",
		__func__, format->which, format->pad);

	if (!csi2_code_to_fmt(format->format.code))
		format->format.code = csi2_formats[0].code;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		priv->mf = format->format;

		/* Propagate the format to sink pad */
		if (format->pad == 0)
		{
			int ret;
			struct v4l2_subdev_pad_config *pad_cfg;
			struct v4l2_subdev_format fmt = {
				.which = V4L2_SUBDEV_FORMAT_ACTIVE,
				.pad = priv->remote_pad,
				.format.width = format->format.width,
				.format.height = format->format.height,
				.format.code = format->format.code,
			};

			ret = v4l2_subdev_call(priv->remote, pad, set_fmt, pad_cfg, &fmt);
			if (ret < 0 && ret != -ENOIOCTLCMD)
				return ret;
		}

		dev_dbg(priv->dev, "%s, priv->mf.code: 0x%x %ux%u\n",
			__func__, priv->mf.code, priv->mf.width, priv->mf.height);
	} else {

		framefmt = v4l2_subdev_get_try_format(sd, cfg, 0);
		*framefmt = format->format;

		dev_dbg(priv->dev, "%s, framefmt->code: 0x%x %ux%u\n",
			__func__, framefmt->code, framefmt->width, framefmt->height);
	}

	return 0;
}

static int csi2_get_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct csi2_dev *priv = sd_to_csi2(sd);

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		format->format = priv->mf;
	else
		format->format = *v4l2_subdev_get_try_format(sd, cfg, 0);

	dev_dbg(priv->dev, "%s, code: 0x%x %ux%u\n", __func__,
		format->format.code, format->format.width, format->format.height);

	return 0;
}

static const struct v4l2_subdev_video_ops car_csi2_video_ops = {
	.s_stream = csi2_s_stream,
};

static const struct v4l2_subdev_pad_ops car_csi2_pad_ops = {
	.set_fmt = csi2_set_pad_format,
	.get_fmt = csi2_get_pad_format,
};

static const struct v4l2_subdev_ops car_csi2_subdev_ops = {
	.video	= &car_csi2_video_ops,
	.pad	= &car_csi2_pad_ops,
};

/* -----------------------------------------------------------------------------
 * Async handling and registration of subdevices and links.
 */

static int csi2_notify_bound(struct v4l2_async_notifier *notifier,
			      struct v4l2_subdev *subdev,
			      struct v4l2_async_subdev *asd)
{
	struct csi2_dev *priv = notifier_to_csi2(notifier);
	int pad;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	pad = media_entity_get_fwnode_pad(&subdev->entity, asd->match.fwnode,
					  MEDIA_PAD_FL_SOURCE);
	if (pad < 0) {
		dev_err(priv->dev, "Failed to find pad for %s\n", subdev->name);
		return pad;
	}

	priv->remote = subdev;
	priv->remote_pad = pad;

	dev_dbg(priv->dev, "Bound %s pad: %d\n", subdev->name, pad);

	return media_create_pad_link(&subdev->entity, pad,
				     &priv->subdev.entity, 0,
				     MEDIA_LNK_FL_ENABLED |
				     MEDIA_LNK_FL_IMMUTABLE);
}

static void csi2_notify_unbind(struct v4l2_async_notifier *notifier,
				struct v4l2_subdev *subdev,
				struct v4l2_async_subdev *asd)
{
	struct csi2_dev *priv = notifier_to_csi2(notifier);

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	priv->remote = NULL;

	dev_dbg(priv->dev, "Unbind %s\n", subdev->name);
}

static const struct v4l2_async_notifier_operations csi2_notify_ops = {
	.bound = csi2_notify_bound,
	.unbind = csi2_notify_unbind,
};

static int csi2_parse_v4l2(struct csi2_dev *priv,
			    struct v4l2_fwnode_endpoint *vep)
{
	char data_lanes[V4L2_FWNODE_CSI2_MAX_DATA_LANES * 2];
	unsigned int i;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);
	dev_dbg(priv->dev, "port: %d, id: %d\n", vep->base.port, vep->base.id);

	/* Only port 0 endpoint 0 is valid. */
	if (vep->base.port || vep->base.id)
		return -ENOTCONN;

	if (vep->bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(priv->dev, "Unsupported bus: %u\n", vep->bus_type);
		return -EINVAL;
	}

	priv->lanes = vep->bus.mipi_csi2.num_data_lanes;
	if (priv->lanes != 1 && priv->lanes != 2 && priv->lanes != 4) {
		dev_err(priv->dev, "Unsupported number of data-lanes: %u\n",
			priv->lanes);
		return -EINVAL;
	}

	for (i = 0; i < vep->bus.mipi_csi2.num_data_lanes; i++) {
		unsigned int lane = vep->bus.mipi_csi2.data_lanes[i];

		if (lane > 4) {
			dev_err(priv->dev, "Invalid position %u for data lane %u\n",
				lane, i);
			return -EINVAL;
		}

		data_lanes[i*2] = '0' + lane;
		data_lanes[i*2+1] = ' ';
	}

	data_lanes[i*2-1] = '\0';

	dev_dbg(priv->dev,
		"CSI-2 bus: clock lane <%u>, data lanes <%s>, flags 0x%08x\n",
		vep->bus.mipi_csi2.clock_lane, data_lanes,
		vep->bus.mipi_csi2.flags);

	for (i = 0; i < ARRAY_SIZE(priv->lane_swap); i++) {
		priv->lane_swap[i] = i < priv->lanes ?
			vep->bus.mipi_csi2.data_lanes[i] : i;

		/* Check for valid lane number. */
		if (priv->lane_swap[i] < 1 || priv->lane_swap[i] > 4) {
			dev_err(priv->dev, "data-lanes must be in 1-4 range\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int csi2_parse_dt(struct csi2_dev *priv)
{
	struct v4l2_async_subdev *asd;
	struct fwnode_handle *fwnode;
	struct device_node *ep;
	struct v4l2_fwnode_endpoint v4l2_ep = { .bus_type = 0 };
	u32 id;
	int ret;

	dev_dbg(priv->dev, "%s, %d\n", __func__, __LINE__);

	/* Make sure CSI id is present and sane */
	ret = of_property_read_u32(priv->dev->of_node, "sunplus,id", &id);
	if (ret) {
		dev_err(priv->dev, "%pOF: No sunplus,id property found\n",
			priv->dev->of_node);
		return ret;
	}

	if (id >= CSI_MAX_NUM) {
		dev_err(priv->dev, "%pOF: Invalid sunplus,id '%u'\n",
			priv->dev->of_node, id);
		ret = -EINVAL;
		return ret;
	}

	priv->id = id;

	dev_dbg(priv->dev, "%s, MIPI-CSI ID is %d\n", __func__, priv->id);

	/* Read virtual channel propoties */
	ret = of_property_read_u32(priv->dev->of_node, "max_channels", &priv->max_channels);
	if (ret) {
		dev_err(priv->dev, "Failed to get max_channels\n");
		return ret;
	}

	ret = of_property_read_u32(priv->dev->of_node, "num_channels", &priv->num_channels);
	if (ret) {
		dev_err(priv->dev, "Failed to get num_channels\n");
		return ret;
	}

	dev_dbg(priv->dev, "%s, max_channels: %d, num_channels: %d\n",
		__func__, priv->max_channels, priv->num_channels);

#if defined(MIPI_CSI_BIST)
	/* For BIST test, skip bounding a sensor */
	if (priv->num_channels == 2)
		priv->lanes = 2;		/* Set active lane number here */
	else if (priv->num_channels == 4)
		priv->lanes = 4;		/* Set active lane number here */
	else
		return -EINVAL;

	dev_dbg(priv->dev, "%s, Skip bounding a sensor\n", __func__);
	return ret;
#endif

	/* Get endpoint */
	ep = of_graph_get_endpoint_by_regs(priv->dev->of_node, 0, 0);
	if (!ep) {
		dev_err(priv->dev, "Not connected to subdevice\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(ep), &v4l2_ep);
	if (ret) {
		dev_err(priv->dev, "Could not parse v4l2 endpoint\n");
		of_node_put(ep);
		return -EINVAL;
	}

	dev_dbg(priv->dev, "%s, port: %d, id: %d\n", __func__, v4l2_ep.base.port, v4l2_ep.base.id);
	dev_dbg(priv->dev, "%s, bus_type: %d\n", __func__, v4l2_ep.bus_type);

	ret = csi2_parse_v4l2(priv, &v4l2_ep);
	if (ret) {
		of_node_put(ep);
		return ret;
	}

	fwnode = fwnode_graph_get_remote_endpoint(of_fwnode_handle(ep));
	of_node_put(ep);

	dev_dbg(priv->dev, "Found '%pOF'\n", to_of_node(fwnode));

	v4l2_async_notifier_init(&priv->notifier);
	priv->notifier.ops = &csi2_notify_ops;

	asd = v4l2_async_notifier_add_fwnode_subdev(&priv->notifier, fwnode,
						    sizeof(*asd));
	fwnode_handle_put(fwnode);
	if (IS_ERR(asd)) {
		dev_err(priv->dev, "Failed to add subdev notifier\n");
		return PTR_ERR(asd);
	}

	ret = v4l2_async_subdev_notifier_register(&priv->subdev,
						  &priv->notifier);
	if (ret) {
		dev_err(priv->dev, "Failed to register notifier\n");
		v4l2_async_notifier_cleanup(&priv->notifier);
	}

	return ret;
}

/* -----------------------------------------------------------------------------
 * PHTW initialization sequences.
 *
 * NOTE: Magic values are from the datasheet and lack documentation.
 */
// These functions are only for Renesas R-Car chips.

static int csi2_init_phtw(struct csi2_dev *priv, unsigned int mbps)
{
	return 0;
}

static int csi2_phy_post_init(struct csi2_dev *priv)
{
	return 0;
}

/* -----------------------------------------------------------------------------
 * sysfs.
 */
#ifdef MIPI_CSI_BIST
static ssize_t csi2_bist_mode_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bist_mode);
}

static ssize_t csi2_bist_mode_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	bist_mode = simple_strtoul(buf, NULL, 16);

	return count;
}

static DEVICE_ATTR(bist_mode, S_IWUSR|S_IRUGO, csi2_bist_mode_show, csi2_bist_mode_store);

static ssize_t csi2_bist_ch_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bist_ch);
}

static ssize_t csi2_bist_ch_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	bist_ch = simple_strtoul(buf, NULL, 16);

	return count;
}

static DEVICE_ATTR(bist_ch, S_IWUSR|S_IRUGO, csi2_bist_ch_show, csi2_bist_ch_store);

static struct attribute *csi2_attributes[] = {
	&dev_attr_bist_mode.attr,
	&dev_attr_bist_ch.attr,
	NULL,
};

static struct attribute_group csi2_attribute_group = {
	.attrs = csi2_attributes,
};
#endif

/* -----------------------------------------------------------------------------
 * Platform Device Driver.
 */

static const struct media_entity_operations csi2_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int csi2_probe_resources(struct csi2_dev *priv,
				 struct platform_device *pdev)
{
	struct resource *res;

	dev_dbg(&pdev->dev, "%s, %d\n", __func__, __LINE__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	dev_dbg(priv->dev, "%s, res->start: 0x%08llx, name: %s\n",
		__func__, res->start, res->name);

#if 1 /* CCHo: Get MIPICSI23_SEL G164 resource */
	/* MIPI-CSI2 and MIPI-CSI3 ports share VI23-CSIIW2 and VI23-CSIIW3. 
	 * They need to get resource MIPICSI23_SEL (G164) to select the virtual
	 * channel source of VI23-CSIIW2 AND VI23-CSIIW3.
	 */
	if ((res->start == 0xF8005380) || (res->start == 0xF8005400)) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		priv->base2 = devm_ioremap(&pdev->dev, res->start, (res->end - res->start + 1));
		if (IS_ERR(priv->base2))
			return PTR_ERR(priv->base2);

	#if 1 /* CCHo: For debugging*/
		dev_info(priv->dev, "%s, res->start: 0x%08llx, name: %s\n",
			__func__, res->start, res->name);
	#else
		dev_dbg(priv->dev, "%s, res->start: 0x%08llx, name: %s\n",
			__func__, res->start, res->name);
	#endif
	} else {
		priv->base2 = NULL;
	}
#endif

	priv->clk = devm_clk_get(&pdev->dev, "clk_mipicsi");
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

	priv->rstc = devm_reset_control_get(&pdev->dev, NULL);

	return PTR_ERR_OR_ZERO(priv->rstc);
}

static const struct csi2_info sp_csi2_info_sp7350 = {
	.init_phtw = csi2_init_phtw,
	.phy_post_init = csi2_phy_post_init,
	.num_channels = 4,
};

static const struct of_device_id sp_csi2_of_table[] = {
	{
		.compatible = "sunplus,sp7350-mipicsi-rx",
		.data = &sp_csi2_info_sp7350,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, sp_csi2_of_table);

static int sp_csi2_probe(struct platform_device *pdev)
{
	struct csi2_dev *priv;
	unsigned int i;
	int ret;

	dev_dbg(&pdev->dev, "%s, %d\n", __func__, __LINE__);

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->info = of_device_get_match_data(&pdev->dev);
	priv->dev = &pdev->dev;

	mutex_init(&priv->lock);
	priv->stream_count = 0;

	ret = csi2_probe_resources(priv, pdev);
	if (ret) {
		dev_err(priv->dev, "Failed to get resources\n");
		return ret;
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(priv->dev, "Failed to enable clock!\n");
		return ret;
	}

	ret = reset_control_deassert(priv->rstc);
	if (ret) {
		dev_err(priv->dev, "Failed to deassert reset controller!\n");
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	ret = csi2_parse_dt(priv);
	if (ret)
		return ret;

	priv->subdev.owner = THIS_MODULE;
	priv->subdev.dev = &pdev->dev;
	v4l2_subdev_init(&priv->subdev, &car_csi2_subdev_ops);
	v4l2_set_subdevdata(&priv->subdev, &pdev->dev);
	snprintf(priv->subdev.name, V4L2_SUBDEV_NAME_SIZE, "%s %s",
		 KBUILD_MODNAME, dev_name(&pdev->dev));
	priv->subdev.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;

	priv->subdev.entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	priv->subdev.entity.ops = &csi2_entity_ops;

	priv->pads[CSI2_SINK].flags = MEDIA_PAD_FL_SINK;
	for (i = CSI2_SOURCE_VC0; i < NR_OF_CSI2_PAD; i++)
		priv->pads[i].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&priv->subdev.entity, NR_OF_CSI2_PAD,
				     priv->pads);
	if (ret)
		goto error;

	csi2_init(priv);

#ifdef MIPI_CSI_BIST
	/* Add the device attribute group into sysfs */
	ret = sysfs_create_group(&pdev->dev.kobj, &csi2_attribute_group);
	if (ret != 0) {
		dev_err(priv->dev, "Failed to create sysfs files!\n");
		goto error;
	}
#endif

#ifdef CONFIG_PM_RUNTIME_MIPICSI
	pm_runtime_set_autosuspend_delay(&pdev->dev, 5000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
#endif

	ret = v4l2_async_register_subdev(&priv->subdev);
	if (ret < 0)
		goto error;

	dev_info(priv->dev, "%d lanes found\n", priv->lanes);

	return 0;

error:
	v4l2_async_notifier_unregister(&priv->notifier);
	v4l2_async_notifier_cleanup(&priv->notifier);

	return ret;
}

static int sp_csi2_remove(struct platform_device *pdev)
{
	struct csi2_dev *priv = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s, %d\n", __func__, __LINE__);

	v4l2_async_notifier_unregister(&priv->notifier);
	v4l2_async_notifier_cleanup(&priv->notifier);
	v4l2_async_unregister_subdev(&priv->subdev);

#ifdef CONFIG_PM_RUNTIME_MIPICSI
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
#endif

	return 0;
}

static int sp_csi2_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct csi2_dev *priv = platform_get_drvdata(pdev);

	clk_disable(priv->clk);

	return 0;
}

static int sp_csi2_resume(struct platform_device *pdev)
{
	struct csi2_dev *priv = platform_get_drvdata(pdev);
	int ret;

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		dev_err(priv->dev, "Failed to enable clock!\n");

	return 0;
}

#ifdef CONFIG_PM_RUNTIME_MIPICSI
static int sp_csi2_runtime_suspend(struct device *dev)
{
	struct csi2_dev *priv = dev_get_drvdata(dev);

	clk_disable(priv->clk);

	return 0;
}

static int sp_csi2_runtime_resume(struct device *dev)
{
	struct csi2_dev *priv = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		dev_err(priv->dev, "Failed to enable clock!\n");

	return 0;
}

static const struct dev_pm_ops sp_csi2_pm_ops = {
	.runtime_suspend = sp_csi2_runtime_suspend,
	.runtime_resume = sp_csi2_runtime_resume,
};
#endif

static struct platform_driver sp_csi2_pdrv = {
	.remove	= sp_csi2_remove,
	.probe	= sp_csi2_probe,
	.driver	= {
		.name	= "sp-csi2",
		.of_match_table	= sp_csi2_of_table,
#ifdef CONFIG_PM_RUNTIME_MIPICSI
		.pm = &sp_csi2_pm_ops,
#endif
	},
	.suspend = sp_csi2_suspend,
	.resume = sp_csi2_resume,
};

module_platform_driver(sp_csi2_pdrv);

MODULE_AUTHOR("Cheng Chung Ho <cc.ho@sunplus.com>");
MODULE_DESCRIPTION("Sunplus MIPI CSI-2 receiver driver");
MODULE_LICENSE("GPL");
