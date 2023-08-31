// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for Sony IMX219 cameras.
 * Copyright (C) 2019, Raspberry Pi (Trading) Ltd
 *
 * Based on Sony imx258 camera driver
 * Copyright (C) 2018 Intel Corporation
 *
 * DT / fwnode changes, and regulator / GPIO control taken from imx214 driver
 * Copyright 2018 Qtechnology A/S
 *
 * Flip handling taken from the Sony IMX319 driver.
 * Copyright (C) 2018 Intel Corporation
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>
#include <asm/unaligned.h>

/* Compiler switches for TP2815 chip */
//#define TP2815_STREAM_ALWAYS_ON				// Streaming is always on after running
//#define TP2815_TEST_PATTERN_LOW_PIXEL_RATE	// Output test pattern at low pixel rate 
//#define TP2815_VC_SEQUENCE_0123					// VC sequence is VC0, VC1, VC2, VC3
//#define TP2815_REGULATOR_CONTROL				// Original requlator control

/* Constants for TP2815 chip */
#define TP2815_MIPI_CSI_HS_CLOCK_RATE_MHZ	74


#define IMX219_REG_VALUE_08BIT		1
#define IMX219_REG_VALUE_16BIT		2

#define IMX219_REG_MODE_SELECT		0x0100
#define IMX219_MODE_STANDBY		0x00
#define IMX219_MODE_STREAMING		0x01

/* Chip ID */
#define IMX219_REG_CHIP_ID_H	0xfe
#define IMX219_REG_CHIP_ID_L	0xff
#define IMX219_CHIP_ID			0x2855

/* External clock frequency is 27.0M */
#define IMX219_XCLK_FREQ		27000000

/* Pixel rate is fixed at 182.4M for all the modes */
#define IMX219_PIXEL_RATE		182400000

#define IMX219_DEFAULT_LINK_FREQ	456000000

/* V_TIMING internal */
#define IMX219_REG_VTS			0x0160
#define IMX219_VTS_15FPS		0x0dc6
#define IMX219_VTS_30FPS_1080P		0x06e3
#define IMX219_VTS_30FPS_BINNED		0x06e3
#define IMX219_VTS_30FPS_640x480	0x06e3
#define IMX219_VTS_MAX			0xffff

#define IMX219_VBLANK_MIN		4

/*Frame Length Line*/
#define IMX219_FLL_MIN			0x08a6
#define IMX219_FLL_MAX			0xffff
#define IMX219_FLL_STEP			1
#define IMX219_FLL_DEFAULT		0x0c98

/* HBLANK control - read only */
#define IMX219_PPL_DEFAULT		3448

/* Exposure control */
#define IMX219_REG_EXPOSURE		0x015a
#define IMX219_EXPOSURE_MIN		4
#define IMX219_EXPOSURE_STEP		1
#define IMX219_EXPOSURE_DEFAULT		0x640
#define IMX219_EXPOSURE_MAX		65535

/* Analog gain control */
#define IMX219_REG_ANALOG_GAIN		0x0157
#define IMX219_ANA_GAIN_MIN		0
#define IMX219_ANA_GAIN_MAX		232
#define IMX219_ANA_GAIN_STEP		1
#define IMX219_ANA_GAIN_DEFAULT		0x0

/* Digital gain control */
#define IMX219_REG_DIGITAL_GAIN		0x0158
#define IMX219_DGTL_GAIN_MIN		0x0100
#define IMX219_DGTL_GAIN_MAX		0x0fff
#define IMX219_DGTL_GAIN_DEFAULT	0x0100
#define IMX219_DGTL_GAIN_STEP		1

#define IMX219_REG_ORIENTATION		0x0172

/* Test Pattern Control */
#define IMX219_REG_TEST_PATTERN		0x0600
#define IMX219_TEST_PATTERN_DISABLE	0
#define IMX219_TEST_PATTERN_SOLID_COLOR	1
#define IMX219_TEST_PATTERN_COLOR_BARS	2
#define IMX219_TEST_PATTERN_GREY_COLOR	3
#define IMX219_TEST_PATTERN_PN9		4

/* Test pattern colour components */
#define IMX219_REG_TESTP_RED		0x0602
#define IMX219_REG_TESTP_GREENR		0x0604
#define IMX219_REG_TESTP_BLUE		0x0606
#define IMX219_REG_TESTP_GREENB		0x0608
#define IMX219_TESTP_COLOUR_MIN		0
#define IMX219_TESTP_COLOUR_MAX		1 //0x03ff
#define IMX219_TESTP_COLOUR_STEP	1
#define IMX219_TESTP_RED_DEFAULT	IMX219_TESTP_COLOUR_MAX
#define IMX219_TESTP_GREENR_DEFAULT	0
#define IMX219_TESTP_BLUE_DEFAULT	0
#define IMX219_TESTP_GREENB_DEFAULT	0

/* IMX219 native and active pixel array size. */
#define IMX219_NATIVE_WIDTH		3296U
#define IMX219_NATIVE_HEIGHT		2480U
#define IMX219_PIXEL_ARRAY_LEFT		8U
#define IMX219_PIXEL_ARRAY_TOP		8U
#define IMX219_PIXEL_ARRAY_WIDTH	3280U
#define IMX219_PIXEL_ARRAY_HEIGHT	2464U

// channel
enum {
	CH_1 = 0,
	CH_2 = 1,
	CH_3 = 2,
	CH_4 = 3,
	CH_ALL = 4,
	MIPI_PAGE = 8,
};
// input_std
enum {
	STD_TVI, // TVI
	STD_HDA, // AHD
};
// video_format
enum {
	HD25,	// 720p25
	HD30,	// 720p30
	FHD25,	// 1080p25
	FHD30,	// 1080p30
	FHD50,	// 1080p50
	FHD60,	// 1080p60
	HD50,	// 720p50
	HD60,	// 720p60
	HHD30,	// half 720p60
};
// video_output
enum {
	/* 4 data lanes */
	MIPI_4CH4LANE_74M,	// for test pattern output
	MIPI_4CH4LANE_148M,	// for test pattern output
	MIPI_4CH4LANE_297M,	// up to 4x720p25/30
	MIPI_4CH4LANE_594M,	// up to 4x1080p25/30
	MIPI_1CH4LANE_74M,	// for test pattern output
	MIPI_1CH4LANE_148M,	// for test pattern output
	MIPI_1CH4LANE_297M,
	MIPI_1CH4LANE_594M,

	/* 2 data lanes */
	MIPI_4CH2LANE_594M, // up to 4x720p25/30
	MIPI_2CH2LANE_594M,
	MIPI_1CH2LANE_594M,

	/* 1 data lane */
	MIPI_4CH1LANE_594M, // up to 4x720p25/30
	MIPI_2CH1LANE_594M,
	MIPI_1CH1LANE_594M,
};

// ioctl cmd
enum {
	TP2815_READ_REG = 0x0,
	TP2815_WRITE_REG = 0x1,
	TP2815_SET_CHANNEL = 0x33,
	TP2815_SET_OUTPUT = 0x34,
	TP2815_GET_DIV_ID,
	TP2815_CHANNEL_INFO,
};

struct imx219_reg {
	u16 address;
	u8 val;
};

struct imx219_reg_list {
	unsigned int num_of_regs;
	const struct imx219_reg *regs;
};

/* Mode : resolution and related config&values */
struct imx219_mode {
	/* Frame width */
	unsigned int width;
	/* Frame height */
	unsigned int height;

	/* Analog crop rectangle. */
	struct v4l2_rect crop;

	/* V-timing */
	unsigned int vts_def;

	/* Default register values */
	struct imx219_reg_list reg_list;
};

/*
 * Register sets lifted off the i2C interface from the Raspberry Pi firmware
 * driver.
 * 3280x2464 = mode 2, 1920x1080 = mode 1, 1640x1232 = mode 4, 640x480 = mode 7.
 */
static const struct imx219_reg mode_3280x2464_regs[] = {
	{0x0100, 0x00},
	{0x30eb, 0x0c},
	{0x30eb, 0x05},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0c},
	{0x0167, 0xcf},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016a, 0x09},
	{0x016b, 0x9f},
	{0x016c, 0x0c},
	{0x016d, 0xd0},
	{0x016e, 0x09},
	{0x016f, 0xa0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x0c},
	{0x0625, 0xd0},
	{0x0626, 0x09},
	{0x0627, 0xa0},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0x0162, 0x0d},
	{0x0163, 0x78},
};

static const struct imx219_reg mode_1920_1080_regs[] = {
	{0x0100, 0x00},
	{0x30eb, 0x05},
	{0x30eb, 0x0c},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0162, 0x0d},
	{0x0163, 0x78},
	{0x0164, 0x02},
	{0x0165, 0xa8},
	{0x0166, 0x0a},
	{0x0167, 0x27},
	{0x0168, 0x02},
	{0x0169, 0xb4},
	{0x016a, 0x06},
	{0x016b, 0xeb},
	{0x016c, 0x07},
	{0x016d, 0x80},
	{0x016e, 0x04},
	{0x016f, 0x38},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x07},
	{0x0625, 0x80},
	{0x0626, 0x04},
	{0x0627, 0x38},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0x0162, 0x0d},
	{0x0163, 0x78},
};

static const struct imx219_reg mode_1640_1232_regs[] = {
	{0x0100, 0x00},
	{0x30eb, 0x0c},
	{0x30eb, 0x05},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0c},
	{0x0167, 0xcf},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016a, 0x09},
	{0x016b, 0x9f},
	{0x016c, 0x06},
	{0x016d, 0x68},
	{0x016e, 0x04},
	{0x016f, 0xd0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x01},
	{0x0175, 0x01},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x06},
	{0x0625, 0x68},
	{0x0626, 0x04},
	{0x0627, 0xd0},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0x0162, 0x0d},
	{0x0163, 0x78},
};

static const struct imx219_reg mode_640_480_regs[] = {
	{0x0100, 0x00},
	{0x30eb, 0x05},
	{0x30eb, 0x0c},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0162, 0x0d},
	{0x0163, 0x78},
	{0x0164, 0x03},
	{0x0165, 0xe8},
	{0x0166, 0x08},
	{0x0167, 0xe7},
	{0x0168, 0x02},
	{0x0169, 0xf0},
	{0x016a, 0x06},
	{0x016b, 0xaf},
	{0x016c, 0x02},
	{0x016d, 0x80},
	{0x016e, 0x01},
	{0x016f, 0xe0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x03},
	{0x0175, 0x03},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x06},
	{0x0625, 0x68},
	{0x0626, 0x04},
	{0x0627, 0xd0},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
};

static const struct imx219_reg raw8_framefmt_regs[] = {
	{0x018c, 0x08},
	{0x018d, 0x08},
	{0x0309, 0x08},
};

static const struct imx219_reg raw10_framefmt_regs[] = {
	{0x018c, 0x0a},
	{0x018d, 0x0a},
	{0x0309, 0x0a},
};

#if 1 // CCHo: TP2815
static const char * const imx219_test_pattern_menu[] = {
	"Disabled",
	"Solid Color",
};

static const int imx219_test_pattern_val[] = {
	IMX219_TEST_PATTERN_DISABLE,
	IMX219_TEST_PATTERN_SOLID_COLOR,
};
#else
static const char * const imx219_test_pattern_menu[] = {
	"Disabled",
	"Color Bars",
	"Solid Color",
	"Grey Color Bars",
	"PN9"
};

static const int imx219_test_pattern_val[] = {
	IMX219_TEST_PATTERN_DISABLE,
	IMX219_TEST_PATTERN_COLOR_BARS,
	IMX219_TEST_PATTERN_SOLID_COLOR,
	IMX219_TEST_PATTERN_GREY_COLOR,
	IMX219_TEST_PATTERN_PN9,
};
#endif

/* regulator supplies */
static const char * const imx219_supply_name[] = {
	/* Supplies can be enabled in any order */
	"VANA",  /* Analog (2.8V) supply */
	"VDIG",  /* Digital Core (1.8V) supply */
	"VDDL",  /* IF (1.2V) supply */
};

#define IMX219_NUM_SUPPLIES ARRAY_SIZE(imx219_supply_name)

/*
 * The supported formats.
 * This table MUST contain 4 entries per format, to cover the various flip
 * combinations in the order
 * - no flip
 * - h flip
 * - v flip
 * - h&v flips
 */
#if 1 /* CCHo */
static const u32 codes[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
	MEDIA_BUS_FMT_VYUY8_2X8,
	MEDIA_BUS_FMT_YUYV8_2X8,
	MEDIA_BUS_FMT_YVYU8_2X8,
};
#else
static const u32 codes[] = {
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SBGGR10_1X10,

	MEDIA_BUS_FMT_SRGGB8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SBGGR8_1X8,
};
#endif

/*
 * Initialisation delay between XCLR low->high and the moment when the sensor
 * can start capture (i.e. can leave software stanby) must be not less than:
 *   t4 + max(t5, t6 + <time to initialize the sensor register over I2C>)
 * where
 *   t4 is fixed, and is max 200uS,
 *   t5 is fixed, and is 6000uS,
 *   t6 depends on the sensor external clock, and is max 32000 clock periods.
 * As per sensor datasheet, the external clock must be from 6MHz to 27MHz.
 * So for any acceptable external clock t6 is always within the range of
 * 1185 to 5333 uS, and is always less than t5.
 * For this reason this is always safe to wait (t4 + t5) = 6200 uS, then
 * initialize the sensor over I2C, and then exit the software standby.
 *
 * This start-up time can be optimized a bit more, if we start the writes
 * over I2C after (t4+t6), but before (t4+t5) expires. But then sensor
 * initialization over I2C may complete before (t4+t5) expires, and we must
 * ensure that capture is not started before (t4+t5).
 *
 * This delay doesn't account for the power supply startup time. If needed,
 * this should be taken care of via the regulator framework. E.g. in the
 * case of DT for regulator-fixed one should define the startup-delay-us
 * property.
 */
#define IMX219_XCLR_MIN_DELAY_US	6200
#define IMX219_XCLR_DELAY_RANGE_US	1000

/* Mode configs */
#if 1 /* CCHo */
static const struct imx219_mode supported_modes[] = {
	#if 0 // CCHo: 1080P has now output
	{
		/* 1080P 30fps cropped */
		.width = 1920,
		.height = 1080,
		.crop = {
			.left = 688,
			.top = 700,
			.width = 1920,
			.height = 1080
		},
		.vts_def = IMX219_VTS_30FPS_1080P,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1920_1080_regs),
			.regs = mode_1920_1080_regs,
		},
	},
	#endif
	{
		/* 720P 30fps mode */
		.width = 1280,
		.height = 720,
		.crop = {
			.left = 0,
			.top = 0,
			.width = 1280,
			.height = 720
		},
		.vts_def = IMX219_VTS_30FPS_640x480,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_640_480_regs),
			.regs = mode_640_480_regs,
		},
	},
	#if defined(TP2815_TEST_PATTERN_LOW_PIXEL_RATE)
	{
		/* Half 720P 60fps mode, 148MHz.
		 * This only for test pattern output.
		 */
		.width = 640,
		.height = 720,
		.crop = {
			.left = 0,
			.top = 0,
			.width = 640,
			.height = 720
		},
		.vts_def = IMX219_VTS_30FPS_640x480,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_640_480_regs),
			.regs = mode_640_480_regs,
		},
	},
	#endif
};
#else
static const struct imx219_mode supported_modes[] = {
	{
		/* 8MPix 15fps mode */
		.width = 3280,
		.height = 2464,
		.crop = {
			.left = IMX219_PIXEL_ARRAY_LEFT,
			.top = IMX219_PIXEL_ARRAY_TOP,
			.width = 3280,
			.height = 2464
		},
		.vts_def = IMX219_VTS_15FPS,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_3280x2464_regs),
			.regs = mode_3280x2464_regs,
		},
	},
	{
		/* 1080P 30fps cropped */
		.width = 1920,
		.height = 1080,
		.crop = {
			.left = 688,
			.top = 700,
			.width = 1920,
			.height = 1080
		},
		.vts_def = IMX219_VTS_30FPS_1080P,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1920_1080_regs),
			.regs = mode_1920_1080_regs,
		},
	},
	{
		/* 2x2 binned 30fps mode */
		.width = 1640,
		.height = 1232,
		.crop = {
			.left = IMX219_PIXEL_ARRAY_LEFT,
			.top = IMX219_PIXEL_ARRAY_TOP,
			.width = 3280,
			.height = 2464
		},
		.vts_def = IMX219_VTS_30FPS_BINNED,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1640_1232_regs),
			.regs = mode_1640_1232_regs,
		},
	},
	{
		/* 640x480 30fps mode */
		.width = 640,
		.height = 480,
		.crop = {
			.left = 1008,
			.top = 760,
			.width = 1280,
			.height = 960
		},
		.vts_def = IMX219_VTS_30FPS_640x480,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_640_480_regs),
			.regs = mode_640_480_regs,
		},
	},
};
#endif

struct imx219 {
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct v4l2_mbus_framefmt fmt;

	struct clk *xclk; /* system clock to IMX219 */
	u32 xclk_freq;

	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[IMX219_NUM_SUPPLIES];

	struct v4l2_ctrl_handler ctrl_handler;

	/* V4L2 Controls */
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;

	/* Current mode */
	const struct imx219_mode *mode;

	/* MIPI-CSI2 bus info */
	struct v4l2_fwnode_bus_mipi_csi2 bus;
	enum v4l2_mbus_type bus_type;

	/* TP2815 input related properties */
	u32 input_ch;
	u32 input_std;

	/*
	 * Mutex for serialized access:
	 * Protect sensor module set pad format and start/stop streaming safely.
	 */
	struct mutex mutex;

	/* Streaming on/off */
	bool streaming;
};

static inline struct imx219 *to_imx219(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct imx219, sd);
}

/* Read registers up to 2 at a time */
static int imx219_read_reg(struct imx219 *imx219, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	struct i2c_msg msgs[2];
	u8 addr_buf[1] = { reg & 0xff };
	u8 data_buf[4] = { 0, };
	int ret;

	if (len > 4)
		return -EINVAL;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_be32(data_buf);

	return 0;
}

/* Write registers up to 2 at a time */
static int imx219_write_reg(struct imx219 *imx219, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	u8 buf[5];

	if (len > 4)
		return -EINVAL;

	buf[0] = reg & 0xff;
	put_unaligned_be32(val << (8 * (4 - len)), buf + 1);
	if (i2c_master_send(client, buf, len + 1) != len + 1)
		return -EIO;

	return 0;
}

#if 1 /* CCHo */
// Blank
#else
/* Write a list of registers */
static int imx219_write_regs(struct imx219 *imx219,
			     const struct imx219_reg *regs, u32 len)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx219_write_reg(imx219, regs[i].address, 1, regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
					    "Failed to write reg 0x%4.4x. error = %d\n",
					    regs[i].address, ret);

			return ret;
		}
	}

	return 0;
}
#endif

/* Get bayer order based on flip setting. */
static u32 imx219_get_format_code(struct imx219 *imx219, u32 code)
{
	unsigned int i;

	lockdep_assert_held(&imx219->mutex);

	for (i = 0; i < ARRAY_SIZE(codes); i++)
		if (codes[i] == code)
			break;

	if (i >= ARRAY_SIZE(codes))
		i = 0;

	i = (i & ~3) | (imx219->vflip->val ? 2 : 0) |
	    (imx219->hflip->val ? 1 : 0);

	return codes[i];
}

void tp2815_decoder_reg_cfg(struct imx219 *imx219, u32 ch, u32 fmt, u32 std)
{
	const unsigned char SYS_MODE[5] = { 0x01, 0x02, 0x04, 0x08, 0x0f };
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	u32 tmp;

	dev_dbg(&client->dev, "%s, %d, ch: %u, fmt: %u, std: %u\n", __func__, __LINE__, ch, fmt, std); // CCHo addied for debugging

	// 13 B
	imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, ch);
	imx219_write_reg(imx219, 0x45, IMX219_REG_VALUE_08BIT, 0x01);
	// default value
	imx219_write_reg(imx219, 0x06, IMX219_REG_VALUE_08BIT, 0x12);
	// EQ2 Control
	imx219_write_reg(imx219, 0x07, IMX219_REG_VALUE_08BIT, 0xc0);
	// EQ2 Control
	imx219_write_reg(imx219, 0x0b, IMX219_REG_VALUE_08BIT, 0xc0);
	// Comb Filter and SD Format control
	imx219_write_reg(imx219, 0x0d, IMX219_REG_VALUE_08BIT, 0x50);
	// Clamping GainControl
	imx219_write_reg(imx219, 0x21, IMX219_REG_VALUE_08BIT, 0x84);
	// Sync Amplitude AGCControl
	imx219_write_reg(imx219, 0x22, IMX219_REG_VALUE_08BIT, 0x36);
	// Clamping LevelContro
	imx219_write_reg(imx219, 0x23, IMX219_REG_VALUE_08BIT, 0x3c);
	// Color Killer ThresholdControl
	imx219_write_reg(imx219, 0x2b, IMX219_REG_VALUE_08BIT, 0x60);
	// Color PLLContro
	imx219_write_reg(imx219, 0x2c, IMX219_REG_VALUE_08BIT, 0x0a);
	// Color GainReferenc
	imx219_write_reg(imx219, 0x2e, IMX219_REG_VALUE_08BIT, 0x70);

	// 0xF5 - System Clocl Control
	// b3 SYSCLK4MD
	//   0 = VIN4 HDTV Decoder system clock is 148.5MHz
	//   1 = VIN4 HDTV Decoder system clock is 74.25MHz
	// b2 SYSCLK3MD
	//   0 = VIN3 HDTV Decoder system clock is 148.5MHz
	//   1 = VIN3 HDTV Decoder system clock is 74.25MHz
	// b1 SYSCLK2MD
	//   0 = VIN2 HDTV Decoder system clock is 148.5MHz
	//   1 = VIN2 HDTV Decoder system clock is 74.25MHz
	// b0 SYSCLK1MD
	//   0 = VIN1 HDTV Decoder system clock is 148.5MHz
	//   1 = VIN1 HDTV Decoder system clock is 74.25MHz
	imx219_read_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, &tmp);

	dev_dbg(&client->dev, "%s, %d, tmp: 0x%02X\n", __func__, __LINE__, tmp); // CCHo addied for debugging

	// 20 B
	if (fmt == HD25) {
		tmp |= SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x42);
		// EQ2 Control
		//  imx219_write_reg(imx219, 0x07, IMX219_REG_VALUE_08BIT, 0xc0);
		// EQ2 Control
		//  imx219_write_reg(imx219, 0x0b, IMX219_REG_VALUE_08BIT, 0xc0);
		imx219_write_reg(imx219, 0x0c, IMX219_REG_VALUE_08BIT, 0x13);
		// Comb Filter and SD Format control
		//  imx219_write_reg(imx219, 0x0d, IMX219_REG_VALUE_08BIT, 0x50);

		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x13);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0x15);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x19);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0xd0);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x25);
		// 1280*720, 25fps
		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x07);
		// 1280*720, 25fps
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0xbc);

		imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x30);
		// Clamping GainControl
		//  imx219_write_reg(imx219, 0x21, IMX219_REG_VALUE_08BIT, 0x84);
		// Sync Amplitude AGCControl
		//  imx219_write_reg(imx219, 0x22, IMX219_REG_VALUE_08BIT, 0x36);
		// Clamping LevelControl
		//  imx219_write_reg(imx219, 0x23, IMX219_REG_VALUE_08BIT, 0x3c);
		// Color Killer ThresholdControl
		//  imx219_write_reg(imx219, 0x2b, IMX219_REG_VALUE_08BIT, 0x60);
		imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x30);
		// Color PLLContro
		//  imx219_write_reg(imx219, 0x2c, IMX219_REG_VALUE_08BIT, 0x0a);
		// Color GainReferenc
		//  imx219_write_reg(imx219, 0x2e, IMX219_REG_VALUE_08BIT, 0x70);

		imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x48);
		imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0xbb);
		imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0x2e);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x90);

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x25);
		imx219_write_reg(imx219, 0x38, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x39, IMX219_REG_VALUE_08BIT, 0x18);

		// 13B
		if (std == STD_HDA) {
			imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x46);

			imx219_write_reg(imx219, 0x0d, IMX219_REG_VALUE_08BIT, 0x71);
			// update v-dlay from 19 to 1b for ahd 720 25/30.
			imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x1b);

			imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x40);
			imx219_write_reg(imx219, 0x21, IMX219_REG_VALUE_08BIT, 0x46);

			imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0xfe);
			imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x01);

			imx219_write_reg(imx219, 0x2c, IMX219_REG_VALUE_08BIT, 0x3a);
			imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x5a);
			imx219_write_reg(imx219, 0x2e, IMX219_REG_VALUE_08BIT, 0x40);

			imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x9e);
			imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0x20);
			imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0x10);
			imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x90);
		}
	}
	// 19B
	else if (fmt == HD30) {
		tmp |= SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x42);
		// EQ2 Control
		//  imx219_write_reg(imx219, 0x07, IMX219_REG_VALUE_08BIT, 0xc0);
		// EQ2 Control
		// imx219_write_reg(imx219, 0x0b, IMX219_REG_VALUE_08BIT, 0xc0);
		imx219_write_reg(imx219, 0x0c, IMX219_REG_VALUE_08BIT, 0x13);
		// Comb Filter and SD Format control
		//  imx219_write_reg(imx219, 0x0d, IMX219_REG_VALUE_08BIT, 0x50);

		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x13);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0x15);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x19);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0xd0);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x25);
		// 1280*720, 30fps
		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x06);
		// 1280*720, 30fps
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0x72);

		imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x30);
		// Clamping GainControl
		//  imx219_write_reg(imx219, 0x21, IMX219_REG_VALUE_08BIT, 0x84);
		// Sync Amplitude AGCControl
		//  imx219_write_reg(imx219, 0x22, IMX219_REG_VALUE_08BIT, 0x36);
		// Clamping LevelContro
		//  imx219_write_reg(imx219, 0x23, IMX219_REG_VALUE_08BIT, 0x3c);
		imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x48);
		imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0xbb);
		imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0x2e);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x90);

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x25);
		imx219_write_reg(imx219, 0x38, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x39, IMX219_REG_VALUE_08BIT, 0x18);
		// 13B
		if (std == STD_HDA) {
			imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x46);

			imx219_write_reg(imx219, 0x0d, IMX219_REG_VALUE_08BIT, 0x70);
			// update v-dlay from 19 to 1b for ahd 720 25/30.
			imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x1b);

			imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x40);
			imx219_write_reg(imx219, 0x21, IMX219_REG_VALUE_08BIT, 0x46);

			imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0xfe);
			imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x01);

			imx219_write_reg(imx219, 0x2c, IMX219_REG_VALUE_08BIT, 0x3a);
			imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x5a);
			imx219_write_reg(imx219, 0x2e, IMX219_REG_VALUE_08BIT, 0x40);

			imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x9d);
			imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0xca);
			imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0x01);
			imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0xd0);
		}
	}
	// 20B
	else if (fmt == FHD30) {
		tmp &= ~SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x40);
		imx219_write_reg(imx219, 0x0c, IMX219_REG_VALUE_08BIT, 0x03);

		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x03);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0xd2);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x80);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x29);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0x38);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x47);
		// 1920*1080, 30fps
		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x08);
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0x98); //

		imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x48);
		imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0xbb);
		imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0x2e);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x90);

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x05);
		imx219_write_reg(imx219, 0x38, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x39, IMX219_REG_VALUE_08BIT, 0x1C);
		// 15B
		if (std == STD_HDA) {
			imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x44);

			imx219_write_reg(imx219, 0x0d, IMX219_REG_VALUE_08BIT, 0x72);

			imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x01);
			imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0xf0);
			// update v-dlay from 29 to 2a for ahd 1080 25/30.
			imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x2a);
			imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x38);
			imx219_write_reg(imx219, 0x21, IMX219_REG_VALUE_08BIT, 0x46);

			imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0xfe);
			imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x0d);

			imx219_write_reg(imx219, 0x2c, IMX219_REG_VALUE_08BIT, 0x3a);
			imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x54);
			imx219_write_reg(imx219, 0x2e, IMX219_REG_VALUE_08BIT, 0x40);

			imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0xa5);
			imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0x95);
			imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0xe0);
			imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x60);
		}
	}
	// 20B
	else if (fmt == FHD25) {
		tmp &= ~SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x40);
		imx219_write_reg(imx219, 0x0c, IMX219_REG_VALUE_08BIT, 0x03);

		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x03);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0xd2);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x80);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x29);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0x38);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x47);
		// 1920*1080, 25fps
		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x0a);
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0x50); //

		imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x48);
		imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0xbb);
		imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0x2e);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x90);

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x05);
		imx219_write_reg(imx219, 0x38, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x39, IMX219_REG_VALUE_08BIT, 0x1C);
		// 15B
		if (std == STD_HDA) {
			imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x44);

			imx219_write_reg(imx219, 0x0d, IMX219_REG_VALUE_08BIT, 0x73);

			imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x01);
			imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0xf0);
			// update v-dlay from 29 to 2a for ahd 1080 25/30.
			imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x2a);
			imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x3c);
			imx219_write_reg(imx219, 0x21, IMX219_REG_VALUE_08BIT, 0x46);

			imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0xfe);
			imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x0d);

			imx219_write_reg(imx219, 0x2c, IMX219_REG_VALUE_08BIT, 0x3a);
			imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x54);
			imx219_write_reg(imx219, 0x2e, IMX219_REG_VALUE_08BIT, 0x40);

			imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0xa5);
			imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0x86);
			imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0xfb);
			imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x60);
		}
	}
	// 22B
	else if (fmt == FHD60) {
		tmp &= ~SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x40);
		imx219_write_reg(imx219, 0x0c, IMX219_REG_VALUE_08BIT, 0x03);

		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x03);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0xf0);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x80);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x12);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0x38);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x47);
		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x08); //
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0x96); //

		imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x38);

		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0xad);

		imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x40);
		imx219_write_reg(imx219, 0x2e, IMX219_REG_VALUE_08BIT, 0x70);

		imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x74);
		imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0x9b);
		imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0xa5);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0xe0);

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x05);
		imx219_write_reg(imx219, 0x38, IMX219_REG_VALUE_08BIT, 0x40);
		imx219_write_reg(imx219, 0x39, IMX219_REG_VALUE_08BIT, 0x68);
	}
	// 21B
	else if (fmt == FHD50) {
		tmp &= ~SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x40);
		imx219_write_reg(imx219, 0x0c, IMX219_REG_VALUE_08BIT, 0x03);

		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x03);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0xe2);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x80);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x27);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0x38);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x47);

		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x0a); //
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0x4e); //

		imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x38);

		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0xad);

		imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x40);

		imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x74);
		imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0x9b);
		imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0xa5);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0xe0);

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x05);
		imx219_write_reg(imx219, 0x38, IMX219_REG_VALUE_08BIT, 0x40);
		imx219_write_reg(imx219, 0x39, IMX219_REG_VALUE_08BIT, 0x68);
	} else if (fmt == HD50) {
		tmp &= ~SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x42);

		imx219_write_reg(imx219, 0x0c, IMX219_REG_VALUE_08BIT, 0x13);

		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x13);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0x15);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x19);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0xd0);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x25);
		// 1280*720,
		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x07);
		// 1280*720, 50fps
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0xbc);

		imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x48);
		imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0xbb);
		imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0x2e);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x90);

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x05);
		imx219_write_reg(imx219, 0x38, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x39, IMX219_REG_VALUE_08BIT, 0x1c);
	} else if (fmt == HD60) {
		tmp &= ~SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x42);

		imx219_write_reg(imx219, 0x0c, IMX219_REG_VALUE_08BIT, 0x13);

		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x13);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0x15);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x19);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0xd0);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x25);
		// 1280*720,
		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x06);
		// 1280*720, 60fps
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0x72);

		imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x2d, IMX219_REG_VALUE_08BIT, 0x30);

		imx219_write_reg(imx219, 0x30, IMX219_REG_VALUE_08BIT, 0x48);
		imx219_write_reg(imx219, 0x31, IMX219_REG_VALUE_08BIT, 0xbb);
		imx219_write_reg(imx219, 0x32, IMX219_REG_VALUE_08BIT, 0x2e);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x90);

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x05);
		imx219_write_reg(imx219, 0x38, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x39, IMX219_REG_VALUE_08BIT, 0x1c);
	} else if (fmt == HHD30) {
		/* Frame size is 640x720 */
		tmp |= SYS_MODE[ch];
		imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, tmp);

		//imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, 0x04);
		//imx219_write_reg(imx219, 0x45, IMX219_REG_VALUE_08BIT, 0x01);
		//imx219_write_reg(imx219, 0x06, IMX219_REG_VALUE_08BIT, 0x12); //default value
		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0x2d); //default value

		//imx219_write_reg(imx219, 0xf5, IMX219_REG_VALUE_08BIT, 0xff);


		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x42);


		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x13);
		imx219_write_reg(imx219, 0x16, IMX219_REG_VALUE_08BIT, 0x15);
		imx219_write_reg(imx219, 0x17, IMX219_REG_VALUE_08BIT, 0x00);
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x19);
		imx219_write_reg(imx219, 0x19, IMX219_REG_VALUE_08BIT, 0xd0);
		imx219_write_reg(imx219, 0x1a, IMX219_REG_VALUE_08BIT, 0x25);
		imx219_write_reg(imx219, 0x1c, IMX219_REG_VALUE_08BIT, 0x06);  //1280*720, 30fps
		imx219_write_reg(imx219, 0x1d, IMX219_REG_VALUE_08BIT, 0x72);  //1280*720, 30fps

		imx219_write_reg(imx219, 0x35, IMX219_REG_VALUE_08BIT, 0x65);
	} else {
		printk(KERN_WARNING " %s : invalid  video format\n", __func__);
	}

	dev_dbg(&client->dev, "%s, %d, tmp: 0x%02X\n", __func__, __LINE__, tmp); // CCHo addied for debugging

	return;
}

void tp2815_decoder_cfg(struct imx219 *imx219)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	u32 fmt, ch, std;

	ch = imx219->input_ch;
	std = imx219->input_std;
	dev_dbg(&client->dev, "%s, %d, ch: %u, std: %u\n", __func__, __LINE__, ch, std); // CCHo addied for debugging

	/* Select output format according to V4L2 MBUS frame format */
	if (imx219->fmt.height == 720) {
		if (imx219->fmt.width == 1280)
			fmt = HD30;
		else
			fmt = HHD30;
	} else if (imx219->fmt.height == 1080) {
		fmt = FHD30;
	} else {
		fmt = HD30;
		dev_warn(&client->dev, "Use default format. (fmt: %u)\n", fmt);
	}
	dev_dbg(&client->dev, "%s, %d, fmt: %u\n", __func__, __LINE__, fmt); // CCHo addied for debugging

#if 0 //CCHo: Enable 2 inputs for debugging
	if (ch < 4) {
		u32 i;

		for (i = 0; i < ch; i++)
			tp2815_decoder_reg_cfg(imx219, i, fmt, std);
	} else {
		tp2815_decoder_reg_cfg(imx219, CH_ALL, fmt, std);
	}
#else
	tp2815_decoder_reg_cfg(imx219, CH_ALL, fmt, std);
#endif
}

void tp2815_mipi_cfg(struct imx219 *imx219)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	u32 output, val;

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	/* The number of video inputs is the same as the number of virtual channels. */
	switch(imx219->input_ch) {
		case 1:		// 1 virtual channel
			if (imx219->bus.num_data_lanes == 4) {
				if (imx219->fmt.width == 1280)
					output = MIPI_1CH4LANE_297M;
				else
					output = (TP2815_MIPI_CSI_HS_CLOCK_RATE_MHZ == 148)?
								MIPI_1CH4LANE_148M : MIPI_1CH4LANE_74M;
			} else if (imx219->bus.num_data_lanes == 2) {
				output = MIPI_1CH2LANE_594M;
			} else {
				output = MIPI_1CH1LANE_594M;
			}
			break;

		case 2: 	// 2 virtual channels
			if (imx219->bus.num_data_lanes == 2)
				output = MIPI_2CH2LANE_594M;
			else
				output = MIPI_2CH1LANE_594M;
			break;

		case 4: 	// 4 virtual channels
			if (imx219->fmt.width == 1280)
				output = MIPI_4CH4LANE_594M;
			else
				output = (TP2815_MIPI_CSI_HS_CLOCK_RATE_MHZ == 148)?
							MIPI_4CH4LANE_148M : MIPI_4CH4LANE_74M;
			break;

		default:
			output = MIPI_4CH4LANE_594M;
			dev_warn(&client->dev, "Use default output format. (output: %u)\n", output);
			break;
	}
	dev_dbg(&client->dev, "%s, %d, output: %u\n", __func__, __LINE__, output); // CCHo addied for debugging

	/* Enable MIPI page register access */
	imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, MIPI_PAGE);
	imx219_write_reg(imx219, 0x01, IMX219_REG_VALUE_08BIT, 0xf0);
#ifdef TP2815_STREAM_ALWAYS_ON
	imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x01);			// MIPICKEN(b0) = 1(Enable MIPI clock output)
	imx219_write_reg(imx219, 0x08, IMX219_REG_VALUE_08BIT, 0x0f);			// MIPIEN0/1/2/3(b3:0) = 0x0f(Enable MIPI data output)
#endif

	/* MIPI NUM_LANES(0x20)
	 * 0x20[6:4] - NUM_CHANNELS - Number of video channels to be processed
	 * 0x20[2:0] - NUM_LANES    - Number of MIPI data lanes to use for Tx
	 */
	val = (imx219->input_ch<<4) | imx219->bus.num_data_lanes;
	imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, val);
	dev_dbg(&client->dev, "%s, %d, reg20: 0x%08x\n", __func__, __LINE__, val); // CCHo addied for debugging

	if ((output == MIPI_4CH4LANE_594M) || (output == MIPI_1CH4LANE_594M)) {
		//imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x44); // 4 CH, 4 Lanes
		//imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x14); // 1 CH, 4 Lanes
#ifdef TP2815_VC_SEQUENCE_0123
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x9c);
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0x9c); // VC0/1/2/3
#else
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0xe4); // VC0/2/3/1 (default)
#endif
		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x0c);
		imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0x08);
		imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x06);
		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0x11);
		imx219_write_reg(imx219, 0x29, IMX219_REG_VALUE_08BIT, 0x0a);

		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x07); // B2: RST_CSI; B1: RST_MUX; B0:RST_IN
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x00);

		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x33);
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0xb3); // B7: RST_CLK_GEN
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x33); // DIV_CSI_CLK = 1/8, DIV_PHY_CLK = 1/8
	} else if ((output == MIPI_4CH4LANE_297M) || (output == MIPI_1CH4LANE_297M)) {
		//imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x44); // 4 CH, 4 Lanes
		//imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x14); // 1 CH, 4 Lanes
#ifdef TP2815_VC_SEQUENCE_0123
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x9c);
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0x9c); // VC0/1/2/3
#else
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0xe4); // VC0/2/3/1 (default)
#endif
		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x0d);
		imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0x04);
		imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x03);
		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0x09);
		imx219_write_reg(imx219, 0x29, IMX219_REG_VALUE_08BIT, 0x02);

		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x07); // B2: RST_CSI; B1: RST_MUX; B0:RST_IN
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x00);

		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x44);
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0xc4); // B7: RST_CLK_GEN
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x44); // DIV_CSI_CLK = 1/16, DIV_PHY_CLK = 1/16
	} else if ((output == MIPI_4CH4LANE_148M) || (output == MIPI_1CH4LANE_148M)) {
		//imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x44); // 4 CH, 4 Lanes
#ifdef TP2815_VC_SEQUENCE_0123
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x9c);
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0x9c); // VC0/1/2/3
#else
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0xe4); // VC0/2/3/1 (default)
#endif
		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x0e);
		imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0x02);
		imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x02);
		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0x04);
		imx219_write_reg(imx219, 0x29, IMX219_REG_VALUE_08BIT, 0x01);

		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x07);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x00);

		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x55);
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0xd5);
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x55);
	} else if ((output == MIPI_4CH4LANE_74M) || (output == MIPI_1CH4LANE_74M)) {
		imx219_write_reg(imx219, 0x12, IMX219_REG_VALUE_08BIT, 0x49); // PLL Control3 (FB Divider)
		//imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x44); // 4 CH, 4 Lanes
#ifdef TP2815_VC_SEQUENCE_0123
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x9c);
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0x9c); // VC0/1/2/3
#else
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0xe4); // VC0/2/3/1 (default)
#endif
		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x0e);
		imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0x01);
		imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x01);
		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0x02);
		imx219_write_reg(imx219, 0x29, IMX219_REG_VALUE_08BIT, 0x01);

		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x07);
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x00);

		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x55);
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0xd5);
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x55);
	} else if ((output == MIPI_4CH2LANE_594M) ||
			 (output == MIPI_2CH2LANE_594M) || (output == MIPI_1CH2LANE_594M)) {
		//imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x42); // 4 CH, 2 Lanes
#ifdef TP2815_VC_SEQUENCE_0123
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x9c);
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0x9c); // VC0/1/2/3
#else
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0xe4); // VC0/2/3/1 (default)
#endif
		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x0c);
		imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0x08);
		imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x06);
		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0x11);
		imx219_write_reg(imx219, 0x29, IMX219_REG_VALUE_08BIT, 0x0a);

		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x07); // B2: RST_CSI; B1: RST_MUX; B0:RST_IN
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x00);

		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x43);
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0xc3); // B7: RST_CLK_GEN
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x43); // DIV_CSI_CLK = 1/16, DIV_PHY_CLK = 1/8
	} else if ((output == MIPI_4CH1LANE_594M) ||
			 (output == MIPI_2CH1LANE_594M) || (output == MIPI_1CH1LANE_594M)) {
		//imx219_write_reg(imx219, 0x20, IMX219_REG_VALUE_08BIT, 0x42); // 4 CH, 2 Lanes
#ifdef TP2815_VC_SEQUENCE_0123
		imx219_write_reg(imx219, 0x18, IMX219_REG_VALUE_08BIT, 0x9c);
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0x9c); // VC0/1/2/3
#else
		imx219_write_reg(imx219, 0x34, IMX219_REG_VALUE_08BIT, 0xe4); // VC0/2/3/1 (default)
#endif
		imx219_write_reg(imx219, 0x15, IMX219_REG_VALUE_08BIT, 0x0c);
		imx219_write_reg(imx219, 0x25, IMX219_REG_VALUE_08BIT, 0x08);
		imx219_write_reg(imx219, 0x26, IMX219_REG_VALUE_08BIT, 0x06);
		imx219_write_reg(imx219, 0x27, IMX219_REG_VALUE_08BIT, 0x11);
		imx219_write_reg(imx219, 0x29, IMX219_REG_VALUE_08BIT, 0x0a);

		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x07); // B2: RST_CSI; B1: RST_MUX; B0:RST_IN
		imx219_write_reg(imx219, 0x33, IMX219_REG_VALUE_08BIT, 0x00);

		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x53);
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0xd3); // B7: RST_CLK_GEN
		imx219_write_reg(imx219, 0x14, IMX219_REG_VALUE_08BIT, 0x53); // DIV_CSI_CLK = 1/32, DIV_PHY_CLK = 1/8
	} else {
		printk(KERN_WARNING " %s : invalid  output format\n", __func__);
	}

#ifdef TP2815_STREAM_ALWAYS_ON
	/* Enable MIPI CSI2 output */
	imx219_write_reg(imx219, 0x23, IMX219_REG_VALUE_08BIT, 0x02);
	imx219_write_reg(imx219, 0x23, IMX219_REG_VALUE_08BIT, 0x00);
#else
	/* Disable MIPI CSI2 output */
	imx219_write_reg(imx219, 0x23, IMX219_REG_VALUE_08BIT, 0x02);
#endif
	return;
}

void tp2815_common_cfg(struct imx219 *imx219)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	tp2815_decoder_cfg(imx219);
	tp2815_mipi_cfg(imx219);
}

static void imx219_set_default_format(struct imx219 *imx219)
{
	struct v4l2_mbus_framefmt *fmt;

	fmt = &imx219->fmt;
	fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
							  fmt->colorspace,
							  fmt->ycbcr_enc);
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = supported_modes[0].width;
	fmt->height = supported_modes[0].height;
	fmt->field = V4L2_FIELD_NONE;
}

static int imx219_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx219 *imx219 = to_imx219(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->pad, 0);
	struct v4l2_rect *try_crop;

	mutex_lock(&imx219->mutex);

	/* Initialize try_fmt */
	try_fmt->width = supported_modes[0].width;
	try_fmt->height = supported_modes[0].height;
	try_fmt->code = imx219_get_format_code(imx219,
					       MEDIA_BUS_FMT_SRGGB10_1X10);
	try_fmt->field = V4L2_FIELD_NONE;

	/* Initialize try_crop rectangle. */
	try_crop = v4l2_subdev_get_try_crop(sd, fh->pad, 0);
	try_crop->top = IMX219_PIXEL_ARRAY_TOP;
	try_crop->left = IMX219_PIXEL_ARRAY_LEFT;
	try_crop->width = IMX219_PIXEL_ARRAY_WIDTH;
	try_crop->height = IMX219_PIXEL_ARRAY_HEIGHT;

	mutex_unlock(&imx219->mutex);

	return 0;
}

static int imx219_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx219 *imx219 =
		container_of(ctrl->handler, struct imx219, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	u32 val;
	int ret;

	dev_dbg(&client->dev, "%s, %d, ctrl->id: 0x%08x\n", __func__, __LINE__, ctrl->id); // CCHo addied for debugging

	if (ctrl->id == V4L2_CID_VBLANK) {
		int exposure_max, exposure_def;

		/* Update max exposure while meeting expected vblanking */
		exposure_max = imx219->mode->height + ctrl->val - 4;
		exposure_def = (exposure_max < IMX219_EXPOSURE_DEFAULT) ?
			exposure_max : IMX219_EXPOSURE_DEFAULT;
		__v4l2_ctrl_modify_range(imx219->exposure,
					 imx219->exposure->minimum,
					 exposure_max, imx219->exposure->step,
					 exposure_def);
	}

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (pm_runtime_get_if_in_use(&client->dev) == 0)
		return 0;

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
#if 1 /* CCHo */
		ret = 0;
#else
		ret = imx219_write_reg(imx219, IMX219_REG_ANALOG_GAIN,
				       IMX219_REG_VALUE_08BIT, ctrl->val);
#endif
		break;
	case V4L2_CID_EXPOSURE:
#if 1 /* CCHo */
		ret = 0;
#else
		ret = imx219_write_reg(imx219, IMX219_REG_EXPOSURE,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
#endif
		break;
	case V4L2_CID_DIGITAL_GAIN:
#if 1 /* CCHo */
		ret = 0;
#else
		ret = imx219_write_reg(imx219, IMX219_REG_DIGITAL_GAIN,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
#endif
		break;
	case V4L2_CID_TEST_PATTERN:
#if 1 /* CCHo */
		/* Page Register(0x40)
		 * 0x40[6]   - APAGE - Enable audio register access
		 * 0x40[3]   - MPAGE - Enable MIPI register access
		 * 0x40[2]   - ALLWE - Enable writing all channel page register
		 * 0x40[1:0] - PAGE  - Select channel page register
		 * ALLWE  PAGE    Write Register      Read Register
		 * 0      0       VIN1 Video          VIN1 Video
		 * 0      1       VIN2 Video          VIN2 Video
		 * 0      2       VIN3 Video          VIN3 Video
		 * 0      3       VIN4 Video          VIN4 Video
		 * 1      0       All VIN1-4 Video    VIN1 Video
		 * 1      1       All VIN1-4 Video    VIN2 Video
		 * 1      2       All VIN1-4 Video    VIN3 Video
		 * 1      3       All VIN1-4 Video    VIN4 Video
		 */
		if (imx219->input_ch == 1)
			imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, imx219->input_ch-1);
		else
			imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, CH_ALL);

		/* FCS(0x2A[3]) - Fource free run mode
		 * 0 = Disabled
		 * 1 = Fource free-run                                     */
		imx219_read_reg(imx219, 0x2a, IMX219_REG_VALUE_08BIT, &val);
		dev_dbg(&client->dev, "%s, %d, Reg 0x2A: 0x%02x, ctrl->val: 0x%02x\n", __func__, __LINE__, val, ctrl->val); // CCHo addied for debugging

		val = val & (~0x00000008);
		val = val | (imx219_test_pattern_val[ctrl->val]<<3);
		ret = imx219_write_reg(imx219, 0x2a, IMX219_REG_VALUE_08BIT, val);

		dev_dbg(&client->dev, "%s, %d, Reg 0x2A: 0x%02x\n", __func__, __LINE__, val); // CCHo addied for debugging
#else
		ret = imx219_write_reg(imx219, IMX219_REG_TEST_PATTERN,
				       IMX219_REG_VALUE_16BIT,
				       imx219_test_pattern_val[ctrl->val]);
#endif
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
#if 1 /* CCHo */
		ret = 0;
#else
		ret = imx219_write_reg(imx219, IMX219_REG_ORIENTATION, 1,
				       imx219->hflip->val |
				       imx219->vflip->val << 1);
#endif
		break;
	case V4L2_CID_VBLANK:
#if 1 /* CCHo */
		ret = 0;
#else
		ret = imx219_write_reg(imx219, IMX219_REG_VTS,
				       IMX219_REG_VALUE_16BIT,
				       imx219->mode->height + ctrl->val);
#endif
		break;
	case V4L2_CID_TEST_PATTERN_RED:
#if 1 /* CCHo */
		ret = 0;
#else
		ret = imx219_write_reg(imx219, IMX219_REG_TESTP_RED,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
#endif
		break;
	case V4L2_CID_TEST_PATTERN_GREENR:
#if 1 /* CCHo */
		ret = 0;
#else
		ret = imx219_write_reg(imx219, IMX219_REG_TESTP_GREENR,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
#endif
		break;
	case V4L2_CID_TEST_PATTERN_BLUE:
#if 1 /* CCHo */
		/* Page Register(0x40)
		 * 0x40[6]   - APAGE - Enable audio register access
		 * 0x40[3]   - MPAGE - Enable MIPI register access
		 * 0x40[2]   - ALLWE - Enable writing all channel page register
		 * 0x40[1:0] - PAGE  - Select channel page register
		 * ALLWE  PAGE    Write Register      Read Register
		 * 0      0       VIN1 Video          VIN1 Video
		 * 0      1       VIN2 Video          VIN2 Video
		 * 0      2       VIN3 Video          VIN3 Video
		 * 0      3       VIN4 Video          VIN4 Video
		 * 1      0       All VIN1-4 Video    VIN1 Video
		 * 1      1       All VIN1-4 Video    VIN2 Video
		 * 1      2       All VIN1-4 Video    VIN3 Video
		 * 1      3       All VIN1-4 Video    VIN4 Video
		 */
		if (imx219->input_ch == 1)
			imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, imx219->input_ch-1);
		else
			imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, CH_ALL);

		/* LCS(0x2A[2]) - Fource free run mode color control
		 * 0 = Normal input video data
		 * 1 = Blue screen                                         */
		imx219_read_reg(imx219, 0x2a, IMX219_REG_VALUE_08BIT, &val);
		dev_dbg(&client->dev, "%s, %d, Reg 0x2A: 0x%02x, ctrl->val: 0x%02x\n", __func__, __LINE__, val, ctrl->val); // CCHo addied for debugging

		val = val & (~0x00000004);
		val = val | (ctrl->val<<2);
		ret = imx219_write_reg(imx219, 0x2a, IMX219_REG_VALUE_08BIT, val);

		dev_dbg(&client->dev, "%s, %d, Reg 0x2A: 0x%02x\n", __func__, __LINE__, val); // CCHo addied for debugging
#else
		ret = imx219_write_reg(imx219, IMX219_REG_TESTP_BLUE,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
#endif
		break;
	case V4L2_CID_TEST_PATTERN_GREENB:
#if 1 /* CCHo */
		ret = 0;
#else
		ret = imx219_write_reg(imx219, IMX219_REG_TESTP_GREENB,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
#endif
		break;
	default:
		dev_info(&client->dev,
			 "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}

	dev_dbg(&client->dev, "%s, %d, ret: %d\n", __func__, __LINE__, ret); // CCHo addied for debugging

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx219_ctrl_ops = {
	.s_ctrl = imx219_set_ctrl,
};

static int imx219_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx219 *imx219 = to_imx219(sd);

	if (code->index >= (ARRAY_SIZE(codes) / 4))
		return -EINVAL;

	code->code = imx219_get_format_code(imx219, codes[code->index * 4]);

	return 0;
}

static int imx219_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx219 *imx219 = to_imx219(sd);

	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != imx219_get_format_code(imx219, fse->code))
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static void imx219_reset_colorspace(struct v4l2_mbus_framefmt *fmt)
{
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
							  fmt->colorspace,
							  fmt->ycbcr_enc);
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
}

static void imx219_update_pad_format(struct imx219 *imx219,
				     const struct imx219_mode *mode,
				     struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	imx219_reset_colorspace(&fmt->format);
}

static int __imx219_get_pad_format(struct imx219 *imx219,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_format *fmt)
{
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(&imx219->sd, cfg, fmt->pad);
		/* update the code which could change due to vflip or hflip: */
		try_fmt->code = imx219_get_format_code(imx219, try_fmt->code);
		fmt->format = *try_fmt;
	} else {
		imx219_update_pad_format(imx219, imx219->mode, fmt);
		fmt->format.code = imx219_get_format_code(imx219,
							  imx219->fmt.code);
	}

	return 0;
}

static int imx219_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct imx219 *imx219 = to_imx219(sd);
	int ret;

	mutex_lock(&imx219->mutex);
	ret = __imx219_get_pad_format(imx219, cfg, fmt);
	mutex_unlock(&imx219->mutex);

	return ret;
}

static int imx219_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct imx219 *imx219 = to_imx219(sd);
	const struct imx219_mode *mode;
	struct v4l2_mbus_framefmt *framefmt;
	int exposure_max, exposure_def, hblank;
	unsigned int i;

	mutex_lock(&imx219->mutex);

	for (i = 0; i < ARRAY_SIZE(codes); i++)
		if (codes[i] == fmt->format.code)
			break;
	if (i >= ARRAY_SIZE(codes))
		i = 0;

	/* Bayer order varies with flips */
	fmt->format.code = imx219_get_format_code(imx219, codes[i]);

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);
	imx219_update_pad_format(imx219, mode, fmt);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		*framefmt = fmt->format;
	} else if (imx219->mode != mode ||
		   imx219->fmt.code != fmt->format.code) {
		imx219->fmt = fmt->format;
		imx219->mode = mode;
		/* Update limits and set FPS to default */
		__v4l2_ctrl_modify_range(imx219->vblank, IMX219_VBLANK_MIN,
					 IMX219_VTS_MAX - mode->height, 1,
					 mode->vts_def - mode->height);
		__v4l2_ctrl_s_ctrl(imx219->vblank,
				   mode->vts_def - mode->height);
		/* Update max exposure while meeting expected vblanking */
		exposure_max = mode->vts_def - 4;
		exposure_def = (exposure_max < IMX219_EXPOSURE_DEFAULT) ?
			exposure_max : IMX219_EXPOSURE_DEFAULT;
		__v4l2_ctrl_modify_range(imx219->exposure,
					 imx219->exposure->minimum,
					 exposure_max, imx219->exposure->step,
					 exposure_def);
		/*
		 * Currently PPL is fixed to IMX219_PPL_DEFAULT, so hblank
		 * depends on mode->width only, and is not changeble in any
		 * way other than changing the mode.
		 */
		hblank = IMX219_PPL_DEFAULT - mode->width;
		__v4l2_ctrl_modify_range(imx219->hblank, hblank, hblank, 1,
					 hblank);
	}

	mutex_unlock(&imx219->mutex);

	return 0;
}

#if 1 /* CCHo */
// Blank
#else
static int imx219_set_framefmt(struct imx219 *imx219)
{
	switch (imx219->fmt.code) {
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SBGGR8_1X8:
		return imx219_write_regs(imx219, raw8_framefmt_regs,
					ARRAY_SIZE(raw8_framefmt_regs));

	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
		return imx219_write_regs(imx219, raw10_framefmt_regs,
					ARRAY_SIZE(raw10_framefmt_regs));
	}

	return -EINVAL;
}
#endif

static const struct v4l2_rect *
__imx219_get_pad_crop(struct imx219 *imx219, struct v4l2_subdev_pad_config *cfg,
		      unsigned int pad, enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(&imx219->sd, cfg, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &imx219->mode->crop;
	}

	return NULL;
}

static int imx219_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP: {
		struct imx219 *imx219 = to_imx219(sd);

		mutex_lock(&imx219->mutex);
		sel->r = *__imx219_get_pad_crop(imx219, cfg, sel->pad,
						sel->which);
		mutex_unlock(&imx219->mutex);

		return 0;
	}

	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = IMX219_NATIVE_WIDTH;
		sel->r.height = IMX219_NATIVE_HEIGHT;

		return 0;

	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.top = IMX219_PIXEL_ARRAY_TOP;
		sel->r.left = IMX219_PIXEL_ARRAY_LEFT;
		sel->r.width = IMX219_PIXEL_ARRAY_WIDTH;
		sel->r.height = IMX219_PIXEL_ARRAY_HEIGHT;

		return 0;
	}

	return -EINVAL;
}

static int imx219_start_streaming(struct imx219 *imx219)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	const struct imx219_reg_list *reg_list;
	int ret;

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	ret = pm_runtime_get_sync(&client->dev);
	dev_dbg(&client->dev, "%s, %d, ret:%d\n", __func__, __LINE__, ret); // CCHo addied for debugging
	if (ret < 0) {
		pm_runtime_put_noidle(&client->dev);
		return ret;
	}

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	/* Apply default values of current mode */
	reg_list = &imx219->mode->reg_list;
#if 1 /* CCHo */
	tp2815_common_cfg(imx219);
	ret = 0;
#else
	ret = imx219_write_regs(imx219, reg_list->regs, reg_list->num_of_regs);
#endif
	if (ret) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		goto err_rpm_put;
	}

#if 1 /* CCHo */
	ret = 0;
#else
	ret = imx219_set_framefmt(imx219);
#endif
	if (ret) {
		dev_err(&client->dev, "%s failed to set frame format: %d\n",
			__func__, ret);
		goto err_rpm_put;
	}

	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(imx219->sd.ctrl_handler);
	dev_dbg(&client->dev, "%s, %d, ret:%d\n", __func__, __LINE__, ret); // CCHo addied for debugging
	if (ret)
		goto err_rpm_put;

	/* set stream on register */
#if 1 /* CCHo */
	#ifndef TP2815_STREAM_ALWAYS_ON
	ret = imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, MIPI_PAGE);	// Switch to MIPI register page

	if (!ret) {
		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x01);			// MIPICKEN(b0) = 1(Enable MIPI clock output)
		imx219_write_reg(imx219, 0x08, IMX219_REG_VALUE_08BIT, 0x0f);			// MIPIEN0/1/2/3(b3:0) = 0x0f(Enable MIPI data output)

		/* Set STOPCLK(reg0x23.b1) to 0. DPHY clock lane is normal operation. */
		ret = imx219_write_reg(imx219, 0x23, IMX219_REG_VALUE_08BIT, 0x00);		// STOPCLK(b1) = 0(DPHY clock lane is normal operation)
	}
	#endif
#else
	ret = imx219_write_reg(imx219, IMX219_REG_MODE_SELECT,
			       IMX219_REG_VALUE_08BIT, IMX219_MODE_STREAMING);
#endif

	if (ret)
		goto err_rpm_put;

	/* vflip and hflip cannot change during streaming */
	__v4l2_ctrl_grab(imx219->vflip, true);
	__v4l2_ctrl_grab(imx219->hflip, true);

	return 0;

err_rpm_put:
	pm_runtime_put(&client->dev);
	return ret;
}

static void imx219_stop_streaming(struct imx219 *imx219)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	int ret;

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	/* set stream off register */
#if 1 /* CCHo */
	#ifndef TP2815_STREAM_ALWAYS_ON
	ret = imx219_write_reg(imx219, 0x40, IMX219_REG_VALUE_08BIT, MIPI_PAGE);	// Switch to MIPI register page

	if (!ret) {
		imx219_write_reg(imx219, 0x02, IMX219_REG_VALUE_08BIT, 0x00);			// MIPICKEN(b0) = 0(Disable MIPI clock output)
		imx219_write_reg(imx219, 0x08, IMX219_REG_VALUE_08BIT, 0x00);			// MIPIEN0/1/2/3(b3:0) = 0x00(Disable MIPI data output)

		/* Set STOPCLK(reg0x23.b1) to 1. Force DPHY clock lane into STOP state. */
		ret = imx219_write_reg(imx219, 0x23, IMX219_REG_VALUE_08BIT, 0x02);		// STOPCLK(b1) = 1(Force DPHY clock lane into STOP state)
	}
	#else
	ret = 0;
	#endif
#else
	ret = imx219_write_reg(imx219, IMX219_REG_MODE_SELECT,
			       IMX219_REG_VALUE_08BIT, IMX219_MODE_STANDBY);
#endif
	if (ret)
		dev_err(&client->dev, "%s failed to set stream\n", __func__);

	__v4l2_ctrl_grab(imx219->vflip, false);
	__v4l2_ctrl_grab(imx219->hflip, false);

	pm_runtime_put(&client->dev);
}

static int imx219_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx219 *imx219 = to_imx219(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd); // CCHo addied for debugging
	int ret = 0;

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	mutex_lock(&imx219->mutex);
	if (imx219->streaming == enable) {
		mutex_unlock(&imx219->mutex);
		return 0;
	}

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	if (enable) {
		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = imx219_start_streaming(imx219);
		if (ret)
			goto err_unlock;
	} else {
		imx219_stop_streaming(imx219);
	}

	imx219->streaming = enable;

	mutex_unlock(&imx219->mutex);

	return ret;

err_unlock:
	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	mutex_unlock(&imx219->mutex);

	return ret;
}

/* Power/clock management functions */
static int imx219_power_on(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_imx219(sd);
	int ret;

#ifdef TP2815_REGULATOR_CONTROL
	ret = regulator_bulk_enable(IMX219_NUM_SUPPLIES,
				    imx219->supplies);
	if (ret) {
		dev_err(&client->dev, "%s: failed to enable regulators\n",
			__func__);
		return ret;
	}
#endif

	ret = clk_prepare_enable(imx219->xclk);
	if (ret) {
		dev_err(&client->dev, "%s: failed to enable clock\n",
			__func__);
		goto reg_off;
	}

	gpiod_set_value_cansleep(imx219->reset_gpio, 1);
	usleep_range(IMX219_XCLR_MIN_DELAY_US,
		     IMX219_XCLR_MIN_DELAY_US + IMX219_XCLR_DELAY_RANGE_US);

	return 0;

reg_off:
#ifdef TP2815_REGULATOR_CONTROL
	regulator_bulk_disable(IMX219_NUM_SUPPLIES, imx219->supplies);
#endif

	return ret;
}

static int imx219_power_off(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_imx219(sd);

	gpiod_set_value_cansleep(imx219->reset_gpio, 0);
#ifdef TP2815_REGULATOR_CONTROL
	regulator_bulk_disable(IMX219_NUM_SUPPLIES, imx219->supplies);
#endif
	clk_disable_unprepare(imx219->xclk);

	return 0;
}

static int __maybe_unused imx219_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_imx219(sd);

	if (imx219->streaming)
		imx219_stop_streaming(imx219);

	return 0;
}

static int __maybe_unused imx219_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_imx219(sd);
	int ret;

	if (imx219->streaming) {
		ret = imx219_start_streaming(imx219);
		if (ret)
			goto error;
	}

	return 0;

error:
	imx219_stop_streaming(imx219);
	imx219->streaming = false;

	return ret;
}

#ifdef TP2815_REGULATOR_CONTROL
static int imx219_get_regulators(struct imx219 *imx219)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	unsigned int i;

	for (i = 0; i < IMX219_NUM_SUPPLIES; i++)
		imx219->supplies[i].supply = imx219_supply_name[i];

	return devm_regulator_bulk_get(&client->dev,
				       IMX219_NUM_SUPPLIES,
				       imx219->supplies);
}
#endif

/* Verify chip ID */
static int imx219_identify_module(struct imx219 *imx219)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	int ret;
	u32 val, msb, lsb;

#if 1
	ret = imx219_read_reg(imx219, IMX219_REG_CHIP_ID_H,
				  IMX219_REG_VALUE_08BIT, &msb);

	ret = imx219_read_reg(imx219, IMX219_REG_CHIP_ID_L,
				  IMX219_REG_VALUE_08BIT, &lsb);

	val = msb << 8 | lsb;
#else
	ret = imx219_read_reg(imx219, IMX219_REG_CHIP_ID,
			      IMX219_REG_VALUE_16BIT, &val);
#endif
	if (ret) {
		dev_err(&client->dev, "failed to read chip id %x\n",
			IMX219_CHIP_ID);
		return ret;
	}

	if (val != IMX219_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x\n",
			IMX219_CHIP_ID, val);
		return -EIO;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops imx219_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops imx219_video_ops = {
	.s_stream = imx219_set_stream,
};

static const struct v4l2_subdev_pad_ops imx219_pad_ops = {
	.enum_mbus_code = imx219_enum_mbus_code,
	.get_fmt = imx219_get_pad_format,
	.set_fmt = imx219_set_pad_format,
	.get_selection = imx219_get_selection,
	.enum_frame_size = imx219_enum_frame_size,
};

static const struct v4l2_subdev_ops imx219_subdev_ops = {
	.core = &imx219_core_ops,
	.video = &imx219_video_ops,
	.pad = &imx219_pad_ops,
};

static const struct v4l2_subdev_internal_ops imx219_internal_ops = {
	.open = imx219_open,
};

/* Initialize control handlers */
static int imx219_init_controls(struct imx219 *imx219)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx219->sd);
	struct v4l2_ctrl_handler *ctrl_hdlr;
	unsigned int height = imx219->mode->height;
	struct v4l2_fwnode_device_properties props;
	int exposure_max, exposure_def, hblank;
	int i, ret;

	ctrl_hdlr = &imx219->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 11);
	if (ret)
		return ret;

	mutex_init(&imx219->mutex);
	ctrl_hdlr->lock = &imx219->mutex;

	/* By default, PIXEL_RATE is read only */
	imx219->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       IMX219_PIXEL_RATE,
					       IMX219_PIXEL_RATE, 1,
					       IMX219_PIXEL_RATE);

	/* Initial vblank/hblank/exposure parameters based on current mode */
	imx219->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					   V4L2_CID_VBLANK, IMX219_VBLANK_MIN,
					   IMX219_VTS_MAX - height, 1,
					   imx219->mode->vts_def - height);
	hblank = IMX219_PPL_DEFAULT - imx219->mode->width;
	imx219->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					   V4L2_CID_HBLANK, hblank, hblank,
					   1, hblank);
	if (imx219->hblank)
		imx219->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	exposure_max = imx219->mode->vts_def - 4;
	exposure_def = (exposure_max < IMX219_EXPOSURE_DEFAULT) ?
		exposure_max : IMX219_EXPOSURE_DEFAULT;
	imx219->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX219_EXPOSURE_MIN, exposure_max,
					     IMX219_EXPOSURE_STEP,
					     exposure_def);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  IMX219_ANA_GAIN_MIN, IMX219_ANA_GAIN_MAX,
			  IMX219_ANA_GAIN_STEP, IMX219_ANA_GAIN_DEFAULT);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  IMX219_DGTL_GAIN_MIN, IMX219_DGTL_GAIN_MAX,
			  IMX219_DGTL_GAIN_STEP, IMX219_DGTL_GAIN_DEFAULT);

	imx219->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);
	if (imx219->hflip)
		imx219->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	imx219->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (imx219->vflip)
		imx219->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &imx219_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(imx219_test_pattern_menu) - 1,
				     0, 0, imx219_test_pattern_menu);
	for (i = 0; i < 4; i++) {
		/*
		 * The assumption is that
		 * V4L2_CID_TEST_PATTERN_GREENR == V4L2_CID_TEST_PATTERN_RED + 1
		 * V4L2_CID_TEST_PATTERN_BLUE   == V4L2_CID_TEST_PATTERN_RED + 2
		 * V4L2_CID_TEST_PATTERN_GREENB == V4L2_CID_TEST_PATTERN_RED + 3
		 */
		v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
				  V4L2_CID_TEST_PATTERN_RED + i,
				  IMX219_TESTP_COLOUR_MIN,
				  IMX219_TESTP_COLOUR_MAX,
				  IMX219_TESTP_COLOUR_STEP,
				  IMX219_TESTP_COLOUR_MAX);
		/* The "Solid color" pattern is white by default */
	}

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&client->dev, "%s control init failed (%d)\n",
			__func__, ret);
		goto error;
	}

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		goto error;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &imx219_ctrl_ops,
					      &props);
	if (ret)
		goto error;

	imx219->sd.ctrl_handler = ctrl_hdlr;

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);
	mutex_destroy(&imx219->mutex);

	return ret;
}

static void imx219_free_controls(struct imx219 *imx219)
{
	v4l2_ctrl_handler_free(imx219->sd.ctrl_handler);
	mutex_destroy(&imx219->mutex);
}

static int imx219_check_hwcfg(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_imx219(sd);
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret = -EINVAL;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	if (v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg)) {
		dev_err(dev, "could not parse endpoint\n");
		goto error_out;
	}

	/* Check the number of MIPI CSI2 data lanes */
	if ((ep_cfg.bus.mipi_csi2.num_data_lanes != 1) &&
		(ep_cfg.bus.mipi_csi2.num_data_lanes != 2) &&
		(ep_cfg.bus.mipi_csi2.num_data_lanes != 4)) {
		dev_err(dev, "only 1/2/4 data lanes are currently supported\n");
		goto error_out;
	}

	memcpy(&imx219->bus, &ep_cfg.bus.mipi_csi2, sizeof(struct v4l2_fwnode_bus_mipi_csi2));
	imx219->bus_type = ep_cfg.bus_type;

	dev_dbg(dev, "%s, %d, num_data_lanes: %u, bus_type: %u\n",
		__func__, __LINE__, imx219->bus.num_data_lanes, imx219->bus_type); // CCHo addied for debugging

	/* Check the link frequency set in device tree */
	if (!ep_cfg.nr_of_link_frequencies) {
		dev_err(dev, "link-frequency property not found in DT\n");
		goto error_out;
	}

	if (ep_cfg.nr_of_link_frequencies != 1 ||
	    ep_cfg.link_frequencies[0] != IMX219_DEFAULT_LINK_FREQ) {
		dev_err(dev, "Link frequency not supported: %lld\n",
			ep_cfg.link_frequencies[0]);
		goto error_out;
	}

	ret = 0;

error_out:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	fwnode_handle_put(endpoint);

	return ret;
}

static int imx219_probe(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct device *dev = &client->dev;
	struct imx219 *imx219;
	u32 val;
	int ret;

	dev_dbg(dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	imx219 = devm_kzalloc(&client->dev, sizeof(*imx219), GFP_KERNEL);
	if (!imx219)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&imx219->sd, client, &imx219_subdev_ops);

	/* Check the hardware configuration in device tree */
	if (imx219_check_hwcfg(dev))
		return -EINVAL;

	/* Get system clock (xclk) */
	imx219->xclk = devm_clk_get(dev, NULL);
	if (IS_ERR(imx219->xclk)) {
		dev_err(dev, "failed to get xclk\n");
		return PTR_ERR(imx219->xclk);
	}

	imx219->xclk_freq = clk_get_rate(imx219->xclk);
	if (imx219->xclk_freq != IMX219_XCLK_FREQ) {
		dev_err(dev, "xclk frequency not supported: %d Hz\n",
			imx219->xclk_freq);
		return -EINVAL;
	}

#ifdef TP2815_REGULATOR_CONTROL
	ret = imx219_get_regulators(imx219);
	if (ret) {
		dev_err(dev, "failed to get regulators\n");
		return ret;
	}
#endif

	/* Request optional enable pin */
	imx219->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_HIGH);

	/* Obtain TP2815 input related properties */
	ret = of_property_read_u32(np, "input-channel", &val);
	if (ret) {
		dev_err(dev, "%pOF: No input-channel property found\n", np);
		return -EINVAL;
	}
	imx219->input_ch = val;

	dev_dbg(dev, "%s, %d, input-channel: %u\n", __func__, __LINE__, imx219->input_ch); // CCHo addied for debugging

	ret = of_property_read_u32(np, "input-standard", &val);
	if (ret) {
		dev_err(dev, "%pOF: No input-standard property found\n", np);
		return -EINVAL;
	}
	imx219->input_std = val;

	dev_dbg(dev, "%s, %d, input-standard: %u\n", __func__, __LINE__, imx219->input_std); // CCHo addied for debugging

	/*
	 * The sensor must be powered for imx219_identify_module()
	 * to be able to read the CHIP_ID register
	 */
	ret = imx219_power_on(dev);
	if (ret)
		return ret;

	ret = imx219_identify_module(imx219);
	if (ret)
		goto error_power_off;

	/* Set default mode to max resolution */
	imx219->mode = &supported_modes[0];

#if 1 /*CCHo: skip this action */
	// Blank
#else
	/* sensor doesn't enter LP-11 state upon power up until and unless
	 * streaming is started, so upon power up switch the modes to:
	 * streaming -> standby
	 */
	ret = imx219_write_reg(imx219, IMX219_REG_MODE_SELECT,
			       IMX219_REG_VALUE_08BIT, IMX219_MODE_STREAMING);
	if (ret < 0)
		goto error_power_off;
	usleep_range(100, 110);

	/* put sensor back to standby mode */
	ret = imx219_write_reg(imx219, IMX219_REG_MODE_SELECT,
			       IMX219_REG_VALUE_08BIT, IMX219_MODE_STANDBY);
	if (ret < 0)
		goto error_power_off;
	usleep_range(100, 110);
#endif

	ret = imx219_init_controls(imx219);
	if (ret)
		goto error_power_off;

	/* Initialize subdev */
	imx219->sd.internal_ops = &imx219_internal_ops;
	imx219->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx219->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	imx219->pad.flags = MEDIA_PAD_FL_SOURCE;

	/* Initialize default format */
	imx219_set_default_format(imx219);

	ret = media_entity_pads_init(&imx219->sd.entity, 1, &imx219->pad);
	if (ret) {
		dev_err(dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor_common(&imx219->sd);
	if (ret < 0) {
		dev_err(dev, "failed to register sensor sub-device: %d\n", ret);
		goto error_media_entity;
	}

	/* Initialize TP2815 chip */
	//tp2815_common_cfg(imx219);

	/* Enable runtime PM and turn off the device */
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	dev_info(dev, "Techpoint TP2815 driver, %d input channels, %d data lanes\n",
		imx219->input_ch, imx219->bus.num_data_lanes);

	return 0;

error_media_entity:
	media_entity_cleanup(&imx219->sd.entity);

error_handler_free:
	imx219_free_controls(imx219);

error_power_off:
	imx219_power_off(dev);

	return ret;
}

static int imx219_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_imx219(sd);

	dev_dbg(&client->dev, "%s, %d\n", __func__, __LINE__); // CCHo addied for debugging

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	imx219_free_controls(imx219);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx219_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static const struct of_device_id tp2815_dt_ids[] = {
	{ .compatible = "Techpoint,tp2815" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tp2815_dt_ids);

static const struct dev_pm_ops tp2815_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(imx219_suspend, imx219_resume)
	SET_RUNTIME_PM_OPS(imx219_power_off, imx219_power_on, NULL)
};

static struct i2c_driver tp2815_i2c_driver = {
	.driver = {
		.name = "tp2815",
		.of_match_table	= tp2815_dt_ids,
		.pm = &tp2815_pm_ops,
	},
	.probe_new = imx219_probe,
	.remove = imx219_remove,
};

module_i2c_driver(tp2815_i2c_driver);

MODULE_AUTHOR("Cheng Chung Ho <cc.ho@sunplus.com");
MODULE_DESCRIPTION("Techpoint TP2815 driver");
MODULE_LICENSE("GPL v2");
