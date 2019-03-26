/*
 * ov9281.c - ov9281 sensor driver
 *
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <mach/gpio_drv.h>


//#include "ov9281_mode_tbls.h"

/* OV9281 Registers */

//#define OV9281_SC_CHIP_ID_HIGH_ADDR	0x300A	
//#define OV9281_SC_CHIP_ID_LOW_ADDR	0x300B


#define OV9281_SC_CTRL_SCCB_ID_ADDR	0x302B

#if 0 //phtsai	not use
#define OV9281_SC_CTRL_3B_ADDR		0x303B		
#define OV9281_SC_CTRL_3B_SCCB_ID2_NACK_EN	(1 << 0)
#define OV9281_SC_CTRL_3B_SCCB_PGM_ID_EN	(1 << 1)

#define OV9281_GROUP_HOLD_ADDR		0x3208
#define OV9281_GROUP_HOLD_START		0x00
#define OV9281_GROUP_HOLD_END		0x10
#define OV9281_GROUP_HOLD_LAUNCH_LBLANK	0x60
#define OV9281_GROUP_HOLD_LAUNCH_VBLANK	0xA0
#define OV9281_GROUP_HOLD_LAUNCH_IMMED	0xE0
#define OV9281_GROUP_HOLD_BANK_0	0x00
#define OV9281_GROUP_HOLD_BANK_1	0x01

#define OV9281_EXPO_HIGH_ADDR		0x3500
#define OV9281_EXPO_MID_ADDR		0x3501
#define OV9281_EXPO_LOW_ADDR		0x3502

#define OV9281_GAIN_SHIFT_ADDR		0x3507
#define OV9281_GAIN_HIGH_ADDR		0x3508
#define OV9281_GAIN_LOW_ADDR		0x3509

#define OV9281_TIMING_VTS_HIGH_ADDR	0x380E
#define OV9281_TIMING_VTS_LOW_ADDR	0x380F
#define OV9281_TIMING_RST_FSIN_HIGH_ADDR	0x3826
#define OV9281_TIMING_RST_FSIN_LOW_ADDR	0x3827

#define OV9281_OTP_BUFFER_ADDR		0x3D00
#define OV9281_OTP_BUFFER_SIZE		32
#define OV9281_OTP_STR_SIZE		(OV9281_OTP_BUFFER_SIZE * 2)
#define OV9281_FUSE_ID_OTP_BUFFER_ADDR	0x3D00
#define OV9281_FUSE_ID_OTP_BUFFER_SIZE	16
#define OV9281_FUSE_ID_STR_SIZE		(OV9281_FUSE_ID_OTP_BUFFER_SIZE * 2)
#define OV9281_OTP_PROGRAM_CTRL_ADDR	0x3D80
#define OV9281_OTP_LOAD_CTRL_ADDR	0x3D81
#define OV9281_OTP_LOAD_CTRL_OTP_RD	0x01

#define OV9281_PRE_CTRL00_ADDR		0x5E00
#define OV9281_PRE_CTRL00_TEST_PATTERN_EN	(1 << 7)

/* OV9281 Other Stuffs */
#define OV9281_DEFAULT_GAIN		0x0010 /* 1.0x real gain */
#define OV9281_MIN_GAIN			0x0001
#define OV9281_MAX_GAIN			0x1FFF

#define OV9281_DEFAULT_FRAME_LENGTH	0x038E
#define OV9281_MIN_FRAME_LENGTH		0x0001
#define OV9281_MAX_FRAME_LENGTH		0xFFFF

#define OV9281_MIN_EXPOSURE_COARSE	0x00000001
#define OV9281_MAX_EXPOSURE_COARSE	0x000FFFFF
#define OV9281_DEFAULT_EXPOSURE_COARSE	0x00002A90

#define OV9281_DEFAULT_MODE		OV9281_MODE_1280X800
#define OV9281_DEFAULT_WIDTH		1280
#define OV9281_DEFAULT_HEIGHT		800
#define OV9281_DEFAULT_DATAFMT		MEDIA_BUS_FMT_SRGGB10_1X10
#define OV9281_DEFAULT_CLK_FREQ		24000000

#define OV9281_DEFAULT_I2C_ADDRESS_C0		(0xc0 >> 1)
#define OV9281_DEFAULT_I2C_ADDRESS_20		(0x20 >> 1)
#define OV9281_DEFAULT_I2C_ADDRESS_PROGRAMMABLE	(0xe0 >> 1)

static const char * const ov9281_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};
#endif

#define OV9281_NUM_SUPPLIES ARRAY_SIZE(ov9281_supply_names)

#define REG_NULL			0xFFFF
/* OV9281 Registers */
#define OV9281_REG_CTRL_MODE		0x0100
#define OV9281_MODE_SW_STANDBY		0x0
#define OV9281_MODE_STREAMING		BIT(0)

#define OV9281_REG_VALUE_08BIT		1
#define OV9281_REG_VALUE_16BIT		2
#define OV9281_REG_VALUE_24BIT		3

#define to_ov9281(sd) container_of(sd, struct ov9281, subdev)

#define OV9281_ADDR 0xC0

struct regval {
	u16 addr;
	u8 val;
};

struct ov9281_mode {
	u32 width;
	u32 height;
	u32 max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list;
};

struct ov9281 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	// add
	struct gpio_desc 	*pwr_gpio;
	struct gpio_desc	*pwr_snd_gpio;

//	struct regulator_bulk_data supplies[OV9281_NUM_SUPPLIES];

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_gain;
	struct v4l2_ctrl	*digi_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	bool			streaming;
	const struct ov9281_mode *cur_mode;
};

#if 1 //phtsai, from steve
static const struct regval ov9281_1280x800_regs[] = {
	{0x0103, 0x01}, {0x0302, 0x32}, {0x030d, 0x50}, {0x030e, 0x02},
	{0x3001, 0x00}, {0x3004, 0x00}, {0x3005, 0x00}, {0x3006, 0x04},
	{0x3011, 0x0a}, {0x3013, 0x18}, {0x301c, 0xf0}, {0x3022, 0x01},
	{0x3030, 0x10}, {0x3039, 0x32}, {0x303a, 0x00}, {0x3500, 0x00},
	{0x3501, 0x2a}, {0x3502, 0x90}, {0x3503, 0x08}, {0x3505, 0x8c},
	{0x3507, 0x03}, {0x3508, 0x00}, {0x3509, 0x10}, {0x3610, 0x80},
	{0x3611, 0xa0}, {0x3620, 0x6e}, {0x3632, 0x56}, {0x3633, 0x78},
	{0x3662, 0x05}, {0x3666, 0x00}, {0x366f, 0x5a}, {0x3680, 0x84},
	{0x3712, 0x80}, {0x372d, 0x22}, {0x3731, 0x80}, {0x3732, 0x30},
	{0x3778, 0x00}, {0x377d, 0x22}, {0x3788, 0x02}, {0x3789, 0xa4},
	{0x378a, 0x00}, {0x378b, 0x4a}, {0x3799, 0x20}, {0x3800, 0x00},
	{0x3801, 0x00}, {0x3802, 0x00}, {0x3803, 0x00}, {0x3804, 0x05}, ///// 3 90fps 5 120fps
	{0x3805, 0x0f}, {0x3806, 0x03}, {0x3807, 0x2f}, {0x3808, 0x05},
	{0x3809, 0x00}, {0x380a, 0x03}, {0x380b, 0x20}, {0x380c, 0x02},
	{0x380d, 0xd8}, {0x380e, 0x07}, {0x380f, 0x1c}, {0x3810, 0x00},
	{0x3811, 0x08}, {0x3812, 0x00}, {0x3813, 0x08}, {0x3814, 0x11},
	{0x3815, 0x11}, {0x3820, 0x40}, {0x3821, 0x00}, {0x382c, 0x05},
	{0x382d, 0xb0}, {0x389d, 0x00}, {0x3881, 0x42}, {0x3882, 0x01},
	{0x3883, 0x00}, {0x3885, 0x02}, {0x38a8, 0x02}, {0x38a9, 0x80},
	{0x38b1, 0x00}, {0x38b3, 0x02}, {0x38c4, 0x00}, {0x38c5, 0xc0},
	{0x38c6, 0x04}, {0x38c7, 0x80}, {0x3920, 0xff}, {0x4003, 0x40},
	{0x4008, 0x04}, {0x4009, 0x0b}, {0x400c, 0x00}, {0x400d, 0x07},
	{0x4010, 0x40}, {0x4043, 0x40}, {0x4307, 0x30}, {0x4317, 0x00},
	{0x4501, 0x00}, {0x4507, 0x00}, {0x4509, 0x00}, {0x450a, 0x08},
	{0x4601, 0x04}, {0x470f, 0x00}, {0x4f07, 0x00}, {0x4800, 0x00},
	{0x5000, 0x9f}, {0x5001, 0x00}, {0x5e00, 0x00}, {0x5d00, 0x07},
	{0x5d01, 0x00}, {0x4f00, 0x04}, {0x4f10, 0x00}, {0x4f11, 0x98},
	{0x4f12, 0x0f}, {0x4f13, 0xc4}, {0x0100, 0x01}, 
    // bist
	//{0x5e00, 0x80},
	{0x3501, 0x38},
	{0x3502, 0x20},
	{0x5000, 0x87},
	{ REG_NULL, 0x00}
};

#else
static const struct regval ov9281_1280x800_regs[] = {
	/* PLL control */
	{ 0x0302, 0x32 },
	{ 0x030d, 0x50 },
	{ 0x030e, 0x02 },

	/* system control */
	{ 0x3001, 0x00 },
	{ 0x3004, 0x00 },
	{ 0x3005, 0x00 },
	{ 0x3006, 0x04 },
	{ 0x3011, 0x0a },
	{ 0x3013, 0x18 },
	{ 0x3022, 0x01 },
	{ 0x3030, 0x10 },
	{ 0x3039, 0x32 },
	{ 0x303a, 0x00 },

	/* manual AEC/AGC */
	{ 0x3500, 0x00 },
	{ 0x3501, 0x2a },
	{ 0x3502, 0x90 },
	{ 0x3503, 0x08 },
	{ 0x3505, 0x8c },
	{ 0x3507, 0x03 },
	{ 0x3508, 0x00 },
	{ 0x3509, 0x10 },

	/* analog control */
	{ 0x3610, 0x80 },
	{ 0x3611, 0xa0 },
	{ 0x3620, 0x6e },
	{ 0x3632, 0x56 },
	{ 0x3633, 0x78 },
	{ 0x3662, 0x05 },
	{ 0x3666, 0x00 },
	{ 0x366f, 0x5a },
	{ 0x3680, 0x84 },

	/* sensor control */
	{ 0x3712, 0x80 },
	{ 0x372d, 0x22 },
	{ 0x3731, 0x80 },
	{ 0x3732, 0x30 },
	{ 0x3778, 0x00 },
	{ 0x377d, 0x22 },
	{ 0x3788, 0x02 },
	{ 0x3789, 0xa4 },
	{ 0x378a, 0x00 },
	{ 0x378b, 0x4a },
	{ 0x3799, 0x20 },

	/* timing control */
	{ 0x3800, 0x00 },
	{ 0x3801, 0x00 },
	{ 0x3802, 0x00 },
	{ 0x3803, 0x00 },
	{ 0x3804, 0x05 },
	{ 0x3805, 0x0f },
	{ 0x3806, 0x03 },
	{ 0x3807, 0x2f },
	{ 0x3808, 0x05 },
	{ 0x3809, 0x00 },
	{ 0x380a, 0x03 },
	{ 0x380b, 0x20 },
	{ 0x380c, 0x02 },
	{ 0x380d, 0xd8 },
	{ 0x380e, 0x03 },
	{ 0x380f, 0x8e },
	{ 0x3810, 0x00 },
	{ 0x3811, 0x08 },
	{ 0x3812, 0x00 },
	{ 0x3813, 0x08 },
	{ 0x3814, 0x11 },
	{ 0x3815, 0x11 },
	{ 0x3820, 0x40 },
	{ 0x3821, 0x00 },
	{ 0x3881, 0x42 },
	{ 0x38a8, 0x02 },
	{ 0x38a9, 0x80 },
	{ 0x38b1, 0x00 },
	{ 0x38c4, 0x00 },
	{ 0x38c5, 0xc0 },
	{ 0x38c6, 0x04 },
	{ 0x38c7, 0x80 },

	/* PWM and strobe control */
	{ 0x3920, 0xff },

	/* BLC control */
	{ 0x4003, 0x40 },
	{ 0x4008, 0x04 },
	{ 0x4009, 0x0b },
	{ 0x400c, 0x00 },
	{ 0x400d, 0x07 },
	{ 0x4010, 0x40 },
	{ 0x4043, 0x40 },

	/* format control */
	{ 0x4307, 0x30 },
	{ 0x4317, 0x00 },

	/* ???? */
	{ 0x4501, 0x00 },
	{ 0x4507, 0x00 },
	{ 0x4509, 0x00 },
	{ 0x450a, 0x08 },

	/* VFIFO control */
	{ 0x4601, 0x04 },

	/* DVP control */
	{ 0x470f, 0x00 },

	/* low power mode control */
	{ 0x4f07, 0x00 },

	/* MIPI top control */
	{ 0x4800, 0x00 }, /* bit 5: discontinuous clk */

	/* ISP top control */
	{ 0x5000, 0x9f },
	{ 0x5001, 0x00 },
	{ 0x5e00, 0x00 },

	/* ???? */
	{ 0x5d00, 0x07 },
	{ 0x5d01, 0x00 },
	{ REG_NULL, 0x00}
};
#endif

static const struct ov9281_mode supported_modes[] = {
	{
		.width = 1280,
		.height = 800,
		.max_fps = 120,
		.exp_def = 0x0600,
		.hts_def = 0x02D8,		//Timing_HTS, 0x380C/0x380D
		.vts_def = 0x038E,		//Timing_VTS, 0x380E/0x380F
		.reg_list = ov9281_1280x800_regs,
	},
};

/* Read registers up to 4 at a time */
//ret = ov9281_read_reg(ov9281->client, 0x0100, OV9281_REG_VALUE_08BIT, &val);
static int ov9281_read_reg(struct i2c_client *client, u16 reg,
			    unsigned int len, u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));

	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

/* Write registers up to 4 at a time */
static int ov9281_write_reg(struct i2c_client *client, u16 reg,
			     u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int ov9281_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = ov9281_write_reg(client, regs[i].addr,
					OV9281_REG_VALUE_08BIT,
					regs[i].val);

	return ret;
}

static int __ov9281_start_stream(struct ov9281 *ov9281)
{
	int ret;		

	ret = ov9281_write_array(ov9281->client, ov9281->cur_mode->reg_list);		
	if (ret)
		return ret;		

	/* In case these controls are set before streaming */
	mutex_unlock(&ov9281->mutex);
	ret = v4l2_ctrl_handler_setup(&ov9281->ctrl_handler);
	mutex_lock(&ov9281->mutex);
	if (ret)
		return ret;

	return ov9281_write_reg(ov9281->client,
				 OV9281_REG_CTRL_MODE,
				 OV9281_REG_VALUE_08BIT,
				 OV9281_MODE_STREAMING);
}

static int __ov9281_stop_stream(struct ov9281 *ov9281)
{
	return ov9281_write_reg(ov9281->client,
				 OV9281_REG_CTRL_MODE,
				 OV9281_REG_VALUE_08BIT,
				 OV9281_MODE_SW_STANDBY);
}

static int ov9281_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ov9281 *ov9281 = to_ov9281(sd);
	struct i2c_client *client = ov9281->client;
	int ret = 0;

	mutex_lock(&ov9281->mutex);
	on = !!on;
	if (on == ov9281->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __ov9281_start_stream(ov9281);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__ov9281_stop_stream(ov9281);
		pm_runtime_put(&client->dev);
	}

	ov9281->streaming = on;

unlock_and_return:
	mutex_unlock(&ov9281->mutex);

	return ret;
}

static struct v4l2_subdev_video_ops ov9281_subdev_video_ops = {
	.s_stream	= ov9281_s_stream,
};
static struct v4l2_subdev_ops ov9281_subdev_ops = {
	.video		= &ov9281_subdev_video_ops,
};

static int ov9281_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ov9281 *ov9281;
	struct v4l2_subdev *sd;
	int ret;

	ov9281 = devm_kzalloc(dev, sizeof(*ov9281), GFP_KERNEL);
	if (!ov9281)
		return -ENOMEM;

	ov9281->client = client;
	ov9281->cur_mode = &supported_modes[0];

	mutex_init(&ov9281->mutex);
	sd = &ov9281->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov9281_subdev_ops);
	
#if 0
	ret = ov9281_check_sensor_id(ov9281, client);
	if (ret)
		goto err_power_off;
#endif
		
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &ov9281_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

#if 0 //defined(CONFIG_MEDIA_CONTROLLER)
	ov9281->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	ret = media_entity_init(&sd->entity, 1, &ov9281->pad, 0);
	if (ret < 0)
		goto err_power_off;
#endif

	ret = v4l2_async_register_subdev(sd);		
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

#if 1 //phtsai, hardcode init
	ret = ov9281_write_array(ov9281->client, ov9281->cur_mode->reg_list);		
	if (ret)
		return ret;	
#endif

	return 0;
	
err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif

	return ret;	
}

static int ov9281_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9281 *ov9281 = to_ov9281(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev->entity);
#endif
	v4l2_ctrl_handler_free(&ov9281->ctrl_handler);
	mutex_destroy(&ov9281->mutex);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ov9281_of_match[] = {
	{ .compatible = "ovti,ov9281" },
	{},
};
MODULE_DEVICE_TABLE(of, ov9281_of_match);
#endif

static const struct i2c_device_id ov9281_match_id[] = {
	{ "ov9281", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ov9281_match_id);

static struct i2c_driver ov9281_i2c_driver = {
	.probe		= ov9281_probe,
	.remove		= ov9281_remove,
	.id_table	= ov9281_match_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "ov9281",
		.of_match_table = of_match_ptr(ov9281_of_match),
	},
};


static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&ov9281_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&ov9281_i2c_driver);
}

//device_initcall_sync(sensor_mod_init);
module_init(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("OmniVision ov9281 sensor driver");
MODULE_LICENSE("GPL v2");
