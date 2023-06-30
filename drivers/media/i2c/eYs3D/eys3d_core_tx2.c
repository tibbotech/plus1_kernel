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

static const u32 ctrl_cid_list[] = {
    TEGRA_CAMERA_CID_GAIN,
    TEGRA_CAMERA_CID_EXPOSURE,
    TEGRA_CAMERA_CID_EXPOSURE_SHORT,
    TEGRA_CAMERA_CID_FRAME_RATE,
    TEGRA_CAMERA_CID_GROUP_HOLD,
    TEGRA_CAMERA_CID_HDR_EN,
};

static struct regmap_config eys3d_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
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
    struct i2c_client* client = v4l2_get_subdevdata(sd);
    int ret = EYS3D_API_RET_OK;

    (void)client;

    return ret;
}
#endif

static int eys3d_power_on(struct camera_common_data *s_data)
{
    struct camera_common_power_rail *pw = s_data->power;
    int ret = EYS3D_API_RET_OK;

    pw->state = SWITCH_ON;

    return ret;
}

static int eys3d_power_off(struct camera_common_data *s_data)
{
    struct camera_common_power_rail *pw = s_data->power;
    int ret = EYS3D_API_RET_OK;

    pw->state = SWITCH_OFF;

    return ret;
}

static int write_reg(struct camera_common_data *s_data, u16 addr, u8 val)
{
    int ret = EYS3D_API_RET_OK;

    return ret;
}

static struct camera_common_pdata *eys3d_parse_dt(struct tegracam_device *tc_dev)
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

static int eys3d_power_get(struct tegracam_device *tc_dev)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct camera_common_power_rail *pw = s_data->power;
    int ret = EYS3D_API_RET_OK;

    pw->state = SWITCH_OFF;

    return ret;
}

static int eys3d_power_put(struct tegracam_device *tc_dev)
{
    int ret = EYS3D_API_RET_OK;

    return ret;
}

static int eys3d_set_mode(struct tegracam_device *tc_dev)
{
    struct eys3d *priv = (struct eys3d *)tegracam_get_privdata(tc_dev);
    struct camera_common_data *s_data = tc_dev->s_data;
    int ret = EYS3D_API_RET_OK;

    (void)priv;
    (void)s_data;

    return ret;
}

static int eys3d_start_streaming(struct tegracam_device *tc_dev)
{
    struct eys3d *priv = (struct eys3d *)tegracam_get_privdata(tc_dev);
    int ret = EYS3D_API_RET_OK;

    mutex_lock(&priv->eys3d_mutex);
    priv->streaming = true;
    __eys3d_start_stream(priv);
    mutex_unlock(&priv->eys3d_mutex);

    return ret;
}

static int eys3d_stop_streaming(struct tegracam_device *tc_dev)
{
    struct eys3d *priv = (struct eys3d *)tegracam_get_privdata(tc_dev);
    int ret = EYS3D_API_RET_OK;

    mutex_lock(&priv->eys3d_mutex);
    priv->streaming = true;
    __eys3d_stop_stream(priv);
    mutex_unlock(&priv->eys3d_mutex);

    return ret;
}

static int eys3d_set_gain(struct tegracam_device *tc_dev, s64 val)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct eys3d *priv = (struct eys3d *)tc_dev->priv;
    struct device *dev = tc_dev->dev;
    const struct sensor_mode_properties *mode =
        &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
    int ret = EYS3D_API_RET_OK;

    (void)priv;
    (void)s_data;
    (void)dev;
    (void)mode;

    return ret;
}

static int eys3d_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct device *dev = tc_dev->dev;
    struct eys3d *priv = (struct eys3d *)tc_dev->priv;
    const struct sensor_mode_properties *mode =
        &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
    int ret = EYS3D_API_RET_OK;

    (void)priv;
    (void)s_data;
    (void)dev;
    (void)mode;

    return ret;
}

static int eys3d_set_exposure_short(struct tegracam_device *tc_dev, s64 val)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct device *dev = tc_dev->dev;
    struct eys3d *priv = (struct eys3d *)tc_dev->priv;
    const struct sensor_mode_properties *mode =
        &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
    int ret = EYS3D_API_RET_OK;

    (void)priv;
    (void)s_data;
    (void)dev;
    (void)mode;

    return ret;
}

static int eys3d_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
    struct camera_common_data *s_data = tc_dev->s_data;
    struct device *dev = tc_dev->dev;
    struct eys3d *priv = (struct eys3d *)tc_dev->priv;
    const struct sensor_mode_properties *mode =
        &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
    int ret = EYS3D_API_RET_OK;

    (void)priv;
    (void)s_data;
    (void)dev;
    (void)mode;

    return ret;
}

static int eys3d_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
    struct eys3d *priv = (struct eys3d *)tc_dev->priv;
    struct device *dev = tc_dev->dev;
    int ret = EYS3D_API_RET_OK;

    (void)priv;
    (void)dev;

    return ret;
}

static const struct dev_pm_ops eys3d_pm_ops = {
    SET_RUNTIME_PM_OPS(eys3d_runtime_suspend,
                       eys3d_runtime_resume,
                       NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops eys3d_internal_ops = {
    .open               = eys3d_open,
};
#endif

static struct camera_common_sensor_ops eys3d_common_ops = {
    .numfrmfmts         = ARRAY_SIZE(eys3d_frmfmt),
    .frmfmt_table       = eys3d_frmfmt,
    .power_on           = eys3d_power_on,
    .power_off          = eys3d_power_off,
    .write_reg          = write_reg,
    .parse_dt           = eys3d_parse_dt,
    .power_get          = eys3d_power_get,
    .power_put          = eys3d_power_put,
    .set_mode           = eys3d_set_mode,
    .start_streaming    = eys3d_start_streaming,
    .stop_streaming     = eys3d_stop_streaming,
};

static struct tegracam_ctrl_ops eys3d_ctrl_ops = {
    .numctrls           = ARRAY_SIZE(ctrl_cid_list),
    .ctrl_cid_list      = ctrl_cid_list,
    .set_gain           = eys3d_set_gain,
    .set_exposure       = eys3d_set_exposure,
    .set_exposure_short = eys3d_set_exposure_short,
    .set_frame_rate     = eys3d_set_frame_rate,
    .set_group_hold     = eys3d_set_group_hold,
};

static int eys3d_probe(struct i2c_client* client,
            const struct i2c_device_id *id)
{
    struct device *dev = NULL;
    struct eys3d *priv = NULL;
    const struct eys3d_video_mode *mode = NULL;
    struct tegracam_device *tc_dev = NULL;
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
        ret = -ENOMEM;
        ret_line = __LINE__;
        goto exit;
    }

    tc_dev = devm_kzalloc(dev, sizeof(struct tegracam_device), GFP_KERNEL);
    if (!tc_dev) {
        ret = -ENOMEM;
        ret_line = __LINE__;
        goto exit;
    }

    priv->client = tc_dev->client = client;
    tc_dev->dev = dev;
    strncpy(tc_dev->name, "eys3d", sizeof(tc_dev->name));
    tc_dev->dev_regmap_config = &eys3d_regmap_config;
    tc_dev->sensor_ops = &eys3d_common_ops;
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
    tc_dev->v4l2sd_internal_ops = &eys3d_internal_ops;
#endif
    tc_dev->tcctrl_ops = &eys3d_ctrl_ops;

    ret = tegracam_device_register(tc_dev);
    if (ret) {
        EYS3D_ERR_DBG_PRT("Tegra camera driver registration failed\n");
        ret_line = __LINE__;
        goto exit;
    }

    //FIXME: This assignment is improtant! With this assignment, we could use sd_to_eys3d to get the proper eys3d object!
    tc_dev->s_data->priv = priv;

    priv->tc_dev = tc_dev;
    priv->s_data = tc_dev->s_data;
    priv->subdev = &tc_dev->s_data->subdev;
    tegracam_set_privdata(tc_dev, (void *)priv);

    ret = tegracam_v4l2subdev_register(tc_dev, true);
    if (ret) {
        EYS3D_ERR_DBG_PRT("Subdev registration failed\n");
        ret_line = __LINE__;
        goto exit;
    }

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

    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;

exit:
    mutex_destroy(&priv->eys3d_mutex);

    if (tc_dev)
        devm_kfree(dev, tc_dev);

    if (priv)
        devm_kfree(dev, priv);

    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static int eys3d_remove(struct i2c_client* client)
{
    struct camera_common_data *s_data = NULL;
    struct eys3d *priv = NULL;
    struct device *dev = NULL;
    int ret = EYS3D_API_RET_OK;

    EYS3D_API_DBG_PRT("Enter\n");

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

    priv = (struct eys3d *)s_data->priv;
    if (!priv) {
        ret = -EINVAL;
        goto exit;
    }

    tegracam_v4l2subdev_unregister(priv->tc_dev);
    tegracam_device_unregister(priv->tc_dev);

    mutex_destroy(&priv->eys3d_mutex);

    eys3d_api_rls(priv);

exit:
    if (priv->tc_dev)
        devm_kfree(dev, priv->tc_dev);

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
