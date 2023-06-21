#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#include "eys3d_api.h"
#include "eys3d_mode_tbls.h"

#define EYS3D_WINDOW_WIDTH_MIN     32U
#define EYS3D_WINDOW_HEIGHT_MIN    32U
#define EYS3D_LINK_FREQ_420MHZ     420000000
#define EYS3D_PIXEL_RATE           (45 * 1000 * 1000 * 4)

static u32 supported_camera_pixel_codes[] = {
    MEDIA_BUS_FMT_UYVY8_2X8,
    MEDIA_BUS_FMT_UYVY8_1X16,
};

static const s64 link_freq_menu_items[] = {
    EYS3D_LINK_FREQ_420MHZ
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id eys3d_of_match[] = {
    {.compatible = "eys3d,eSP876"},
    {},
};

MODULE_DEVICE_TABLE(of, eys3d_of_match);
#endif

static const struct i2c_device_id eys3d_match_id[] = {
    {EYS3D_I2C_DEVICE_ID_NAME, 0},
    {},
};

static int __eys3d_power_on(struct eys3d *priv)
{
    struct device *dev = NULL;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    dev = &priv->client->dev;

    (void)dev;

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static void __eys3d_power_off(struct eys3d *priv)
{
    struct device *dev = NULL;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    dev = &priv->client->dev;

    (void)dev;

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
}

static int __eys3d_start_stream(struct eys3d *priv)
{
    const struct eys3d_video_mode *mode = NULL;
    struct i2c_client* client = NULL;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    u8 val = 0;
    u16 reg = 0;

    (void)val;
    (void)reg;

    EYS3D_ALERT_DBG_PRT("Enter\n");

    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    client = priv->client;
    if (!client) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = eys3d_api_get_current_video_mode(priv);
    if (!mode) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    EYS3D_API_DBG_PRT("[0x%08X][%d, %d, %d, %d, %d]\n", mode->mode_index, \
        (int)mode->width, \
        (int)mode->height, \
        (int)mode->fps, (int)mode->is_interleave_mode, (int)mode->is_rectify);

    ret = eys3d_api_start_stream(priv);

exit:
    EYS3D_ALERT_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static int __eys3d_stop_stream(struct eys3d *priv)
{
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    EYS3D_ALERT_DBG_PRT("Enter\n");

    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    ret = eys3d_api_stop_stream(priv);

exit:
    EYS3D_ALERT_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static struct v4l2_rect *__eys3d_get_pad_crop(struct eys3d *priv, struct v4l2_subdev_pad_config *cfg,
              unsigned int pad, u32 which)
{
    switch (which) {
        case V4L2_SUBDEV_FORMAT_TRY:
            return v4l2_subdev_get_try_crop(&priv->subdev, cfg, pad);
        case V4L2_SUBDEV_FORMAT_ACTIVE:
            return &priv->crop;
        default:
            return NULL;
    }
}

static struct v4l2_mbus_framefmt *__eys3d_get_pad_format(struct eys3d *priv,
            struct v4l2_subdev_pad_config *cfg,
            unsigned int pad, u32 which)
{
    switch (which) {
        case V4L2_SUBDEV_FORMAT_TRY:
            return v4l2_subdev_get_try_format(&priv->subdev, cfg, pad);
        case V4L2_SUBDEV_FORMAT_ACTIVE:
            return &priv->format;
        default:
            return NULL;
    }
}

static int __eys3d_initialize_controls(struct eys3d *priv)
{
    struct v4l2_ctrl_handler *handler;
    struct v4l2_ctrl *ctrl;
    int ret = EYS3D_API_RET_OK;

    handler = &priv->ctrl_handler;
    ret = v4l2_ctrl_handler_init(handler, 8);
    if (ret)
        return ret;
    
    handler->lock = &priv->eys3d_mutex;

    ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
                      0, 0, link_freq_menu_items);
    if (ctrl)
        ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

    v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
              0, EYS3D_PIXEL_RATE, 1, EYS3D_PIXEL_RATE);

    if (handler->error) {
        ret = handler->error;
        dev_err(&priv->client->dev,
            "Failed to init controls(%d)\n", ret);
        goto err_free_handler;
    }

    priv->subdev.ctrl_handler = handler;

    return ret;

err_free_handler:
    v4l2_ctrl_handler_free(handler);

    return ret;
}

static int eys3d_runtime_suspend(struct device *dev)
{
    struct i2c_client* client = to_i2c_client(dev);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *priv = sd_to_eys3d(sd);
    int ret = EYS3D_API_RET_OK;

    __eys3d_power_off(priv);

    return ret;
}

static int eys3d_runtime_resume(struct device *dev)
{
    struct i2c_client* client = to_i2c_client(dev);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *priv = sd_to_eys3d(sd);

    return __eys3d_power_on(priv);
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int eys3d_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
    struct eys3d *priv = sd_to_eys3d(sd);
    struct v4l2_mbus_framefmt *try_fmt =
                v4l2_subdev_get_try_format(sd, fh->pad, 0);
    const struct eys3d_video_mode *def_mode = NULL;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!priv) {
        EYS3D_ERR_DBG_PRT("eYs3D Camera is NULL\n");
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    def_mode = eys3d_api_get_current_video_mode(priv);
    if (!def_mode) {
        EYS3D_ERR_DBG_PRT("eys3d_api_get_current_video_mode fail\n");
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mutex_lock(&priv->eys3d_mutex);
    /* Initialize try_fmt */
    EYS3D_API_DBG_PRT("Width %d, Height %d\n", def_mode->width, def_mode->height);
    try_fmt->width = def_mode->width;
    try_fmt->height = def_mode->height;
    try_fmt->code = def_mode->pixelcode;
    if (!def_mode->is_interleave_mode)
        try_fmt->field = V4L2_FIELD_ANY;
    else
        try_fmt->field = V4L2_FIELD_INTERLACED;
    mutex_unlock(&priv->eys3d_mutex);

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}
#endif

static int eys3d_s_stream(struct v4l2_subdev *sd, int on)
{
    struct eys3d *priv = sd_to_eys3d(sd);
    struct i2c_client* client = NULL;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    (void)client;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    client = priv->client;

    mutex_lock(&priv->eys3d_mutex);
    if (on == priv->streaming) {
        ret_line = __LINE__;
        goto unlock_and_return;
    }

    if (on) {
        ret = pm_runtime_get_sync(&client->dev);
        if (ret < 0) {
            pm_runtime_put_noidle(&client->dev);
            ret_line = __LINE__;
            goto unlock_and_return;
        }

        ret = __eys3d_start_stream(priv);
        if (ret) {
            EYS3D_ERR_DBG_PRT("Failed to start stream(%d)\n", ret);
            ret_line = __LINE__;
            goto unlock_and_return;
        }
    } else {
        __eys3d_stop_stream(priv);
        pm_runtime_put(&client->dev);
    }

    priv->streaming = on;

    EYS3D_API_DBG_PRT("Stream [%d]\n", on);

unlock_and_return:
    mutex_unlock(&priv->eys3d_mutex);

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static int eys3d_g_frame_interval(struct v4l2_subdev *sd,
                   struct v4l2_subdev_frame_interval *fi)
{
    struct eys3d *priv = sd_to_eys3d(sd);
    const struct eys3d_video_mode *mode = NULL;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = eys3d_api_get_current_video_mode(priv);
    if (!mode) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mutex_lock(&priv->eys3d_mutex);
    fi->interval.numerator = 1;
    fi->interval.denominator = mode->fps;
    mutex_unlock(&priv->eys3d_mutex);

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static int eys3d_enum_mbus_code(struct v4l2_subdev *sd,
                 struct v4l2_subdev_pad_config *cfg,
                 struct v4l2_subdev_mbus_code_enum *code)
{
    struct eys3d *priv = sd_to_eys3d(sd);
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    (void)priv;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!code) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (code->index >= ARRAY_SIZE(supported_camera_pixel_codes)) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    code->code = supported_camera_pixel_codes[code->index];

exit:
    EYS3D_API_DBG_PRT("Leave(%d, 0x%08X, %d, %d)\n", code->index, code->code, ret, ret_line);

    return ret;
}

static int eys3d_enum_frame_sizes(struct v4l2_subdev *sd,
                   struct v4l2_subdev_pad_config *cfg,
                   struct v4l2_subdev_frame_size_enum *fse)
{
    struct eys3d *priv = sd_to_eys3d(sd);
    const struct eys3d_video_mode *mode = NULL;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    (void)priv;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!fse) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = eys3d_api_get_video_mode_by_index(priv, fse->index);
    if (!mode) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    fse->min_width  = mode->width;
    fse->max_width  = mode->width;
    fse->min_height = mode->height;
    fse->max_height = mode->height;

    EYS3D_API_DBG_PRT("[%d][0x%08X, 0x%08X]\n", fse->index, fse->code, mode->pixelcode);
    EYS3D_API_DBG_PRT("[%d][%d x %d][%d x %d]\n", fse->index, fse->min_width, fse->min_height, fse->max_width, fse->max_height);

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static int eys3d_get_fmt(struct v4l2_subdev *sd,
              struct v4l2_subdev_pad_config *cfg,
              struct v4l2_subdev_format *fmt)
{
    struct eys3d *priv = sd_to_eys3d(sd);
    const struct eys3d_video_mode *mode = NULL;
    int ret = EYS3D_API_RET_OK;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!priv) {
        ret = -EINVAL;
        goto exit;
    }

    mode = eys3d_api_get_current_video_mode(priv);
    if (!mode) {
        ret = -EINVAL;
        goto exit;
    }

    mutex_lock(&priv->eys3d_mutex);
    if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
        fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
        EYS3D_API_DBG_PRT("CONFIG_VIDEO_V4L2_SUBDEV_API\n");
#else
        mutex_unlock(&priv->eys3d_mutex);
        EYS3D_API_DBG_PRT("Leave\n");
        return -ENOTTY;
#endif
    } else {
        fmt->format.width = mode->width;
        fmt->format.height = mode->height;
        fmt->format.code = mode->pixelcode;
        if (!mode->is_interleave_mode)
            fmt->format.field = V4L2_FIELD_ANY;
        else
            fmt->format.field = V4L2_FIELD_INTERLACED;
    }

    EYS3D_API_DBG_PRT("[%d][%d, %d, %d]\n",mode->mode_index, mode->width, mode->height, mode->fps);
    mutex_unlock(&priv->eys3d_mutex);

exit:
    EYS3D_API_DBG_PRT("Leave[%d]\n", ret);

    return ret;
}

static int eys3d_set_fmt(struct v4l2_subdev *sd,
              struct v4l2_subdev_pad_config *cfg,
              struct v4l2_subdev_format *format)
{
    struct eys3d *priv = sd_to_eys3d(sd);
    struct v4l2_mbus_framefmt *__format;
    struct v4l2_rect *__crop;
    const struct eys3d_video_mode *mode = NULL;
    unsigned int width;
    unsigned int height;
    unsigned int hratio;
    unsigned int vratio;
    int ret = EYS3D_API_RET_OK;

    EYS3D_API_DBG_PRT("Enter, Set Format[%u x %u]\n", format->format.width, format->format.height);
#if 0
    __crop = __eys3d_get_pad_crop(eys3d, cfg, format->pad, format->which);
    EYS3D_API_DBG_PRT("Crop[%u, %u, %u, %u]\n",
        __crop->left, __crop->top, __crop->width, __crop->height);

    /* Clamp the width and height to avoid dividing by zero. */
    width = clamp_t(unsigned int, ALIGN(format->format.width, 2),
            max(__crop->width / 3, EYS3D_WINDOW_WIDTH_MIN),
            __crop->width);
    height = clamp_t(unsigned int, ALIGN(format->format.height, 2),
            max(__crop->height / 3, EYS3D_WINDOW_HEIGHT_MIN),
             __crop->height);
    hratio = DIV_ROUND_CLOSEST(__crop->width, width);
    vratio = DIV_ROUND_CLOSEST(__crop->height, height);

    __format = __eys3d_get_pad_format(eys3d, cfg, format->pad, format->which);
    __format->width = __crop->width / hratio;
    __format->height = __crop->height / vratio;

    if (__format->width == 0 || __format->width == 0) {
        __format->width = format->format.width;
        __format->height = format->format.height;
    }

    format->format = *__format;
#endif
    mode = eys3d_api_get_current_video_mode(priv);
    EYS3D_ALERT_DBG_PRT("Current Format: [%d](%d x %d, %d)\n", mode->mode_index, mode->width, mode->height, mode->fps);

    if (format->format.width != mode->width && format->format.height != mode->height) {
        if (eys3d_api_eys3d_set_current_vid_mode(priv, format->format.width, format->format.height) == 0) {
            if (mode) {
                EYS3D_ALERT_DBG_PRT("[%d](%d x %d, %d)\n", mode->mode_index, mode->width, mode->height, mode->fps);
            } else {
                mode = eys3d_api_get_current_video_mode(priv);
                EYS3D_ERR_DBG_PRT("Not Found mode! Current Format: [%d](%d x %d, %d)\n", mode->mode_index, mode->width, mode->height, mode->fps);
            }
        } else {
            mode = eys3d_api_get_current_video_mode(priv);
            EYS3D_ERR_DBG_PRT("Not Found mode! Current Format: [%d](%d x %d, %d)\n", mode->mode_index, mode->width, mode->height, mode->fps);
        }
    }

    EYS3D_API_DBG_PRT("Leave[%d]\n", ret);

    return ret;
}

static int eys3d_enum_frameintervals(struct v4l2_subdev *sd,
        struct v4l2_subdev_pad_config *cfg,
        struct v4l2_subdev_frame_interval_enum *fie)
{
    struct eys3d *priv = sd_to_eys3d(sd);
    const struct eys3d_video_mode *mode = NULL;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    EYS3D_API_DBG_PRT("Enter\n");

    (void)priv;

    if (!fie) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = eys3d_api_get_current_video_mode(priv);
    if (!mode) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (fie->index >= mode->support_fps_num) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    fie->interval.numerator = 1;
    fie->interval.denominator = mode->support_fps[fie->index];

    EYS3D_API_DBG_PRT("[%d][%d / %d]\n", fie->index, fie->interval.numerator, fie->interval.denominator);

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static int eys3d_set_ctrl(struct v4l2_ctrl *ctrl)
{
    int ret = EYS3D_API_RET_OK;

    return ret;
}

static const struct dev_pm_ops eys3d_pm_ops = {
    SET_RUNTIME_PM_OPS(eys3d_runtime_suspend,
                       eys3d_runtime_resume,
                       NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops eys3d_internal_ops = {
    .open                   = eys3d_open,
};
#endif

static const struct v4l2_subdev_video_ops eys3d_video_ops = {
    .s_stream               = eys3d_s_stream,
    .g_frame_interval       = eys3d_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops eys3d_pad_ops = {
    .enum_mbus_code         = eys3d_enum_mbus_code,
    .enum_frame_size        = eys3d_enum_frame_sizes,
    .get_fmt                = eys3d_get_fmt,
    .set_fmt                = eys3d_set_fmt,
    .enum_frame_interval    = eys3d_enum_frameintervals
};

static const struct v4l2_subdev_ops eys3d_subdev_ops = {
    .video                  = &eys3d_video_ops,
    .pad                    = &eys3d_pad_ops,
};

static const struct v4l2_ctrl_ops eys3d_ctrl_ops = {
    .s_ctrl                 = eys3d_set_ctrl,
};

static int eys3d_probe(struct i2c_client* client,
            const struct i2c_device_id *id)
{
    struct device *dev = NULL;
    struct eys3d *priv = NULL;
    const struct eys3d_video_mode *mode = NULL;
    struct v4l2_subdev *sd = NULL;	
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!client || !id) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    dev = &client->dev;
    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    priv->client = client;
    sd = &priv->subdev;
    v4l2_i2c_subdev_init(sd, client, &eys3d_subdev_ops);

    ret = __eys3d_initialize_controls(priv);
    if (ret) {
        ret_line = __LINE__;
        goto err_destroy_mutex;
    }

    ret = __eys3d_power_on(priv);
    if (ret) {
        ret_line = __LINE__;
        goto err_free_handler;
    }

#if defined(CONFIG_VIDEO_V4L2_SUBDEV_API)
    sd->internal_ops = &eys3d_internal_ops;
    sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
    priv->pad.flags = MEDIA_PAD_FL_SOURCE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,5,0)
    sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
    ret = media_entity_pads_init(&sd->entity, 1, &priv->pad);
#else
    sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
    ret = media_entity_init(&sd->entity, 1, &priv->pad, 0);
#endif
    if (ret < EYS3D_API_RET_OK) {
        EYS3D_ERR_DBG_PRT("Subdev registration failed\n");
        ret_line = __LINE__;
        goto err_power_off;
    }
#endif

    mutex_init(&priv->eys3d_mutex);
    EYS3D_API_DBG_PRT("[%s] 0x%p Addr 0x%02X\n", id->name, client, client->addr);

    ret = eys3d_api_init(priv);
    if (ret == EYS3D_API_RET_OK) {
        mode = eys3d_api_get_current_video_mode(priv);
        if (mode) {
            EYS3D_API_DBG_PRT("[%u][%d, %d, %d, %d, %d]\n", \
                mode->mode_index, \
                (int)mode->width, \
                (int)mode->height, \
                (int)mode->fps, \
                (int)mode->is_interleave_mode, \
                (int)mode->is_rectify);
        }
    } else {
        EYS3D_ERR_DBG_PRT("Failed to call eys3d_api_init[%d]\n", ret);
    }

    ret = v4l2_async_register_subdev(sd);
    if (ret) {
        EYS3D_ERR_DBG_PRT("v4l2 async register subdev failed\n");
        ret_line = __LINE__;
        goto err_clean_entity;
    }

    pm_runtime_set_active(dev);
    pm_runtime_enable(dev);
    pm_runtime_idle(dev);

    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
    media_entity_cleanup(&sd->entity);
#endif

err_power_off:
    __eys3d_power_off(priv);

err_free_handler:
    v4l2_ctrl_handler_free(&priv->ctrl_handler);

err_destroy_mutex:
    mutex_destroy(&priv->eys3d_mutex);

exit:
    if (priv)
        devm_kfree(dev, priv);

    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static int eys3d_remove(struct i2c_client* client)
{
    struct v4l2_subdev *sd = NULL;
    struct eys3d *priv = NULL;
    struct device *dev = NULL;
    int ret = EYS3D_API_RET_OK;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!client) {
        ret = -EINVAL;
        goto exit;
    }

    dev = &client->dev;

    sd = i2c_get_clientdata(client);
    if (!sd) {
        ret = -EINVAL;
        goto exit;
    }

    priv = sd_to_eys3d(sd);
    if (!priv) {
        ret = -EINVAL;
        goto exit;
    }

    v4l2_async_unregister_subdev(sd);

#if defined(CONFIG_MEDIA_CONTROLLER)
    media_entity_cleanup(&sd->entity);
#endif

    v4l2_ctrl_handler_free(&priv->ctrl_handler);
    mutex_destroy(&priv->eys3d_mutex);

    pm_runtime_disable(&client->dev);
    if (!pm_runtime_status_suspended(&client->dev))
        __eys3d_power_off(priv);
    pm_runtime_set_suspended(&client->dev);

    eys3d_api_rls(priv);

exit:
    if (priv)
        devm_kfree(dev, priv);

    EYS3D_API_DBG_PRT("Leave[%d]\n", ret);

    return ret;
}

static struct i2c_driver eys3d_i2c_driver = {
    .driver = {
        .name           = "eys3d",
        .pm             = &eys3d_pm_ops,
        .of_match_table = of_match_ptr(eys3d_of_match),
    },
    .probe              = &eys3d_probe,
    .remove             = &eys3d_remove,
    .id_table           = eys3d_match_id,
};

#if 1
module_i2c_driver(eys3d_i2c_driver);
#else
static int __init sensor_mod_init(void)
{
    return i2c_add_driver(&eys3d_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
    i2c_del_driver(&eys3d_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);
#endif

MODULE_DESCRIPTION("eYs3D Camera Sensor Driver");
MODULE_LICENSE("GPL v2");
MODULE_INFO(intree, "Y");
MODULE_VERSION(EYS3D_API_VERSION);
