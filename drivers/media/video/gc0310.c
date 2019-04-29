/*
 * gc0310.c - gc0310 Image Sensor Driver
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
#include "gc0310.h"


static const struct regval gc0310_640x480_regs[] = {
	// system reg
	{0xfe,0xf0}, {0xfe,0xf0}, {0xfe,0x00}, {0xfc,0x0e},
	{0xfc,0x0e}, {0xf2,0x80}, {0xf3,0x00}, {0xf7,0x1b},
	{0xf8,0x04}, {0xf9,0x8e}, {0xfa,0x11},

	// MIPI reg
	{0xfe,0x03}, {0x40,0x08}, {0x42,0x00}, {0x43,0x00},
	{0x01,0x03}, {0x10,0x84},

	{0x01,0x03}, {0x02,0x11}, {0x03,0x94}, {0x04,0x01},
	{0x05,0x00}, {0x06,0x80}, {0x11,0x1e}, {0x12,0x00},
	{0x13,0x05}, {0x15,0x10}, {0x21,0x10}, {0x22,0x01},
	{0x23,0x10}, {0x24,0x02}, {0x25,0x10}, {0x26,0x03},
	{0x29,0x02}, {0x2a,0x0a}, {0x2b,0x04}, {0xfe,0x00},

	// CISCTL reg
	{0x00,0x2f}, {0x01,0x0f}, {0x02,0x04}, {0x03,0x03},
	{0x04,0x50}, {0x09,0x00}, {0x0a,0x00}, {0x0b,0x00},
	{0x0c,0x04}, {0x0d,0x01}, {0x0e,0xe8}, {0x0f,0x02},
	{0x10,0x88}, {0x16,0x00}, {0x17,0x14}, {0x18,0x1a},
	{0x19,0x14}, {0x1b,0x48}, {0x1c,0x1c}, {0x1e,0x6b},
	{0x1f,0x28}, {0x20,0x8b}, {0x21,0x49}, {0x22,0xb0},
	{0x23,0x04}, {0x24,0x16}, {0x34,0x20},
};


static const struct gc0310_mode supported_modes[] = {
	{
		.width = 640,
		.height = 480,
		.max_fps = 30,
		.exp_def = 0x0600,
		.hts_def = 0x02D8,              //Timing_HTS, 0x380C/0x380D
		.vts_def = 0x038E,              //Timing_VTS, 0x380E/0x380F
		.reg_list = gc0310_640x480_regs,
		.reg_num = ARRAY_SIZE(gc0310_640x480_regs),
	},
};

/* Read registers up to 4 at a time */
static int gc0310_read_reg(struct i2c_client *client, u8 reg, unsigned int len, u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	int ret;

	if ((len > 4) || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

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
static int gc0310_write_reg(struct i2c_client *client, u8 reg, u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 1;
	val_i = 4 - len;

	while (val_i < 4) {
		buf[buf_i++] = val_p[val_i++];
	}

	if (i2c_master_send(client, buf, len + 1) != len + 1)
		return -EIO;

	return 0;
}

static int gc0310_write_array(struct i2c_client *client, const struct regval *regs, int num)
{
	u32 i;
	int ret = 0;

	FUNC_DEBUG();

	for (i = 0; ((ret == 0) && (i < num)); i++) {
		ret = gc0310_write_reg(client, regs[i].addr,
					GC0310_REG_VALUE_08BIT,
					regs[i].val);
	}

	return ret;
}

static int __gc0310_start_stream(struct gc0310 *gc0310)
{
	int ret;

	FUNC_DEBUG();

	ret = gc0310_write_array(gc0310->client, gc0310->cur_mode->reg_list, gc0310->cur_mode->reg_num);
	if (ret) {
		return ret;
	}

	return gc0310_write_reg(gc0310->client,
				 GC0310_REG_CTRL_MODE,
				 GC0310_REG_VALUE_08BIT,
				 GC0310_MODE_STREAMING);
}

static int __gc0310_stop_stream(struct gc0310 *gc0310)
{
	FUNC_DEBUG();

	return gc0310_write_reg(gc0310->client,
				 GC0310_REG_CTRL_MODE,
				 GC0310_REG_VALUE_08BIT,
				 GC0310_MODE_SW_STANDBY);
}

static int gc0310_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc0310 *gc0310 = to_gc0310(sd);
	int ret = 0;

	FUNC_DEBUG();

	mutex_lock(&gc0310->mutex);
	//on = !!on;
	if (on == gc0310->streaming)
		goto unlock_and_return;

	if (on) {
		ret = __gc0310_start_stream(gc0310);
		if (ret) {
			DBG_ERR("Start streaming failed while write sensor registers!\n");
			goto unlock_and_return;
		}
	} else {
		__gc0310_stop_stream(gc0310);
	}

	gc0310->streaming = on;

unlock_and_return:
	mutex_unlock(&gc0310->mutex);

	return ret;
}

static struct v4l2_subdev_video_ops gc0310_subdev_video_ops = {
	.s_stream       = gc0310_s_stream,
};
static struct v4l2_subdev_ops gc0310_subdev_ops = {
	.video          = &gc0310_subdev_video_ops,
};

static int gc0310_check_sensor_id(struct gc0310 *gc0310, struct i2c_client *client)
{
	u32 val = 0;
	int ret;

	ret = gc0310_read_reg(client, GC0310_REG_CHIP_ID,
			      GC0310_REG_VALUE_16BIT, &val);
	if (val != CHIP_ID) {
		DBG_ERR("Unexpected sensor (id = 0x%04x, ret = %d)!\n", val, ret);
		return -1;
	}
	DBG_INFO("Check sensor id success (id = 0x%04x).\n", val);

	return 0;
}

static int gc0310_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct gc0310 *gc0310;
	struct v4l2_subdev *sd;
	int ret;

	FUNC_DEBUG();

	gc0310 = devm_kzalloc(dev, sizeof(*gc0310), GFP_KERNEL);
	if (!gc0310) {
		DBG_ERR("Failed to allocate memory for \'gc0310\'!\n");
		return -ENOMEM;
	}

	gc0310->client = client;
	gc0310->cur_mode = &supported_modes[0];

	mutex_init(&gc0310->mutex);

	sd = &gc0310->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc0310_subdev_ops);
	DBG_INFO("Initialized V4L2 I2C subdevice.\n");

	ret = gc0310_check_sensor_id(gc0310, client);
	if (ret) {
		return ret;
	}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc0310_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

	ret = v4l2_async_register_subdev(sd);
	if (ret) {
		DBG_ERR("V4L2 async register subdevice failed.\n");
		goto err_clean_entity;
	}
	DBG_INFO("Registered V4L2 sub-device successfully.\n");

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif

	return ret;
}

static int gc0310_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0310 *gc0310 = to_gc0310(sd);

	FUNC_DEBUG();

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev->entity);
#endif
	v4l2_ctrl_handler_free(&gc0310->ctrl_handler);
	mutex_destroy(&gc0310->mutex);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc0310_of_match[] = {
	{ .compatible = "ovti,gc0310" },
	{},
};
MODULE_DEVICE_TABLE(of, gc0310_of_match);
#endif

static const struct i2c_device_id gc0310_match_id[] = {
	{ "gc0310", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, gc0310_match_id);

static struct i2c_driver gc0310_i2c_driver = {
	.probe          = gc0310_probe,
	.remove         = gc0310_remove,
	.id_table       = gc0310_match_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "gc0310",
		.of_match_table = of_match_ptr(gc0310_of_match),
	},
};


static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc0310_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc0310_i2c_driver);
}

module_init(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sunplus gc0310 sensor driver");
MODULE_LICENSE("GPL v2");
