/*
 * imx219.c - imx219 Image Sensor Driver
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
#include "imx219.h"

// BAYER, RAW10
static const struct regval imx219_quxga_3280_2464_regs[] = {
	// 3280x2464/ raw10/ 15fps/ 2 lane
	{0x30EB, 0x05},	{0x30EB, 0x0C},	{0x300A, 0xFF},	{0x300B, 0xFF},
	{0x30EB, 0x05},	{0x30EB, 0x09},	{0x0114, 0x01},	{0x0128, 0x00},
	{0x012A, 0x18},	{0x012B, 0x00},	{0x0160, 0x09},	{0x0161, 0xC8},
	{0x0162, 0x0D},	{0x0163, 0x78},	{0x0164, 0x00},	{0x0165, 0x00},
	{0x0166, 0x0C},	{0x0167, 0xCF},	{0x0168, 0x00},	{0x0169, 0x00},
	{0x016A, 0x09},	{0x016B, 0x9F},	{0x016C, 0x0C},	{0x016D, 0xD0},
	{0x016E, 0x09},	{0x016F, 0xA0},	{0x0170, 0x01},	{0x0171, 0x01},
	{0x0172, 0x03},	{0x0174, 0x00},	{0x0175, 0x00},	{0x018C, 0x0A},
	{0x018D, 0x0A},	{0x0301, 0x05},	{0x0303, 0x01},	{0x0304, 0x03},
	{0x0305, 0x03},	{0x0306, 0x00},	{0x0307, 0x2B},	{0x0309, 0x0A},
	{0x030B, 0x01},	{0x030C, 0x00},	{0x030D, 0x55},	{0x455E, 0x00},
	{0x471E, 0x4B},	{0x4767, 0x0F},	{0x4750, 0x14},	{0x4540, 0x00},
	{0x47B4, 0x14},	{0x4713, 0x30},	{0x478B, 0x10},	{0x478F, 0x10},
	{0x4797, 0x0E},	{0x479B, 0x0E}, 
	{0x0100, 0x01},	{ REG_NULL, 0x00}
#if 0
	{0x0160, 0x04},	{0x0161, 0x59}, {0x0164, 0x02},	{0x0165, 0xA8},
	{0x0166, 0x0A},	{0x0167, 0x27},	{0x0168, 0x02},	{0x0169, 0xB4},
	{0x016A, 0x06},	{0x016B, 0xEB},	{0x016C, 0x07},	{0x016D, 0x80},
	{0x016E, 0x04},	{0x016F, 0x38},	
	{0x0172, xxx},
	{0x0307, 0x39},
	{0x030D, 0x72}
	{0x4793, 0x10},
#endif
};

static const struct regval imx219_1080p_1920_1080_regs [] = {
	// 1920x1080/ raw10/ 48fps/ 2 lane
	{0x30EB, 0x05},	{0x30EB, 0x0C},	{0x300A, 0xFF},	{0x300B, 0xFF},
	{0x30EB, 0x05},	{0x30EB, 0x09},	{0x0114, 0x01},	{0x0128, 0x00},
	{0x012A, 0x18},	{0x012B, 0x00},	{0x0160, 0x04},	{0x0161, 0x59},
	{0x0162, 0x0D},	{0x0163, 0x78},	{0x0164, 0x02},	{0x0165, 0xA8},
	{0x0166, 0x0A},	{0x0167, 0x27},	{0x0168, 0x02},	{0x0169, 0xB4},
	{0x016A, 0x06},	{0x016B, 0xEB},	{0x016C, 0x07},	{0x016D, 0x80},
	{0x016E, 0x04},	{0x016F, 0x38},	{0x0170, 0x01},	{0x0171, 0x01},
	{0x0172, 0x03},	{0x0174, 0x00},	{0x0175, 0x00},	{0x018C, 0x0A},
	{0x018D, 0x0A}, {0x0301, 0x05},	{0x0303, 0x01},	{0x0304, 0x03},
	{0x0305, 0x03},	{0x0306, 0x00},	{0x0307, 0x39},	{0x0309, 0x0A},
	{0x030B, 0x01},	{0x030C, 0x00},	{0x030D, 0x72},	{0x455E, 0x00},
	{0x471E, 0x4B},	{0x4767, 0x0F},	{0x4750, 0x14},	{0x4540, 0x00},
	{0x47B4, 0x14},	{0x4713, 0x30},	{0x478B, 0x10},	{0x478F, 0x10},
	{0x4793, 0x10},	{0x4797, 0x0E},	{0x479B, 0x0E},
	{0x0100, 0x01},	{ REG_NULL, 0x00}
};

struct imx219_mode_info imx219_mode_data[IMX219_NUM_MODES] = {
	{IMX219_MODE_1080P_1920_1080, SUBSAMPLING,
	 1920, 3280, 1080, 2464,
	 imx219_1080p_1920_1080_regs,
	 ARRAY_SIZE(imx219_1080p_1920_1080_regs)},
	{IMX219_MODE_QUXGA_3280_2464, SUBSAMPLING,
	 3280, 3280, 2464, 2464,
	 imx219_quxga_3280_2464_regs,
	 ARRAY_SIZE(imx219_quxga_3280_2464_regs)},
};

static const int imx219_framerates[] = {
	[IMX219_15_FPS] = 15,
	[IMX219_48_FPS] = 48,
};

static const struct regval imx219_start_settings[] = {
	{0x0100, 0x01},
	{REG_NULL, 0x00}
};

static const struct regval imx219_stop_settings[] = {
	{0x0100, 0x00},
	{REG_NULL, 0x00}
};

struct imx219_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct imx219_pixfmt imx219_formats[] = {
	{ MEDIA_BUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB, },
};

/* Read registers up to 4 at a time */
static int imx219_read_reg(struct i2c_client *client, u16 reg, unsigned int len, u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if ((len > 4) || !len)
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
static int imx219_write_reg(struct i2c_client *client, u16 reg, u32 len, u32 val)
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

static int imx219_write_array(struct i2c_client *client, const struct regval *regs)
{
	u32 i;
	int ret = 0;
	//int val;

	FUNC_DEBUG();

	for (i = 0; ((ret == 0) && (regs[i].addr != REG_NULL)); i++) {
		ret = imx219_write_reg(client, regs[i].addr,
					IMX219_REG_VALUE_08BIT,
					regs[i].val);
		//imx219_read_reg(client, regs[i].addr, IMX219_REG_VALUE_08BIT, &val);
		//	printk("i=%4d:, reg=%4x, val=%4x, rb_val=%4x\n",
		//	i, regs[i].addr, regs[i].val, val);
	}

	return ret;
}

static int imx219_set_mode(struct imx219 *imx219)
{
	const struct imx219_mode_info *mode = imx219->current_mode;
	const struct imx219_mode_info *orig_mode = imx219->last_mode;
	enum imx219_downsize_mode dn_mode, orig_dn_mode;
	unsigned long rate;
	int ret = 0;

	FUNC_DEBUG();
	DBG_INFO("%s, id: %d, dn_mode: %d, %dx%d\n", __FUNCTION__,
		mode->id, mode->dn_mode, mode->hact, mode->vact);

	dn_mode = mode->dn_mode;
	orig_dn_mode = orig_mode->dn_mode;

	/*
	 * All the formats we support have 16 bits per pixel, seems to require
	 * the same rate than YUV, so we can just use 16 bpp all the time.
	 */
	rate = mode->vtot * mode->htot * 16;
	rate *= imx219_framerates[imx219->current_fr];


	//if ((dn_mode == SUBSAMPLING && orig_dn_mode == SCALING) ||
	//    (dn_mode == SCALING && orig_dn_mode == SUBSAMPLING)) {
	//	/*
	//	 * change between subsampling and scaling
	//	 * go through exposure calculation
	//	 */
	//	ret = imx219_set_mode_exposure_calc(imx219, mode);
	//} else {
	//	/*
	//	 * change inside subsampling or scaling
	//	 * download firmware directly
	//	 */
	//	ret = imx219_set_mode_direct(imx219, mode);
	//}

	ret = imx219_write_array(imx219->i2c_client, mode->reg_data);

	imx219->pending_mode_change = false;
	imx219->last_mode = mode;

	return ret;
}

static int imx219_set_framefmt(struct imx219 *imx219,
			       struct v4l2_mbus_framefmt *format)
{
	int ret = 0;
	const struct regval *reg_list;

	FUNC_DEBUG();
	DBG_INFO("%s, format->code: 0x%04x\n", __FUNCTION__, format->code);

	switch (format->code) {
		case MEDIA_BUS_FMT_SBGGR8_1X8:
			/* SBGGR, RAW10 */
			reg_list = imx219_quxga_3280_2464_regs;
			break;

		default:
			return -EINVAL;
	}

	//ret = imx219_write_array(imx219->i2c_client, reg_list);

	return ret;
}

static int imx219_set_stream_mipi(struct imx219 *imx219, bool on)
{
	int ret;
	const struct regval *reg_list;

	FUNC_DEBUG();
	DBG_INFO("%s, on: %d\n", __FUNCTION__, on);

	if (on){
		reg_list = imx219_start_settings;
	} else {
		reg_list = imx219_stop_settings;
	}

	ret = imx219_write_array(imx219->i2c_client, reg_list);
	if (ret) {
		return ret;
	}

	return 0;
}

static int imx219_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx219 *imx219 = to_imx219(sd);
	int ret = 0;

	FUNC_DEBUG();
	DBG_INFO("%s, streaming: %d, pending_mode_change: %d, pending_fmt_change: %d\n",
			__FUNCTION__, imx219->streaming, imx219->pending_mode_change,
			imx219->pending_fmt_change);

	mutex_lock(&imx219->lock);

	if (imx219->streaming == !enable) {
		if (enable && imx219->pending_mode_change) {
			ret = imx219_set_mode(imx219);
			if (ret)
				goto out;
		}

		if (enable && imx219->pending_fmt_change) {
			ret = imx219_set_framefmt(imx219, &imx219->fmt);
			if (ret)
				goto out;
			imx219->pending_fmt_change = false;
		}

		//if (imx219->ep.bus_type == V4L2_MBUS_CSI2_DPHY)
			ret = imx219_set_stream_mipi(imx219, enable);
		//else
		//	ret = imx219_set_stream_dvp(imx219, enable);

		if (!ret)
			imx219->streaming = enable;
	}
out:
	if (ret) {
		DBG_ERR("Start streaming failed while write sensor registers!\n");
	}
	mutex_unlock(&imx219->lock);
	return ret;
}

static struct v4l2_subdev_video_ops imx219_subdev_video_ops = {
	.s_stream       = imx219_s_stream,
};

static const struct imx219_mode_info *
imx219_find_mode(struct imx219 *imx219, enum imx219_frame_rate fr,
		 int width, int height, bool nearest)
{
	const struct imx219_mode_info *mode;

	mode = v4l2_find_nearest_size(imx219_mode_data,
				      ARRAY_SIZE(imx219_mode_data),
				      hact, vact,
				      width, height);

	DBG_INFO("%s, mode: %px, width: %d, height: %d, nearest: %d\n",
		__FUNCTION__, mode, width, height, nearest);

	if (!mode ||
	    (!nearest && (mode->hact != width || mode->vact != height)))
		return NULL;

	/* 3280x2464 (QUXGA) can operate at max framerate 15fps */
	if (fr == IMX219_15_FPS &&
	    !(mode->hact <= 3280 && mode->vact <= 2464))
		return NULL;

	/* 1920x1080 (1080P) can operate at max frame rate 48fps */
	if (fr == IMX219_48_FPS &&
	    !(mode->hact <= 1920 && mode->vact <= 1080))
		return NULL;

	return mode;
}

static int imx219_enum_mbus_code(struct v4l2_subdev *sd,
								struct v4l2_subdev_pad_config *cfg,
								struct v4l2_subdev_mbus_code_enum *code)
{
	FUNC_DEBUG();

	if (code->pad != 0)
		return -EINVAL;
	if (code->index >= ARRAY_SIZE(imx219_formats))
		return -EINVAL;

	code->code = imx219_formats[code->index].code;

	DBG_INFO("%s, index: %d, code: 0x%04x\n",
		__FUNCTION__, code->index, code->code);
	return 0;
}

static int imx219_enum_frame_size(struct v4l2_subdev *sd,
								struct v4l2_subdev_pad_config *cfg,
								struct v4l2_subdev_frame_size_enum *fse)
{
	FUNC_DEBUG();

	if (fse->pad != 0)
		return -EINVAL;
	if (fse->index >= IMX219_NUM_MODES)
		return -EINVAL;

	fse->min_width = imx219_mode_data[fse->index].hact;
	fse->max_width = fse->min_width;
	fse->min_height = imx219_mode_data[fse->index].vact;
	fse->max_height = fse->min_height;

	DBG_INFO("%s, index: %d, min_w: %d, max_w: %d, min_h: %d, max_h: %d\n",
		__FUNCTION__, fse->index,
		fse->min_width, fse->max_width,
		fse->min_height, fse->max_height);
	return 0;
}

static int imx219_try_frame_interval(struct imx219 *imx219,
				     struct v4l2_fract *fi,
				     u32 width, u32 height)
{
	const struct imx219_mode_info *mode;
	enum imx219_frame_rate rate = IMX219_15_FPS;
	int minfps, maxfps, best_fps, fps;
	int i;

	minfps = imx219_framerates[IMX219_15_FPS];
	maxfps = imx219_framerates[IMX219_48_FPS];

	if (fi->numerator == 0) {
		fi->denominator = maxfps;
		fi->numerator = 1;
		rate = IMX219_48_FPS;
		goto find_mode;
	}

	fps = clamp_val(DIV_ROUND_CLOSEST(fi->denominator, fi->numerator),
			minfps, maxfps);

	DBG_INFO("%s, fps: %d, numerator: %d, denominator = %d\n",
		__FUNCTION__, fps, fi->numerator, fi->denominator);

	best_fps = minfps;
	for (i = 0; i < ARRAY_SIZE(imx219_framerates); i++) {
		int curr_fps = imx219_framerates[i];

		if (abs(curr_fps - fps) < abs(best_fps - fps)) {
			best_fps = curr_fps;
			rate = i;
		}
	}

	fi->numerator = 1;
	fi->denominator = best_fps;

find_mode:
	mode = imx219_find_mode(imx219, rate, width, height, false);
	return mode ? rate : -EINVAL;
}

static int imx219_enum_frame_interval(
	struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx219 *imx219 = to_imx219(sd);
	struct v4l2_fract tpf;
	int ret;

	FUNC_DEBUG();

	if (fie->pad != 0)
		return -EINVAL;
	if (fie->index >= IMX219_NUM_FRAMERATES)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = imx219_framerates[fie->index];

	ret = imx219_try_frame_interval(imx219, &tpf,
					fie->width, fie->height);
	if (ret < 0)
		return -EINVAL;

	DBG_INFO("%s, index: %d, numerator: %d, denominator = %d\n",
		__FUNCTION__, fie->index, tpf.numerator, tpf.denominator);

	fie->interval = tpf;
	return 0;
}

static int imx219_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct imx219 *imx219 = to_imx219(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&imx219->lock);

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt = v4l2_subdev_get_try_format(&imx219->subdev, cfg,
						 format->pad);
	else
		fmt = &imx219->fmt;
#else
	fmt = &imx219->fmt;
#endif

	format->format = *fmt;

	mutex_unlock(&imx219->lock);

	return 0;
}

static int imx219_try_fmt_internal(struct v4l2_subdev *sd,
				   struct v4l2_mbus_framefmt *fmt,
				   enum imx219_frame_rate fr,
				   const struct imx219_mode_info **new_mode)
{
	struct imx219 *imx219 = to_imx219(sd);
	const struct imx219_mode_info *mode;
	int i;

	FUNC_DEBUG();

	mode = imx219_find_mode(imx219, fr, fmt->width, fmt->height, true);
	if (!mode)
		return -EINVAL;
	fmt->width = mode->hact;
	fmt->height = mode->vact;

	if (new_mode)
		*new_mode = mode;

	for (i = 0; i < ARRAY_SIZE(imx219_formats); i++)
		if (imx219_formats[i].code == fmt->code)
			break;
	if (i >= ARRAY_SIZE(imx219_formats))
		i = 0;

	fmt->code = imx219_formats[i].code;
	fmt->colorspace = imx219_formats[i].colorspace;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);

	DBG_INFO("%s, code: 0x%04x, width: %d, height: %d\n",
		__FUNCTION__, fmt->code, fmt->width, fmt->height);

	return 0;
}

static int imx219_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct imx219 *imx219 = to_imx219(sd);
	const struct imx219_mode_info *new_mode;
	struct v4l2_mbus_framefmt orig_fmt = imx219->fmt;
	struct v4l2_mbus_framefmt *mbus_fmt = &format->format;
	struct v4l2_mbus_framefmt *fmt;
	int ret;

	FUNC_DEBUG();
	DBG_INFO("%s, mbus_fmt code: 0x%04x, %dx%d\n",
			__FUNCTION__, mbus_fmt->code, mbus_fmt->width, mbus_fmt->height);

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&imx219->lock);

	if (imx219->streaming) {
		ret = -EBUSY;
		goto out;
	}

	ret = imx219_try_fmt_internal(sd, mbus_fmt,
				      imx219->current_fr, &new_mode);
	if (ret)
		goto out;

	DBG_INFO("%s, mbus_fmt->code: 0x%04x, imx219->fmt.code: 0x%04x\n",
			__FUNCTION__, mbus_fmt->code, imx219->fmt.code);

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt = v4l2_subdev_get_try_format(sd, cfg, 0);
	else
		fmt = &imx219->fmt;
#else
	fmt = &imx219->fmt;
#endif

	*fmt = *mbus_fmt;

	if (new_mode != imx219->current_mode) {
		imx219->current_mode = new_mode;
		imx219->pending_mode_change = true;
	}
	if ((mbus_fmt->code != imx219->fmt.code) ||
		(orig_fmt.code != imx219->fmt.code))
		imx219->pending_fmt_change = true;

	DBG_INFO("%s, code: 0x%04x, pending_mode_change: %d, pending_fmt_change: %d\n",
			__FUNCTION__, imx219->fmt.code, imx219->pending_mode_change,
			imx219->pending_fmt_change);

out:
	mutex_unlock(&imx219->lock);
	return ret;
}

static struct v4l2_subdev_pad_ops imx219_subdev_pad_ops = {
	.enum_mbus_code      = imx219_enum_mbus_code,
	.get_fmt             = imx219_get_fmt,
	.set_fmt             = imx219_set_fmt,
	.enum_frame_size     = imx219_enum_frame_size,
	.enum_frame_interval = imx219_enum_frame_interval,
};

static struct v4l2_subdev_ops imx219_subdev_ops = {
	.video          = &imx219_subdev_video_ops,
	.pad            = &imx219_subdev_pad_ops,
};

static int imx219_check_sensor_id(struct imx219 *imx219, struct i2c_client *client)
{
	u32 val = 0;
	int ret;

	ret = imx219_read_reg(client, IMX219_REG_CHIP_ID,
			      IMX219_REG_VALUE_16BIT, &val);
	if ((ret != 0) || (val != CHIP_ID)) {
		DBG_ERR("Unexpected sensor (id = 0x%04x, ret = %d)!\n", val, ret);
		return -1;
	}
	DBG_INFO("Check sensor id success (id = 0x%04x).\n", val);

	return 0;
}

static int imx219_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct imx219 *imx219;
	struct v4l2_subdev *sd;
	int ret;

	FUNC_DEBUG();

	imx219 = devm_kzalloc(dev, sizeof(*imx219), GFP_KERNEL);
	if (!imx219) {
		DBG_ERR("Failed to allocate memory for \'imx219\'!\n");
		return -ENOMEM;
	}

	imx219->i2c_client = client;

	mutex_init(&imx219->lock);

	sd = &imx219->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx219_subdev_ops);
	DBG_INFO("Initialized V4L2 I2C subdevice.\n");

	ret = imx219_check_sensor_id(imx219, client);
	if (ret) {
		return ret;
	}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx219_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

	// Let IMX290 go to standby mode
	imx219_write_reg(imx219->i2c_client, IMX219_REG_CTRL_MODE,
				 IMX219_REG_VALUE_08BIT, IMX219_MODE_SW_STANDBY);

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

static int imx219_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_imx219(sd);

	FUNC_DEBUG();

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev->entity);
#endif
	v4l2_ctrl_handler_free(&imx219->ctrl_handler);
	mutex_destroy(&imx219->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx219_of_match[] = {
	{ .compatible = "imx219" },
	{},
};
MODULE_DEVICE_TABLE(of, imx219_of_match);
#endif

static const struct i2c_device_id imx219_match_id[] = {
	{ "imx219", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, imx219_match_id);

static struct i2c_driver imx219_i2c_driver = {
	.probe          = imx219_probe,
	.remove         = imx219_remove,
	.id_table       = imx219_match_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "imx219",
		.of_match_table = of_match_ptr(imx219_of_match),
	},
};

module_i2c_driver(imx219_i2c_driver);

MODULE_DESCRIPTION("Sunplus imx219 sensor driver");
MODULE_LICENSE("GPL v2");
