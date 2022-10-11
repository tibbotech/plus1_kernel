#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>

#include "esp876_api.h"
#include "esp876_mode_tbls.h"

#define ESP876_I2C_DEVICE_ID_NAME   "eSP876"
#define ESP876_WINDOW_WIDTH_MIN     32U
#define ESP876_WINDOW_HEIGHT_MIN    32U

#define ESP876_LINK_FREQ_420MHZ     420000000
#define ESP876_PIXEL_RATE           (45 * 1000 * 1000 * 4)

static u32 supported_camera_pixel_codes[] = {
    MEDIA_BUS_FMT_UYVY8_2X8,
    MEDIA_BUS_FMT_UYVY8_1X16,
};

static struct v4l2_rect *
esp876_get_pad_crop(struct esp876 *esp876, struct v4l2_subdev_pad_config *cfg,
              unsigned int pad, u32 which)
{
    switch (which) {
        case V4L2_SUBDEV_FORMAT_TRY:
            return v4l2_subdev_get_try_crop(&esp876->subdev, cfg, pad);
        case V4L2_SUBDEV_FORMAT_ACTIVE:
            return &esp876->crop;
        default:
            return NULL;
    }
}

static struct v4l2_mbus_framefmt *
esp876_get_pad_format(struct esp876 *esp876,
            struct v4l2_subdev_pad_config *cfg,
            unsigned int pad, u32 which)
{
    switch (which) {
        case V4L2_SUBDEV_FORMAT_TRY:
            return v4l2_subdev_get_try_format(&esp876->subdev, cfg, pad);
        case V4L2_SUBDEV_FORMAT_ACTIVE:
            return &esp876->format;
        default:
            return NULL;
    }
}

static int esp876_set_fmt(struct v4l2_subdev *sd,
              struct v4l2_subdev_pad_config *cfg,
              struct v4l2_subdev_format *format)
{
    struct esp876 *esp876 = sd_to_esp876(sd);
    struct v4l2_mbus_framefmt *__format;
    struct v4l2_rect *__crop;
    const struct esp876_video_mode *mode = NULL;
    unsigned int width;
    unsigned int height;
    unsigned int hratio;
    unsigned int vratio;
    int ret = 0;

    ESP876_API_DEBUG("Enter, Set Format = (%u x %u)\n", format->format.width, format->format.height);
#if 0
    __crop = esp876_get_pad_crop(esp876, cfg, format->pad, format->which);
    ESP876_API_DEBUG("crop = (%u, %u, %u, %u)\n",
        __crop->left, __crop->top, __crop->width, __crop->height);

    /* Clamp the width and height to avoid dividing by zero. */
    width = clamp_t(unsigned int, ALIGN(format->format.width, 2),
            max(__crop->width / 3, ESP876_WINDOW_WIDTH_MIN),
            __crop->width);
    height = clamp_t(unsigned int, ALIGN(format->format.height, 2),
            max(__crop->height / 3, ESP876_WINDOW_HEIGHT_MIN),
             __crop->height);
    hratio = DIV_ROUND_CLOSEST(__crop->width, width);
    vratio = DIV_ROUND_CLOSEST(__crop->height, height);

    __format = esp876_get_pad_format(esp876, cfg, format->pad, format->which);
    __format->width = __crop->width / hratio;
    __format->height = __crop->height / vratio;

    if (__format->width == 0 || __format->width == 0) {
        __format->width = format->format.width;
        __format->height = format->format.height;
    }

    format->format = *__format;
#endif
    mode = esp876_api_get_current_video_mode(esp876);
    ESP876_ALERT_DEBUG("Current Format: [%d](%d x %d, %d)\n", mode->mode_index, mode->width, mode->height, mode->fps);

    if (format->format.width != mode->width && format->format.height != mode->height) {
        if (esp876_api_set_current_video_mode(esp876, format->format.width, format->format.height) == 0) {
            if (mode) {
                ESP876_ALERT_DEBUG("[%d](%d x %d, %d)\n", mode->mode_index, mode->width, mode->height, mode->fps);
            } else {
                mode = esp876_api_get_current_video_mode(esp876);
                ESP876_ERR_DEBUG("Not Found mode! Current Format: [%d](%d x %d, %d)\n", mode->mode_index, mode->width, mode->height, mode->fps);
            }
        } else {
            mode = esp876_api_get_current_video_mode(esp876);
            ESP876_ERR_DEBUG("Not Found mode! Current Format: [%d](%d x %d, %d)\n", mode->mode_index, mode->width, mode->height, mode->fps);
        }
    }

    ESP876_API_DEBUG("Leave(%d)\n", ret);

    return ret;
}

static int esp876_get_fmt(struct v4l2_subdev *sd,
              struct v4l2_subdev_pad_config *cfg,
              struct v4l2_subdev_format *fmt)
{
    struct esp876 *esp876 = sd_to_esp876(sd);
    const struct esp876_video_mode *mode = NULL;
    int ret = 0;

    ESP876_API_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        goto exit;
    }

    mode = esp876_api_get_current_video_mode(esp876);
    if (!mode) {
        ret = -EINVAL;
        goto exit;
    }

    mutex_lock(&esp876->mutex);
    if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
        fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
        ESP876_API_DEBUG("CONFIG_VIDEO_V4L2_SUBDEV_API\n");
#else
        mutex_unlock(&esp876->mutex);
        ESP876_API_DEBUG("Leave\n");
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

    ESP876_API_DEBUG("[%d](%d, %d, %d)\n",mode->mode_index, mode->width, mode->height, mode->fps);
    mutex_unlock(&esp876->mutex);

exit:
    ESP876_API_DEBUG("Leave(%d)\n", ret);
    return 0;
}

static int esp876_enum_mbus_code(struct v4l2_subdev *sd,
                 struct v4l2_subdev_pad_config *cfg,
                 struct v4l2_subdev_mbus_code_enum *code)
{
    struct esp876 *esp876 = sd_to_esp876(sd);
    int ret = 0;
    int ret_line = 0;

    (void)esp876;

    ESP876_API_DEBUG("Enter\n");

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

    ESP876_API_DEBUG("[%d][0x%08x]\n", code->index, code->code);

exit:
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}

static int esp876_enum_frame_sizes(struct v4l2_subdev *sd,
                   struct v4l2_subdev_pad_config *cfg,
                   struct v4l2_subdev_frame_size_enum *fse)
{
    struct esp876 *esp876 = sd_to_esp876(sd);
    const struct esp876_video_mode *mode = NULL;
    int ret = 0;
    int ret_line = 0;

    (void)esp876;

    ESP876_API_DEBUG("Enter\n");

    if (!fse) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = esp876_api_get_video_mode_by_index(esp876, fse->index);
    if (!mode) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    fse->min_width  = mode->width;
    fse->max_width  = mode->width;
    fse->min_height = mode->height;
    fse->max_height = mode->height;

    ESP876_API_DEBUG("[%d][0x%08x, 0x%08x]\n", fse->index, fse->code, mode->pixelcode);
    ESP876_API_DEBUG("[%d][%d x %d][%d x %d]\n", fse->index, fse->min_width, fse->min_height, fse->max_width, fse->max_height);

exit:
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}

static int esp876_enum_frameintervals(struct v4l2_subdev *sd,
        struct v4l2_subdev_pad_config *cfg,
        struct v4l2_subdev_frame_interval_enum *fie)
{
    struct esp876 *esp876 = sd_to_esp876(sd);
    const struct esp876_video_mode *mode = NULL;
    int ret = 0;
    int ret_line = 0;

    ESP876_API_DEBUG("Enter\n");

    (void)esp876;

    if (!fie) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = esp876_api_get_current_video_mode(esp876);
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

    ESP876_API_DEBUG("[%d][%d / %d]\n", fie->index, fie->interval.numerator, fie->interval.denominator);

exit:
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}

static int esp876_g_frame_interval(struct v4l2_subdev *sd,
                   struct v4l2_subdev_frame_interval *fi)
{
    struct esp876 *esp876 = sd_to_esp876(sd);
    const struct esp876_video_mode *mode = NULL;
    int ret = 0;
    int ret_line = 0;

    ESP876_API_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = esp876_api_get_current_video_mode(esp876);
    if (!mode) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mutex_lock(&esp876->mutex);
    fi->interval.numerator = 1;
    fi->interval.denominator = mode->fps;
    mutex_unlock(&esp876->mutex);

exit:
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return 0;
}

static int __esp876_start_stream(struct esp876 *esp876)
{
    const struct esp876_video_mode *mode = NULL;
    struct i2c_client *client = NULL;
    int ret = 0;
    int ret_line = 0;
    u8 val = 0;
    u16 reg = 0;

    (void)val;
    (void)reg;

    ESP876_ALERT_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    client = esp876->client;
    if (!client) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = esp876_api_get_current_video_mode(esp876);
    if (!mode) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    ESP876_API_DEBUG("[0x%08x](%d, %d, %d, %d, %d)\n", mode->mode_index, \
        (int)mode->width, \
        (int)mode->height, \
        (int)mode->fps, (int)mode->is_interleave_mode, (int)mode->is_rectify);

    ret = esp876_api_start_stream(esp876);

exit:
    ESP876_ALERT_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}

static int __esp876_stop_stream(struct esp876 *esp876)
{
    int ret = 0;
    int ret_line = 0;

    ESP876_ALERT_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    ret = esp876_api_stop_stream(esp876);

exit:
    ESP876_ALERT_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}

static int esp876_s_stream(struct v4l2_subdev *sd, int on)
{
    struct esp876 *esp876 = sd_to_esp876(sd);
    struct i2c_client *client = NULL;
    int ret = 0;
    int ret_line = 0;

    (void)client;

    ESP876_API_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    client = esp876->client;

    mutex_lock(&esp876->mutex);
    if (on == esp876->streaming) {
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

        ret = __esp876_start_stream(esp876);
        if (ret) {
            ESP876_ERR_DEBUG("Failed to start stream!(%d)\n", ret);
            ret_line = __LINE__;
            goto unlock_and_return;
        }
    } else {
        __esp876_stop_stream(esp876);
        pm_runtime_put(&client->dev);
    }

    esp876->streaming = on;

    ESP876_API_DEBUG("Stream = [%d]\n", on);

unlock_and_return:
    mutex_unlock(&esp876->mutex);

exit:
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}

static int __esp876_power_on(struct esp876 *esp876)
{
    struct device *dev = NULL;
    int ret = 0;
    int ret_line = 0;

    ESP876_API_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    dev = &esp876->client->dev;

    (void)dev;

exit:
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}

static void __esp876_power_off(struct esp876 *esp876)
{
    struct device *dev = NULL;
    int ret = 0;
    int ret_line = 0;

    ESP876_API_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    dev = &esp876->client->dev;

    (void)dev;

exit:
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
}

static int esp876_runtime_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *esp876 = sd_to_esp876(sd);

    return __esp876_power_on(esp876);
}

static int esp876_runtime_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *esp876 = sd_to_esp876(sd);

    __esp876_power_off(esp876);

    return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int esp876_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
    struct esp876 *esp876 = sd_to_esp876(sd);
    struct v4l2_mbus_framefmt *try_fmt =
                v4l2_subdev_get_try_format(sd, fh->pad, 0);
    const struct esp876_video_mode *def_mode = NULL;
    int ret = 0;
    int ret_line = 0;

    ESP876_API_DEBUG("Enter\n");

    if (!esp876) {
        ESP876_ERR_DEBUG("ESP876 is NULL\n");
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    def_mode = esp876_api_get_current_video_mode(esp876);
    if (!def_mode) {
        ESP876_ERR_DEBUG("esp876_api_get_current_video_mode fail\n");
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mutex_lock(&esp876->mutex);
    /* Initialize try_fmt */
    ESP876_API_DEBUG("Width = %d, Height = %d\n", def_mode->width, def_mode->height);
    try_fmt->width = def_mode->width;
    try_fmt->height = def_mode->height;
    try_fmt->code = def_mode->pixelcode;
    if (!def_mode->is_interleave_mode)
        try_fmt->field = V4L2_FIELD_ANY;
    else
        try_fmt->field = V4L2_FIELD_INTERLACED;
    mutex_unlock(&esp876->mutex);

exit:
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}
#endif

static int esp876_set_ctrl(struct v4l2_ctrl *ctrl)
{
    int ret = 0;
    return ret;
}

static const struct dev_pm_ops esp876_pm_ops = {
    SET_RUNTIME_PM_OPS(esp876_runtime_suspend,
               esp876_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops esp876_internal_ops = {
    .open                   = esp876_open,
};
#endif

static const struct v4l2_subdev_video_ops esp876_video_ops = {
    .s_stream               = esp876_s_stream,
    .g_frame_interval       = esp876_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops esp876_pad_ops = {
    .enum_mbus_code         = esp876_enum_mbus_code,
    .enum_frame_size        = esp876_enum_frame_sizes,
    .get_fmt                = esp876_get_fmt,
    .set_fmt                = esp876_set_fmt,
    .enum_frame_interval    = esp876_enum_frameintervals
};

static const struct v4l2_subdev_ops esp876_subdev_ops = {
    .video                  = &esp876_video_ops,
    .pad                    = &esp876_pad_ops,
};

static const struct v4l2_ctrl_ops esp876_ctrl_ops = {
    .s_ctrl                 = esp876_set_ctrl,
};

static const s64 link_freq_menu_items[] = {
    ESP876_LINK_FREQ_420MHZ
};

static int esp876_initialize_controls(struct esp876 *esp876)
{
    struct v4l2_ctrl_handler *handler;
    struct v4l2_ctrl *ctrl;
    int ret;

    handler = &esp876->ctrl_handler;
    ret = v4l2_ctrl_handler_init(handler, 8);
    if (ret)
        return ret;
    handler->lock = &esp876->mutex;

    ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
                      0, 0, link_freq_menu_items);
    if (ctrl)
        ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

    v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
              0, ESP876_PIXEL_RATE, 1, ESP876_PIXEL_RATE);

    if (handler->error) {
        ret = handler->error;
        dev_err(&esp876->client->dev,
            "Failed to init controls(%d)\n", ret);
        goto err_free_handler;
    }

    esp876->subdev.ctrl_handler = handler;

    return 0;

err_free_handler:
    v4l2_ctrl_handler_free(handler);

    return ret;
}

static int esp876_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    struct device *dev = NULL;
    struct esp876 *esp876 = NULL;
    struct v4l2_subdev *sd = NULL;
    const struct esp876_video_mode *mode = NULL;
    int ret = 0;
    int ret_line = 0;

    ESP876_API_DEBUG("Enter\n");

    if (!client || !id) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    ESP876_API_DEBUG("Device ID name: [%s], Client: 0x%p\n", id->name, client);

    dev = &client->dev;
    esp876 = devm_kzalloc(dev, sizeof(*esp876), GFP_KERNEL);
    if (!esp876) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    esp876->client = client;

    mutex_init(&esp876->mutex);

    sd = &esp876->subdev;
    v4l2_i2c_subdev_init(sd, client, &esp876_subdev_ops);

    ret = esp876_initialize_controls(esp876);
    if (ret) {
        ret_line = __LINE__;
        goto err_destroy_mutex;
    }

    ret = __esp876_power_on(esp876);
    if (ret) {
        ret_line = __LINE__;
        goto err_free_handler;
    }

#if defined(CONFIG_VIDEO_V4L2_SUBDEV_API)
    sd->internal_ops = &esp876_internal_ops;
    sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
    esp876->pad.flags = MEDIA_PAD_FL_SOURCE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,5,0)
    sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
    ret = media_entity_pads_init(&sd->entity, 1, &esp876->pad);
#else
    sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
    ret = media_entity_init(&sd->entity, 1, &esp876->pad, 0);
#endif
    if (ret < 0) {
        ret_line = __LINE__;
        goto err_power_off;
    }
#endif

    ret = esp876_api_init(esp876);
    if (ret == 0) {
        mode  = esp876_api_get_current_video_mode(esp876);
        if (mode) {
            ESP876_API_DEBUG("[%u](%d, %d, %d, %d, %d)\n", \
                    mode->mode_index, \
                    (int)mode->width, \
                    (int)mode->height, \
                    (int)mode->fps, \
                    (int)mode->is_interleave_mode, \
                    (int)mode->is_rectify);
        }
    } else {
        ESP876_API_DEBUG("Failed to call esp876_api_init()(%d)\n", ret);
    }

    ret = v4l2_async_register_subdev(sd);
    if (ret) {
        ESP876_ERR_DEBUG("v4l2 async register subdev failed\n");
        ret_line = __LINE__;
        goto err_clean_entity;
    }

    pm_runtime_set_active(dev);
    pm_runtime_enable(dev);
    pm_runtime_idle(dev);

    ESP876_API_DEBUG("Leave(%d)\n", ret);

    return ret;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
    media_entity_cleanup(&sd->entity);
#endif

err_power_off:
    __esp876_power_off(esp876);

err_free_handler:
    v4l2_ctrl_handler_free(&esp876->ctrl_handler);

err_destroy_mutex:
    mutex_destroy(&esp876->mutex);

exit:
    if (esp876)
        devm_kfree(dev, esp876);
    ESP876_API_DEBUG("Leave(%d, %d)\n", ret, ret_line);
    return ret;
}

static int esp876_remove(struct i2c_client *client)
{
    struct v4l2_subdev *sd = NULL;
    struct esp876 *esp876 = NULL;
    struct device *dev = NULL;
    int ret = 0;

    ESP876_API_DEBUG("Enter\n");

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

    esp876 = sd_to_esp876(sd);
    if (!esp876) {
        ret = -EINVAL;
        goto exit;
    }

    v4l2_async_unregister_subdev(sd);

#if defined(CONFIG_MEDIA_CONTROLLER)
    media_entity_cleanup(&sd->entity);
#endif

    v4l2_ctrl_handler_free(&esp876->ctrl_handler);
    mutex_destroy(&esp876->mutex);

    pm_runtime_disable(&client->dev);
    if (!pm_runtime_status_suspended(&client->dev))
        __esp876_power_off(esp876);
    pm_runtime_set_suspended(&client->dev);

    esp876_api_rls(esp876);

exit:
    if (esp876)
        devm_kfree(dev, esp876);
    ESP876_API_DEBUG("Leave(%d)\n", ret);
    return ret;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id esp876_of_match[] = {
    {.compatible = "eys3d,eSP876"},
    {},
};

MODULE_DEVICE_TABLE(of, esp876_of_match);
#endif

static const struct i2c_device_id esp876_match_id[] = {
    {ESP876_I2C_DEVICE_ID_NAME, 0},
    {},
};

static struct i2c_driver esp876_i2c_driver = {
    .driver = {
        .name           = "esp876",
        .pm             = &esp876_pm_ops,
        .of_match_table = of_match_ptr(esp876_of_match),
    },
    .probe              = &esp876_probe,
    .remove             = &esp876_remove,
    .id_table           = esp876_match_id,
};

#if 1
module_i2c_driver(esp876_i2c_driver);
#else
static int __init sensor_mod_init(void)
{
    return i2c_add_driver(&esp876_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
    i2c_del_driver(&esp876_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);
#endif

MODULE_DESCRIPTION("eYs3D eSP876 Camera Sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_INFO(intree, "Y");
MODULE_VERSION(ESP876_API_VERSION);
