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

#define ESP876_I2C_DEVICE_ID_NAME "eSP876"

static const u32 ctrl_cid_list[] = {
    TEGRA_CAMERA_CID_GAIN,
    TEGRA_CAMERA_CID_EXPOSURE,
    TEGRA_CAMERA_CID_EXPOSURE_SHORT,
    TEGRA_CAMERA_CID_FRAME_RATE,
    TEGRA_CAMERA_CID_GROUP_HOLD,
    TEGRA_CAMERA_CID_HDR_EN,
};

static struct regmap_config esp876_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id esp876_of_match[] = {
    {.compatible = "eys3d,eSP876"},
    {},
};
MODULE_DEVICE_TABLE(of, esp876_of_match);
#endif

static int __esp876_start_stream(struct esp876 *esp876)
{
    int ret = 0;
    const struct esp876_video_mode *mode = NULL;
    struct i2c_client *client = NULL;
    u8 val = 0;
    u16 reg = 0;

    (void)val;
    (void)reg;

    ESP876_API_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        goto exit;
    }

    client = esp876->client;
    if (!client) {
        ret = -EINVAL;
        goto exit;
    }

    mode = esp876_api_get_current_video_mode(esp876);
    if (!mode) {
        ret = -EINVAL;
        goto exit;
    }

    ESP876_API_DEBUG("[0x%08x](%d, %d, %d, %d, %d)\n", mode->mode_index, \
        (int)mode->width, \
        (int)mode->height, \
        (int)mode->fps, (int)mode->is_interleave_mode, (int)mode->is_rectify);

    ret = esp876_api_start_stream(esp876);

exit:
    ESP876_API_DEBUG("Leave(%d)\n", ret);
    return ret;
}

static int __esp876_stop_stream(struct esp876 *esp876)
{
    int ret = 0;

    ESP876_ALERT_DEBUG("Enter\n");

    if (!esp876) {
        ret = -EINVAL;
        goto exit;
    }

    ret = esp876_api_stop_stream(esp876);

exit:
    ESP876_API_DEBUG("Leave(%d)\n", ret);
    return ret;
}

static int esp876_write_reg(struct camera_common_data *s_data, u16 addr, u8 val)
{
    return 0;
}

static int esp876_power_on(struct camera_common_data *s_data)
{
    struct camera_common_power_rail *pw = s_data->power;
    pw->state = SWITCH_ON;
    return 0;
}

static int esp876_power_off(struct camera_common_data *s_data)
{
    struct camera_common_power_rail *pw = s_data->power;
    pw->state = SWITCH_OFF;
    return 0;
}

static int esp876_power_put(struct tegracam_device *tc_dev)
{
    return 0;
}

static int esp876_power_get(struct tegracam_device *tc_dev)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct camera_common_power_rail *pw = s_data->power;

    pw->state = SWITCH_OFF;
    return 0;
}

static struct camera_common_pdata *esp876_parse_dt(struct tegracam_device
                            *tc_dev)
{
    struct device *dev = tc_dev->dev;
    struct device_node *node = dev->of_node;
    struct camera_common_pdata *board_priv_pdata;

    if (!node)
        return NULL;

    board_priv_pdata = devm_kzalloc(dev,
               sizeof(*board_priv_pdata), GFP_KERNEL);
    if (!board_priv_pdata)
        return NULL;

    return board_priv_pdata;
}

static int esp876_set_mode(struct tegracam_device *tc_dev)
{
    struct esp876 *priv = (struct esp876 *)tegracam_get_privdata(tc_dev);
    struct camera_common_data *s_data = tc_dev->s_data;

    (void)priv;
    (void)s_data;

    return 0;
}

static int esp876_start_streaming(struct tegracam_device *tc_dev)
{
    struct esp876 *priv = (struct esp876 *)tegracam_get_privdata(tc_dev);

    mutex_lock(&priv->mutex);
    priv->streaming = true;
    __esp876_start_stream(priv);
    mutex_unlock(&priv->mutex);

    return 0;
}

static int esp876_stop_streaming(struct tegracam_device *tc_dev)
{
    struct esp876 *priv = (struct esp876 *)tegracam_get_privdata(tc_dev);

    mutex_lock(&priv->mutex);
    priv->streaming = true;
    __esp876_stop_stream(priv);
    mutex_unlock(&priv->mutex);

    return 0;
}

static int esp876_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
    struct esp876 *priv = tc_dev->priv;
    struct device *dev = tc_dev->dev;

    (void)priv;
    (void)dev;

#if 0
    if (priv->group_hold_en == true && gh_prev == SWITCH_OFF) {
        /**/
    } else if (priv->group_hold_en == false && gh_prev == SWITCH_ON) {
        /**/
    }
#endif

    return 0;
}

static int esp876_set_gain(struct tegracam_device *tc_dev, s64 val)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct esp876 *priv = (struct esp876 *)tc_dev->priv;
    struct device *dev = tc_dev->dev;
    const struct sensor_mode_properties *mode =
        &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];

    (void)priv;
    (void)s_data;
    (void)dev;
    (void)mode;

    return 0;
}

static int esp876_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct device *dev = tc_dev->dev;
    struct esp876 *priv = tc_dev->priv;
    const struct sensor_mode_properties *mode =
        &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];

    (void)priv;
    (void)s_data;
    (void)dev;
    (void)mode;

    return 0;
}

static int esp876_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct device *dev = tc_dev->dev;
    struct esp876 *priv = tc_dev->priv;
    const struct sensor_mode_properties *mode =
        &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];

    (void)priv;
    (void)s_data;
    (void)dev;
    (void)mode;

    return 0;
}

static int esp876_set_exposure_short(struct tegracam_device *tc_dev, s64 val)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct device *dev = tc_dev->dev;
    struct esp876 *priv = tc_dev->priv;
    const struct sensor_mode_properties *mode =
        &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];

    (void)priv;
    (void)s_data;
    (void)dev;
    (void)mode;

    return 0;
}

static int esp876_runtime_resume(struct device *dev)
{
    return 0;
}

static int esp876_runtime_suspend(struct device *dev)
{
    return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int esp876_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int ret = 0;

    (void)client;

    ESP876_API_DEBUG("Enter\n");
    ESP876_API_DEBUG("Leave(%d)\n", ret);
    return ret;
}
#endif

static const struct dev_pm_ops esp876_pm_ops = {
    SET_RUNTIME_PM_OPS(esp876_runtime_suspend,
               esp876_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops esp876_internal_ops = {
    .open = esp876_open,
};
#endif

static struct camera_common_sensor_ops esp876_common_ops = {
    .numfrmfmts         = ARRAY_SIZE(esp876_frmfmt),
    .frmfmt_table       = esp876_frmfmt,
    .power_on           = esp876_power_on,
    .power_off          = esp876_power_off,
    .write_reg          = esp876_write_reg,
    .parse_dt           = esp876_parse_dt,
    .power_get          = esp876_power_get,
    .power_put          = esp876_power_put,
    .set_mode           = esp876_set_mode,
    .start_streaming    = esp876_start_streaming,
    .stop_streaming     = esp876_stop_streaming,
};

static struct tegracam_ctrl_ops esp876_ctrl_ops = {
    .numctrls           = ARRAY_SIZE(ctrl_cid_list),
    .ctrl_cid_list      = ctrl_cid_list,
    .set_gain           = esp876_set_gain,
    .set_exposure       = esp876_set_exposure,
    .set_exposure_short = esp876_set_exposure_short,
    .set_frame_rate     = esp876_set_frame_rate,
    .set_group_hold     = esp876_set_group_hold,
};

static int esp876_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    struct device *dev = NULL;
    struct esp876 *priv = NULL;
    int ret = 0;
    const struct esp876_video_mode *mode = NULL;
    struct tegracam_device *tc_dev = NULL;

    ESP876_API_DEBUG("Enter\n");

    if (!client || !id){
        ret = -EINVAL;
        goto exit;
    }

    ESP876_API_DEBUG("Device ID name: [%s], Client: 0x%p\n", id->name, client);

    dev = &client->dev;
    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        ret = -ENOMEM;
        goto exit;
    }

    tc_dev = devm_kzalloc(dev, sizeof(struct tegracam_device), GFP_KERNEL);
    if (!tc_dev) {
        ret = -ENOMEM;
        goto exit;
    }

    priv->client = tc_dev->client = client;
    tc_dev->dev = dev;
    strncpy(tc_dev->name, "esp876", sizeof(tc_dev->name));
    tc_dev->dev_regmap_config = &esp876_regmap_config;
    tc_dev->sensor_ops = &esp876_common_ops;
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
    tc_dev->v4l2sd_internal_ops = &esp876_internal_ops;
#endif
    tc_dev->tcctrl_ops = &esp876_ctrl_ops;

    ret = tegracam_device_register(tc_dev);
    if (ret) {
        ESP876_ERR_DEBUG("tegra camera driver registration failed\n");
        goto exit;
    }

    //FIXME: This assignment is improtant! With this assignment, we could use sd_to_esp876 to get the proper esp876 object!
    tc_dev->s_data->priv = priv;

    priv->tc_dev = tc_dev;
    priv->s_data = tc_dev->s_data;
    priv->subdev = &tc_dev->s_data->subdev;
    tegracam_set_privdata(tc_dev, (void *)priv);
    mutex_init(&priv->mutex);

    ret = tegracam_v4l2subdev_register(tc_dev, true);
    if (ret) {
        ESP876_ERR_DEBUG("tegra camera subdev registration failed\n");
        goto exit;
    }

    ret = esp876_api_init(priv);
    if (ret == 0) {
        mode  = esp876_api_get_current_video_mode(priv);
        if (mode) {
            ESP876_API_DEBUG("[%u](%d, %d, %d, %d, %d)\n", \
                    mode->mode_index, \
                    (int)mode->width, \
                    (int)mode->height, \
                    (int)mode->fps, \
                    (int)mode->is_interleave_mode, \
                    (int)mode->is_rectify);
        }

        ESP876_API_DEBUG("driv = [0x%p], sd = [0x%p],  client = [0x%p], priv =[0x%p]\n", priv, priv->subdev,  priv->client, priv->data);
    } else {
        ESP876_ERR_DEBUG("Failed to call esp876_api_init()(%d)\n", ret);
    }

    ESP876_API_DEBUG("Leave(%d)\n", ret);
    return ret;

exit:
    if (priv) {
        mutex_destroy(&priv->mutex);
        devm_kfree(dev, priv);
    }

    if (tc_dev) {
        devm_kfree(dev, tc_dev);
    }

    ESP876_API_DEBUG("Leave(%d)\n", ret);
    return ret;
}

static int esp876_remove(struct i2c_client *client)
{
    struct camera_common_data *s_data = NULL;
    struct esp876 *priv = NULL;
    struct device *dev = NULL;
    int ret = 0;

    ESP876_API_DEBUG("Enter\n");

    if (!client) {
        ret = -EINVAL;
        goto exit;
    }

    dev = &client->dev;

    s_data = to_camera_common_data(&client->dev);
    if (!s_data) {
        ret = -EINVAL;
        goto exit;
    }

    priv = (struct esp876 *)s_data->priv;
    if (!priv) {
        ret = -EINVAL;
        goto exit;
    }

    tegracam_v4l2subdev_unregister(priv->tc_dev);
    tegracam_device_unregister(priv->tc_dev);

    mutex_destroy(&priv->mutex);

    esp876_api_rls(priv);

exit:
    if (priv) {
        if (priv->tc_dev) {
            devm_kfree(dev, priv->tc_dev);
        }
        devm_kfree(dev, priv);
    }

    ESP876_API_DEBUG("Leave(%d)\n", ret);
    return ret;
}

static const struct i2c_device_id esp876_match_id[] = {
    {ESP876_I2C_DEVICE_ID_NAME, 0},
    { },
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
