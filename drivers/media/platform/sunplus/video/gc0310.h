/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Sunplus Technology Co., Ltd.
 *       All rights reserved.
 */

#ifndef __GC0310_H__
#define __GC0310_H__

//#define FUNC_DEBUG()            pr_info("[GC0310] %s (L:%d)\n", __func__, __LINE__)
#define FUNC_DEBUG()
#define DBG_INFO(fmt, args ...) pr_info("[GC0310] " fmt, ## args)
#define DBG_ERR(fmt, args ...)  pr_err("[GC0310] ERR: " fmt, ## args)

/* GC0310 Registers */
#define GC0310_REG_CHIP_ID      0xF0
#define CHIP_ID                 0xa310
#define GC0310_REG_CTRL_MODE    0x00

#define GC0310_REG_VALUE_08BIT  1
#define GC0310_REG_VALUE_16BIT  2
#define GC0310_REG_VALUE_24BIT  3

#define to_gc0310(sd)           container_of(sd, struct gc0310, subdev)


enum gc0310_mode_id {
	GC0310_MODE_VGA_640_480  = 0,
	GC0310_NUM_MODES,
};

enum gc0310_frame_rate {
	GC0310_30_FPS = 0,
	GC0310_NUM_FRAMERATES,
};

/*
 * Image size under 1280 * 960 are SUBSAMPLING
 * Image size upper 1280 * 960 are SCALING
 */
enum gc0310_downsize_mode {
	SUBSAMPLING,
	SCALING,
};


struct regval {
	u8      addr;
	u8      val;
};

struct gc0310_mode_info {
	enum gc0310_mode_id             id;
	enum gc0310_downsize_mode       dn_mode;
	u32                             hact;       // Active horizontal pixels
	u32                             htot;       // Total horizontal pixels
	u32                             vact;       // Active vertical pixels
	u32                             vtot;       // Total vertical pixels
};

struct gc0310 {
	//struct i2c_client               *client;

	//struct gpio_desc                *reset_gpio;
	//struct gpio_desc                *pwdn_gpio;
	//struct gpio_desc                *pwr_gpio;
	//struct gpio_desc                *pwr_snd_gpio;
	//struct pinctrl                  *pinctrl;
	//struct pinctrl_state            *pins_default;
	//struct pinctrl_state            *pins_sleep;

	//struct v4l2_subdev              subdev;
	struct v4l2_ctrl_handler        ctrl_handler;
	//struct v4l2_ctrl                *exposure;
	//struct v4l2_ctrl                *anal_gain;
	//struct v4l2_ctrl                *digi_gain;
	//struct v4l2_ctrl                *hblank;
	//struct v4l2_ctrl                *vblank;
	//struct v4l2_ctrl                *test_pattern;

	//struct mutex                    mutex;
	//bool                            streaming;

	//const struct gc0310_mode        *cur_mode;
	//struct sp_subdev_sensor_data    sensor_data;


	/*
	 * from ov5640 driver
	 */
	struct i2c_client               *i2c_client;
	struct v4l2_subdev              subdev;
	//struct media_pad pad;
	//struct v4l2_fwnode_endpoint ep; /* the parsed DT endpoint info */
	//struct clk *xclk; /* system clock to OV5640 */
	//u32 xclk_freq;

	//struct regulator_bulk_data supplies[OV5640_NUM_SUPPLIES];
	//struct gpio_desc *reset_gpio;
	//struct gpio_desc *pwdn_gpio;
	//bool   upside_down;

	/* lock to protect all members below */
	struct mutex                    lock;

	//int power_count;

	struct v4l2_mbus_framefmt       fmt;
	bool                            pending_fmt_change;

	const struct gc0310_mode_info   *current_mode;
	const struct gc0310_mode_info   *last_mode;
	enum gc0310_frame_rate          current_fr;
	struct v4l2_fract               frame_interval;

	//struct ov5640_ctrls ctrls;

	//u32 prev_sysclk, prev_hts;
	//u32 ae_low, ae_high, ae_target;

	bool                            pending_mode_change;
	bool                            streaming;
};

#endif
