#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "eys3d_api.h"
#include "eys3d_esp876.h"

static const struct eys3d_uvc_ctrl g_uvc_ct_ctrls[] = {
    {.id = CAMERA_HW_CT_CONTROL_UNDEFINED, .val_len = 0},
    {.id = CAMERA_HW_CT_SCANNING_MODE_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_CT_AE_MODE_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_CT_AE_PRIORITY_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL, .val_len = 4},
    {.id = CAMERA_HW_CT_EXPOSURE_TIME_RELATIVE_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_CT_FOCUS_ABSOLUTE_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_CT_FOCUS_RELATIVE_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_CT_FOCUS_AUTO_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_CT_IRIS_ABSOLUTE_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_CT_IRIS_RELATIVE_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_CT_ZOOM_ABSOLUTE_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_CT_ZOOM_RELATIVE_CONTROL, .val_len = 3},
    {.id = CAMERA_HW_CT_PANTILT_ABSOLUTE_CONTROL, .val_len = 8},
    {.id = CAMERA_HW_CT_PANTILT_RELATIVE_CONTROL, .val_len = 4},
    {.id = CAMERA_HW_CT_ROLL_ABSOLUTE_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_CT_ROLL_RELATIVE_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_CT_PRIVACY_CONTROL, .val_len = 1},
};

static const struct eys3d_uvc_ctrl g_uvc_pu_ctrls[] = {
    {.id = CAMERA_HW_PU_CONTROL_UNDEFINED, .val_len = 0},
    {.id = CAMERA_HW_PU_BACKLIGHT_COMPENSATION_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_BRIGHTNESS_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_CONTRAST_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_GAIN_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_POWER_LINE_FREQUENCY_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_PU_HUE_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_SATURATION_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_SHARPNESS_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_GAMMA_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_WHITE_BALANCE_TEMPERATURE_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_PU_WHITE_BALANCE_COMPONENT_CONTROL, .val_len = 4},
    {.id = CAMERA_HW_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_PU_DIGITAL_MULTIPLIER_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL, .val_len = 2},
    {.id = CAMERA_HW_PU_HUE_AUTO_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_PU_ANALOG_VIDEO_STANDARD_CONTROL, .val_len = 1},
    {.id = CAMERA_HW_PU_ANALOG_LOCK_STATUS_CONTROL, .val_len = 1},
};

static int esp876_wait_reg_for_target(struct i2c_client* client, uint16_t reg, uint8_t val, uint8_t target)
{
    unsigned char buf[1] = {0x0};
    struct i2c_client *fd = client;
    unsigned int retry_new_cmd_cnt = 0;
    unsigned int try_cnt = 0;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    EYS3D_I2C_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

retry_new_cmd:
    if (retry_new_cmd_cnt == EYS3D_API_RETRY_NEW_CMD_MAX_CNT) {
        ret = -EBUSY;
        EYS3D_ERR_DBG_PRT("[0x%04X] ?= [0x%02X](retry %u times) (W:0x%02X)(T:0x%02X)\n", reg, buf[0], EYS3D_API_RETRY_NEW_CMD_MAX_CNT, val, target);
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    try_cnt = 0;

    ret = eys3d_wr_reg(fd, reg, val);
    I2C_ERR_HANDLE(ret, ret_line);

    EYS3D_I2C_DBG_PRT("Wait for [0x%04X] = [0x%02X](%d)\n", reg, target, ret);
    while (1) {
        ret = eys3d_rd_reg(fd, reg, buf);
        if (try_cnt == EYS3D_API_I2C_READ_TRY_MAX_CNT) {
            EYS3D_ERR_DBG_PRT("[0x%04X] ?= [0x%02X](%ums) (W:0x%02X)(T:0x%02X)\n", reg, buf[0], (EYS3D_API_SLEEP_PARM * EYS3D_API_I2C_READ_TRY_MAX_CNT), val, target);
            try_cnt = 0;
            ret = -EBUSY;
            if (val == EYS3D_API_CRF_DEV_RDY_ENTER) {
                retry_new_cmd_cnt++;
                goto retry_new_cmd;
            } else {
                ret_line = __LINE__;
                goto exit;
            }
        }
        if (buf[0] == target) {
            ret = EYS3D_API_RET_OK;
            break;
        }
        else eys3d_sleep(EYS3D_API_SLEEP_PARM);
        try_cnt++;
    }

exit:
    EYS3D_I2C_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

static int esp876_rd_next(struct i2c_client* client, unsigned char *input, int size, bool has_next)
{
    int length = size;
    int i = 0;
    int ret = EYS3D_API_RET_OK;
    struct i2c_client *fd = client;

    for (i = 0; i < length; i++) {
        eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, (unsigned char *)&input[i]);
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);

    if (has_next) {
        ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_NEX_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
        if (ret != EYS3D_API_RET_OK) {
            ret = -EBUSY;
            goto exit;
        }
    }

exit:
    return ret;
}

static int esp876_wr_next(struct i2c_client* client, unsigned char *input, int size, bool has_next, bool first_wr_)
{
    int length = size;
    int i = 0;
    int ret = EYS3D_API_RET_OK;
    struct i2c_client *fd = client;

    for (i = 0; i < length; i++) {
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, input[i]);
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);

    if (first_wr_) {
        ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
        if (ret != EYS3D_API_RET_OK) {
            ret = -EBUSY;
            goto exit;
        }
    } else {
        if (has_next) {
            ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_NEX_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
            if (ret != EYS3D_API_RET_OK) {
                ret = -EBUSY;
                goto exit;
            }
        }
    }

exit:
    return ret;
}

int esp876_get_pid_vid(struct i2c_client* client, uint16_t *pid, uint16_t *vid)
{
    int ret = EYS3D_API_RET_OK;
    uint8_t data[32] = {0x0};
    uint8_t id = 1;
    uint16_t val = 0;

    if (!pid || !vid)
        return -EINVAL;

    *pid = 0;
    *vid = 0;

    /* 0x4e, 0x1e, 0x67, 0x01 */
    ret = esp876_rd_fw_data(client, EYS3D_API_IO_REQ_MEM_TP_FS_TABLE, id, 0, data, 32);
    if (ret == EYS3D_API_RET_OK) {
        val = data[3];
        val = val << 8;
        val |= data[2];
        *pid = val;

        val = 0;
        val = data[1];
        val = val << 8;
        val |= data[0];
        *vid = val;
    }

    return ret;
}

int esp876_get_fw_ver(struct i2c_client* client, char *version)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_RD;
    int i = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd || !version) {
        ret = -EINVAL;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x05);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x0F);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x20);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    for (i = 0; i < 32; i++) {
        eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, (unsigned char *)&version[i]);
    }


    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    version[32] = '\0';

exit:
    EYS3D_API_DBG_PRT("Leave[%d]\n", ret);
    LEAVE_CS

    return ret;
}

int esp876_enable_mipi_output(struct i2c_client* client, bool en)
{
    int ret = EYS3D_API_RET_OK;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (en) {
        ret = esp876_wr_fw_reg(client, 0xF1, 0x10); // MIPI output
        ENTER_CS
        if (ret == EYS3D_API_RET_OK) {
            priv->mipi_out_en = true;
        } else {
            priv->mipi_out_en = false;
        }
        LEAVE_CS
    } else {
        ret = esp876_wr_fw_reg(client, 0xF1, 0x00); // USB output
        ENTER_CS
        if (ret == EYS3D_API_RET_OK) {
            priv->mipi_out_en = false;
        }
        LEAVE_CS
    }

    EYS3D_API_DBG_PRT("Leave[%d]\n", ret);

    return ret;
}

int esp876_video_switch(struct i2c_client* client, uint32_t mode_index)
{
    unsigned char cmdOpt_02 = 0;
    unsigned char cmdOpt_03 = 0;
    unsigned char cmdOpt_04 = 0;
    unsigned char cmdOpt_05 = 0;
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_OPEN_VIDEO;
    const struct eys3d_video_mode *mode = NULL;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_ALERT_DBG_PRT("Enter[0x%08X], VC[%d]\n", mode_index, priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (!priv->supported_video_modes) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    mode = &priv->supported_video_modes[mode_index];
    cmdOpt_02 = mode->param.cmdOpt_02;
    cmdOpt_03 = mode->param.cmdOpt_03;
    cmdOpt_04 = mode->param.cmdOpt_04;
    cmdOpt_05 = mode->param.cmdOpt_05;

    EYS3D_API_DBG_PRT("[%u][0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", mode_index, cmdOpt_02, cmdOpt_03, cmdOpt_04, cmdOpt_05);

    eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmdOpt_02);
    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmdOpt_03);
    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmdOpt_04);
    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmdOpt_05);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_ALERT_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_video_close(struct i2c_client* client)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_COLSE_VIDEO;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS;
#if EYS3D_API_IGNORE_I2C_KEEP_MIPI
    EYS3D_ALERT_DBG_PRT("Enter, IGNORE_I2C_KEEP_MIPI goto exit\n");
    ret_line = __LINE__;
    goto exit;
#else
    EYS3D_ALERT_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);
#endif

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0xFF); //[0xDC02] [0xFF]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00); //[0xDC03] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00); //[0xDC04] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00); //[0xDC05] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00); //[0xDC06] [0x00]

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_ALERT_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_rd_fw_reg(struct i2c_client* client, uint8_t reg, uint8_t *val)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_RD;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                           // [0xDC01] [0x81]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, EYS3D_API_IO_REQ_MEM_TP_FW_REG);// [0xDC02] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC03] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC04] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC05] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC06] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, reg);                           // [0xDC07] [Reg]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC08] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x01);                          // [0xDC09] [0x01]

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, val);                           // [0xDC20] [Val]
    EYS3D_API_DBG_PRT("[EYS3D_API_CMD_DAT_REG] [0x%02X]\n", *val);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_wr_fw_reg(struct i2c_client* client, uint8_t reg, uint8_t val)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_WR;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter[0x%02X, 0x%02X], VC[%d]\n", reg, val, priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                           // [0xDC01] [0x81]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, EYS3D_API_IO_REQ_MEM_TP_FW_REG);// [0xDC02] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC03] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC04] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC05] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC06] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, reg);                           // [0xDC07] [Reg]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC08] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x01);                          // [0xDC09] [0x01]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, val);                           // [0xDC20] [0x01]

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_rd_asic_reg(struct i2c_client* client, uint16_t reg, uint8_t *data, uint16_t len)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_RD;
    int i = 0;
    uint32_t wr_buf = EYS3D_API_MAX_ASIC_REG_BUF_SZ;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (len > EYS3D_API_MAX_ASIC_REG_WR_LEN) {
        EYS3D_ERR_DBG_PRT("%u > %u\n", len, EYS3D_API_MAX_ASIC_REG_WR_LEN);
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                               // [0xDC01] [0x41]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, EYS3D_API_IO_REQ_MEM_TP_FW_SRAM);   // [0xDC02] [0x01]
    if (len > 1) {
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC03] [0x01]
    } else {
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x01);                          // [0xDC03] [0x01]
    }
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                              // [0xDC04] [00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                              // [0xDC05] [00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, reg >> 8);                          // [0xDC06] [Addr (MSB)]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, reg & 0xFF);                        // [0xDC07] [Addr (LSB)]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len >> 8);                          // [0xDC08] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len & 0xFF);                        // [0xDC09] [0x01]

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    if (len <= wr_buf) {
        for (i = 0; i < len; i++) {
            eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, (unsigned char *)&data[i]);
        }
    } else {
        int count = len / wr_buf;
        int diff = len - (count * wr_buf);
        bool has_next = true;

        for (i = 0; i < count; i++){
            if ((diff == 0) && (i == (count -1))) {
                has_next = false;
            }
            ret = esp876_rd_next(fd, &data[i * wr_buf], wr_buf, has_next);
            if (ret != EYS3D_API_RET_OK) {
                ret_line = __LINE__;
                goto exit;
            }
        }

        if (diff != 0) {
            ret = esp876_rd_next(fd, &data[i * wr_buf], diff, false);
        }
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_wr_asic_reg(struct i2c_client* client, uint16_t reg, uint8_t *data, uint16_t len)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_WR;
    int i = 0;
    uint32_t wr_buf = EYS3D_API_MAX_ASIC_REG_BUF_SZ;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter(0x%04X, 0x%02X), VC[%d]\n", reg, *data, priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (len > EYS3D_API_MAX_ASIC_REG_WR_LEN) {
        EYS3D_ERR_DBG_PRT("%u > %u\n", len, EYS3D_API_MAX_ASIC_REG_WR_LEN);
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                               // [0xDC01] [0x81]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, EYS3D_API_IO_REQ_MEM_TP_FW_SRAM);   // [0xDC02] [0x01]
    if (len > 1) {
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC03] [0x01]
    } else {
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x01);                          // [0xDC03] [0x01]
    }
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                              // [0xDC04] [00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                              // [0xDC05] [00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, reg >> 8);                          // [0xDC06] [Addr (MSB)]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, reg & 0xFF);                        // [0xDC07] [Addr (LSB)]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len >> 8);                          // [0xDC08] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len & 0xFF);                        // [0xDC09] [0x01]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    if (len <= wr_buf) {
        for (i = 0; i < len; i++) {
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data[i]);                   // [[0xDC20] =[Data]
        }

        ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
        if (ret != EYS3D_API_RET_OK) {
            ret = -EBUSY;
            ret_line = __LINE__;
            goto exit;
        }
    } else {
        int count = len / wr_buf;
        int diff = len - (count * wr_buf);
        bool has_next = true;
        bool first_w = true;

        for (i = 0; i < count; i++){
            if ((diff == 0) && (i == (count -1))) {
                has_next = false;
            }
            ret = esp876_wr_next(fd, &data[i * wr_buf], wr_buf, has_next, first_w);
            if (ret != EYS3D_API_RET_OK) {
                ret_line = __LINE__;
                goto exit;
            }
            if (first_w) {
                first_w = false;
            }
        }

        if (diff != 0) {
            ret = esp876_wr_next(fd, &data[i * wr_buf], diff, false, first_w);
        }
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_rd_fd(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t *data, uint16_t len)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    int i = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_RD;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_I2C_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (!data) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }
/*
    //The max buffer length is EYS3D_API_MAX_FS_BUF_SZ bytes.
    if (len > EYS3D_API_MAX_FS_BUF_SZ) {
        ret = -EINVAL;
        goto exit;
    }

    eys3d_sleep(500);
*/

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    /*
     * data[0] = pFWFSData->nMutex;
     * data[1] = 0x40 | 0x01; //b[7:6]=1 Read, b[5:0]=1 Memory I/O Command
     * data[2] = 0x05;  //FileSystem Table
     * data[3] = 0; //Acess Same Address
     * data[4] = 0;
     * data[5] = pFWFSData->nFsID; //FileSystem ID
     * data[6] = (BYTE)((pFWFSData->nOffset >> 8) & 0xFF); //Offset-H
     * data[7] = (BYTE)(pFWFSData->nOffset & 0xFF); //Offset-L
     * data[8] = 0;
     * data[9] = (BYTE)pFWFSData->nLength; //Read-Data Length
     */

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                   // [0xDC01] [0x41]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, type);                  // [0xDC02] [MemType]
    switch (type) {
        case EYS3D_API_IO_REQ_MEM_TP_SPI_FLASH:
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);          // [0xDC03] [0x00]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset >> 24);  // [0xDC04] [0x00]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset >> 16);  // [0xDC05] [ID]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset >> 8);   // [0xDC06] [Offset (MSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset & 0xFF); // [0xDC07] [Offset (LSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len >> 8);      // [0xDC08] [Len (MSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len & 0xFF);    // [0xDC09] [Len (LSB)]
            break;
        case EYS3D_API_IO_REQ_MEM_TP_FS_TABLE:
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);          // [0xDC03] [0x00]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);          // [0xDC04] [0x00]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, id);            // [0xDC05] [ID]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset >> 8);   // [0xDC06] [Offset (MSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset & 0xFF); // [0xDC07] [Offset (LSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len >> 8);      // [0xDC08] [Len (MSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len & 0xFF);    // [0xDC09] [Len (LSB)]
            break;
        default:
            break;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    for (i = 0; i < len; i++) {
        eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, (unsigned char *)&data[i]);
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_wr_fd(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t *data, uint16_t len)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    int i = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_WR;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_I2C_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (!data) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }
/*
    //The max buffer length is EYS3D_API_MAX_FS_BUF_SZ bytes.
    if (len > EYS3D_API_MAX_FS_BUF_SZ) {
        ret = -EINVAL;
        goto exit;
    }

    eys3d_sleep(500);
*/

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    /*
     * data[0] = pFWFSData->nMutex;
     * data[1] = 0x80 | 0x01; //b[7:6]=2 Write, b[5:0]=1 FileSystem Table
     * data[2] = 0x05;  //FileSystem Table
     * data[3] = 0; //Acess Same Address
     * data[4] = 0;
     * data[5] = pFWFSData->nFsID; //FileSystem ID
     * data[6] = (BYTE)((pFWFSData->nOffset >> 8) & 0xFF); //Offset-H
     * data[7] = (BYTE)(pFWFSData->nOffset & 0xFF); //Offset-L
     * data[8] = 0;
     * data[9] = (BYTE)pFWFSData->nLength; //Write-Data Length
     */

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                   // [0xDC01] [0x81]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, type);                  // [0xDC02] [MemType]
    switch (type) {
        case EYS3D_API_IO_REQ_MEM_TP_SPI_FLASH:
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);          // [0xDC03] [0x00]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset >> 24);  // [0xDC04] [0x00]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset >> 16);  // [0xDC05] [ID]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset >> 8);   // [0xDC06] [Offset (MSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset & 0xFF); // [0xDC07] [Offset (LSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len >> 8);      // [0xDC08] [Len (MSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len & 0xFF);    // [0xDC09] [Len (LSB)]
            break;
        case EYS3D_API_IO_REQ_MEM_TP_FS_TABLE:
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);          // [0xDC03] [0x00]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);          // [0xDC04] [0x00]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, id);            // [0xDC05] [ID]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset >> 8);   // [0xDC06] [Offset (MSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, offset & 0xFF); // [0xDC07] [Offset (LSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len >> 8);      // [0xDC08] [Len (MSB)]
            eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, len & 0xFF);    // [0xDC09] [Len (LSB)]
            break;
        default:
            break;
    }

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    for (i = 0; i < len; i++) {
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data[i]);           // [[0xDC20] [Data]
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }
exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_sync_fd(struct i2c_client* client)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_DAT_SYNC;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                               // [0xDC01] [0x02]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, EYS3D_API_IO_REQ_MEM_TP_FW_SRAM);   // [0xDC02] [0x01]

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_rd_fw_i2c_reg(struct i2c_client* client, uint8_t id, uint8_t format, uint16_t addr, uint16_t *data)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint16_t addr_len = 0;
    uint16_t data_len = 0;
    uint8_t tmp8 = 0;
    uint16_t tmp16 = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_RD;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    if (!data) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

#if defined(EYS3D_API_APN_REV1_9_20220114)
    /* NOTE: 16-Bit Address/8-Bit Data,  16-Bit Address/16-Bit Data, 8-Bit Address/16-Bit Data, 8-Bit Address/8-Bit Data */
    if (format == 0x10) {
        //16-Bit Address/8-Bit Data
        addr_len = 2;
        data_len = 1;
    } else  if (format == 0x30) {
        //16-Bit Address/16-Bit Data
        addr_len = 2;
        data_len = 2;
    } else  if (format == 0x20) {
        //8-Bit Address/16-Bit Data
        addr_len = 1;
        data_len = 2;
    } else {
        //8-Bit Address/8-Bit Data
        addr_len = 1;
        data_len = 1;
    }
#else
    /* NOTE: 16-Bit Address/8-Bit Data,  16-Bit Address/16-Bit Data, 8-Bit Address/16-Bit Data, 8-Bit Address/8-Bit Data */
    if (format == 0x20) {
        //16-Bit Address/8-Bit Data
        addr_len = 2;
        data_len = 1;
    } else  if (format ==  0x30) {
        //16-Bit Address/16-Bit Data
        addr_len = 2;
        data_len = 2;
    } else  if (format ==  0x10) {
        //8-Bit Address/16-Bit Data
        addr_len = 1;
        data_len = 2;
    } else {
        //8-Bit Address/8-Bit Data
        addr_len = 1;
        data_len = 1;
    }
#endif

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                           // [0xDC01] [0x41]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, EYS3D_API_IO_REQ_MEM_TP_I2C);   // [0xDC02] [0x02]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, format);                        // [0xDC03] [Format]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC04] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, id);                            // [0xDC05] [ID]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, addr >> 8);                     // [0xDC06] [Addr (MSB)]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, addr & 0xFF);                   // [0xDC07] [Addr (LSB)]
#if defined(EYS3D_API_APN_REV1_9_20220114)
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data_len >> 8);                 // [0xDC08] [Len (MSB)]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data_len & 0xFF);               // [0xDC09] [Len (LSB)]
#else
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data_len);                      // [0xDC08] [Len]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x01);                          // [0xDC09] [0x01]
#endif

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    if (data_len == 1) {
        tmp8 = 0;
        tmp16 = 0;
        eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);
        EYS3D_API_DBG_PRT("[EYS3D_API_CMD_DAT_REG] [0x%02X]\n", tmp8);
        tmp16 = tmp8;
        *data = (*data & ~0x00FF) | tmp16;
    } else if (data_len == 2) {
        tmp8 = 0;
        tmp16 = 0;
        eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);    // Read MSB
        EYS3D_API_DBG_PRT("[EYS3D_API_CMD_DAT_REG] [0x%02X]\n", tmp8);
        tmp16 = tmp8;
        *data = (*data & ~0x00FF) | tmp16;

        tmp8 = 0;
        tmp16 = 0;
        eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);    // Read LSB
        EYS3D_API_DBG_PRT("[EYS3D_API_CMD_DAT_REG] [0x%02X]\n", tmp8);
        tmp16 = tmp8;
        *data = (*data << 8) | tmp16;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

int esp876_wr_fw_i2c_reg(struct i2c_client* client, uint8_t id, uint8_t format, uint16_t addr, uint16_t data)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    uint16_t addr_len = 0;
    uint16_t data_len = 0;
    uint8_t cmd = EYS3D_API_CMD_TP_DEV_IO_REQ_WR;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter(0x%04X, 0x%02X), VC[%d]\n", addr, data, priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

#if defined(EYS3D_API_APN_REV1_9_20220114)
    /* NOTE: 16-Bit Address/8-Bit Data,  16-Bit Address/16-Bit Data, 8-Bit Address/16-Bit Data, 8-Bit Address/8-Bit Data */
    if (format == 0x10) {
        //16-Bit Address/8-Bit Data
        addr_len = 2;
        data_len = 1;
    } else  if (format == 0x30) {
        //16-Bit Address/16-Bit Data
        addr_len = 2;
        data_len = 2;
    } else  if (format == 0x20) {
        //8-Bit Address/16-Bit Data
        addr_len = 1;
        data_len = 2;
    } else {
        //8-Bit Address/8-Bit Data
        addr_len = 1;
        data_len = 1;
    }
#else
    /* NOTE: 16-Bit Address/8-Bit Data,  16-Bit Address/16-Bit Data, 8-Bit Address/16-Bit Data, 8-Bit Address/8-Bit Data */
    if (format == 0x20) {
        //16-Bit Address/8-Bit Data
        addr_len = 2;
        data_len = 1;
    } else  if (format == 0x30) {
        //16-Bit Address/16-Bit Data
        addr_len = 2;
        data_len = 2;
    } else  if (format == 0x10) {
        //8-Bit Address/16-Bit Data
        addr_len = 1;
        data_len = 2;
    } else {
        //8-Bit Address/8-Bit Data
        addr_len = 1;
        data_len = 1;
    }
#endif

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);                           // [0xDC01] [0x81]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, EYS3D_API_IO_REQ_MEM_TP_I2C);   // [0xDC02] [0x02]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, format);                        // [0xDC03] [Format]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);                          // [0xDC04] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, id);                            // [0xDC05] [ID]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, addr >> 8);                     // [0xDC06] [Addr (MSB)]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, addr & 0xFF);                   // [0xDC07] [Addr (LSB)]
#if defined(EYS3D_API_APN_REV1_9_20220114)
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data_len >> 8);                 // [0xDC08] [Len (MSB)]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data_len & 0xFF);               // [0xDC09] [Len (LSB)]
#else
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data_len);                      // [0xDC08] [Len]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x01);                          // [0xDC09] [0x01]
#endif

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    if (data_len == 1) {
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data & 0xFF);
    } else if (data_len == 2) {
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data >> 8);   // Write MSB
        eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, data & 0xFF); // Write LSB
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);
    LEAVE_CS

    return ret;
}

static int esp876_property_bar_val_mask(uint8_t id, uint8_t ctrsel, uint32_t *mask)
{
    int i = 0;
    int num = 0;
    const struct eys3d_uvc_ctrl *ctrl = NULL;

    if (!mask)
        return -EINVAL;

    /* 0x01: CT, 0x03: PU */
    if (id == EYS3D_API_PROP_ID_PU) {
        ctrl = g_uvc_pu_ctrls;
        num = sizeof(g_uvc_pu_ctrls) / sizeof(struct eys3d_uvc_ctrl);
    } else if (id == EYS3D_API_PROP_ID_CT) {
        ctrl = g_uvc_ct_ctrls;
        num = sizeof(g_uvc_ct_ctrls) / sizeof(struct eys3d_uvc_ctrl);
    } else {
        return -EINVAL;
    }

    *mask = 0;

    for (i = 0; i < num; i++) {
        if (ctrl[i].id == ctrsel) {
            switch (ctrl[i].val_len) {
                case 0:
                    *mask = 0;
                    break;
                case 1:
                    *mask = 0xFF;
                    break;
                case 2:
                    *mask = 0xFFFF;
                    break;
                case 3:
                    *mask = 0xFFFFFF;
                    break;
                case 4:
                    *mask = 0xFFFFFFFF;
                    break;
                default:
                    *mask = 0xFFFFFFFF;
            }
            break;
        }
    }

    return EYS3D_API_RET_OK;
}

int esp876_rd_prop_bar(struct i2c_client* client, uint8_t id, uint8_t type, uint8_t ctrsel, uint32_t *val)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    uint8_t tmp8 = 0x00;
    uint8_t tmp32 = 0x00;
    uint8_t cmd = EYS3D_API_CMD_TP_GET_PROP_BAR;
    uint32_t mask = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;
    (void)mask;
    (void)esp876_property_bar_val_mask;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    /* 0x01: CT, 0x03: PU */
    if ((id != EYS3D_API_PROP_ID_CT) &&
        (id != EYS3D_API_PROP_ID_PU)) {
        ret = -EINVAL;
        goto exit;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);   // [0xDC01] [0x81]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, id);    // [0xDC02] [CT/PD]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, type);  // [0xDC03] [0x01]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, ctrsel);// [0xDC04] [CtrSel]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC05] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC06] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC07] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC08] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC09] [0x00]

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    tmp8 = 0;
    tmp32 = 0;
    eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);
    tmp32 = tmp8;
    *val = (*val & ~0x000000FF) | tmp32;

    tmp8 = 0;
    tmp32 = 0;
    eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);
    tmp32 = tmp8;
    *val = (*val & ~0x0000FF00) | (tmp32 << 8);

    tmp8 = 0;
    tmp32 = 0;
    eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);
    tmp32 = tmp8;
    *val = (*val & ~0x00FF0000) | (tmp32 << 16);

    tmp8 = 0;
    tmp32 = 0;
    eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);
    tmp32 = tmp8;
    *val = (*val & ~0xFF000000) | (tmp32 << 24);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    if (esp876_property_bar_val_mask(id, ctrsel, &mask) == 0) {
        *val = *val & mask;
    }

    /* NOTE: The CAMERA_HW_GET_INFO should be 1 bytes [page 75 of USB_Video_Class_1.1.pdf]:
     * The GET_INFO request queries the capabilities and status of the specified control. When issuing
     * this request, the wLength field shall always be set to a value of 1 byte. The result returned is a
     * bit mask reporting the capabilities of the control.
     */
    if (type == CAMERA_HW_GET_INFO) {
        *val = *val & 0xFF;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d]\n", ret);
    LEAVE_CS

    return ret;
}

int esp876_wr_prop_bar(struct i2c_client* client, uint8_t id, uint8_t type, uint8_t ctrsel, uint32_t val)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    uint8_t cmd = EYS3D_API_CMD_TP_SET_PROP_BAR;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    /* 0x01: CT, 0x03: PU */
    if ((id != EYS3D_API_PROP_ID_CT) &&
        (id != EYS3D_API_PROP_ID_PU)) {
        ret = -EINVAL;
        goto exit;
    }

    //eys3d_sleep(500);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);   // [0xDC01] [0x81]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, id);    // [0xDC02] [CT/PD]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, type);  // [0xDC03] [0x01]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, ctrsel);// [0xDC04] [CtrSel]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC05] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC06] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC07] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC08] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC09] [0x00]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, (val & 0x000000FF));
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, (val & 0x0000FF00) >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, (val & 0x00FF0000) >> 16);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, (val & 0xFF000000) >> 24);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

exit:
    EYS3D_API_DBG_PRT("Leave[%d]\n", ret);
    LEAVE_CS

    return ret;
}

int esp876_rd_prop_bar_sp_list(struct i2c_client* client, uint8_t id, uint16_t *val)
{
    struct i2c_client *fd = client;
    int ret = EYS3D_API_RET_OK;
    uint8_t tmp8 = 0x00;
    uint8_t tmp16 = 0x00;
    uint8_t cmd = EYS3D_API_CMD_TP_GET_SP_LIST;
    uint32_t mask = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;
    (void)mask;
    (void)esp876_property_bar_val_mask;

    ENTER_CS
    EYS3D_API_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    /* 0x01: CT, 0x03: PU */
    if ((id != EYS3D_API_PROP_ID_CT) &&
        (id != EYS3D_API_PROP_ID_PU)) {
        ret = -EINVAL;
        goto exit;
    }

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_DEV_RDY_ENTER, EYS3D_API_CRF_DEV_RDY_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    eys3d_sleep(EYS3D_API_SLEEP_PARM);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_REQ >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_REQ & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, cmd);   // [0xDC01] [0x81]

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_OPT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_OPT & 0xFF);
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, id);    // [0xDC02] [CT/PD]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC03] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC04] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC05] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC06] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC07] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC08] [0x00]
    eys3d_wr_reg(fd, EYS3D_API_CMD_DAT_REG, 0x00);  // [0xDC09] [0x00]

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_PRE_ENTER, EYS3D_API_CRF_CMD_PRE_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_MSB_REG, EYS3D_API_CMD_DAT >> 8);
    eys3d_wr_reg(fd, EYS3D_API_CMD_ADR_LSB_REG, EYS3D_API_CMD_DAT & 0xFF);
    tmp8 = 0;
    tmp16 = 0;
    eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);
    tmp16 = tmp8;
    *val = (*val & ~0x00FF) | tmp16;

    tmp8 = 0;
    tmp16 = 0;
    eys3d_rd_reg(fd, EYS3D_API_CMD_DAT_REG, &tmp8);
    tmp16 = tmp8;
    *val = (*val & ~0xFF00) | (tmp16 << 8);

    ret = esp876_wait_reg_for_target(fd, EYS3D_API_CMD_TRIG_REG, EYS3D_API_CRF_CMD_CLS_ENTER, EYS3D_API_CRF_CMD_CLS_LEAVE);
    if (ret != EYS3D_API_RET_OK)
        goto exit;

exit:
    EYS3D_API_DBG_PRT("Leave[%d]\n", ret);
    LEAVE_CS

    return ret;
}

int esp876_rd_fw_data(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t* data, uint16_t len)
{
    uint8_t *ptr = data;
    uint16_t nOffset = 0;
    uint16_t nLength = 0;
    int ret = EYS3D_API_RET_OK;

    if (!ptr || !client) {
        ret = -EINVAL;
        goto exit;
    }

    if (len <= EYS3D_API_MAX_FS_BUF_SZ) {
        ret = esp876_rd_fd(client, type, id, offset, ptr, len);
        if (ret != EYS3D_API_RET_OK) {
            goto exit;
        }
    } else {
        nOffset = 0;
        while (nOffset < len) {
            nLength = len - nOffset;

            if (nLength > EYS3D_API_MAX_FS_BUF_SZ) {
                nLength = EYS3D_API_MAX_FS_BUF_SZ;
            }

            ret = esp876_rd_fd(client, type, id, nOffset, (ptr + nOffset), nLength);
            if (ret != EYS3D_API_RET_OK) {
                goto exit;
            }
            nOffset += nLength;
        }
    }

exit:
    return ret;
}

int esp876_wr_fw_data(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t* data, uint16_t len)
{
    uint8_t *ptr = data;
    uint16_t nOffset = 0;
    uint16_t nLength = 0;
    int ret = EYS3D_API_RET_OK;

    if (!ptr || !client) {
        ret = -EINVAL;
        goto exit;
    }

    if (len <= EYS3D_API_MAX_FS_BUF_SZ) {
        ret = esp876_wr_fd(client, type, id, offset, ptr, len);
        if (ret != EYS3D_API_RET_OK) {
            goto exit;
        }
    } else {
        nOffset = 0;
        while (nOffset < len) {
            nLength = len - nOffset;

            if (nLength > EYS3D_API_MAX_FS_BUF_SZ) {
                nLength = EYS3D_API_MAX_FS_BUF_SZ;
            }

            ret = esp876_wr_fd(client, type, id, nOffset, (ptr + nOffset), nLength);
            if (ret != EYS3D_API_RET_OK) {
                goto exit;
            }
            nOffset += nLength;
        }
    }

    ret = esp876_sync_fd(client);

exit:
    return ret;
}

int eys3d_esp876_start_stream(struct eys3d *driv)
{
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    struct eys3d_api_data *priv = driv->data;
    struct i2c_client     *client = driv->client;

    if (priv->is_initialed == false) {
        ret_line = __LINE__;
        return -EINVAL;
    }

#if EYS3D_API_IGNORE_I2C_KEEP_MIPI
    EYS3D_ALERT_DBG_PRT("Enter, IGNORE_I2C_KEEP_MIPI goto exit\n");
    ret_line = __LINE__;
    goto exit;
#else
    EYS3D_ALERT_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);
#endif

    EYS3D_API_DBG_PRT("Is MIPI output enabled? (%d)\n", priv->mipi_out_en);
    if (priv->mipi_out_en == false) {
        ret = -EIO;
        goto exit;
    }

    if (priv->is_first_set_video_mode) {
        ret = esp876_video_switch(client, priv->cur_video_mode_index);
        if (ret != EYS3D_API_RET_OK) {
            EYS3D_ERR_DBG_PRT("Failed to switch video mode\n");
            ret_line = __LINE__;
            goto exit;
        }
    }

    if (priv->pre_video_mode_index != priv->cur_video_mode_index) {
        EYS3D_API_DBG_PRT("switch video mode!(0x%08X -> 0x%08X)\n", priv->pre_video_mode_index, priv->cur_video_mode_index);
        priv->pre_video_mode_index = priv->cur_video_mode_index;
    }

    if (priv->is_first_set_video_mode) {
        ret = esp876_video_close(client);
        if (ret != EYS3D_API_RET_OK) {
            EYS3D_ERR_DBG_PRT("Failed to close video mode\n");
            ret_line = __LINE__;
            goto exit;
        }
    }

    ret = esp876_video_switch(client, priv->cur_video_mode_index);
    if (ret != EYS3D_API_RET_OK) {
        EYS3D_ERR_DBG_PRT("Failed to switch video mode\n");
        ret_line = __LINE__;
        goto exit;
    }

exit:
    ENTER_CS
    if (priv->is_first_set_video_mode) {
        priv->is_first_set_video_mode = false;
    }

    if (ret == EYS3D_API_RET_OK) {
        priv->stream_on = true;
    } else {
        priv->stream_on = false;
    }
    EYS3D_ALERT_DBG_PRT("Is stream on? [%d, %d, %d]\n", priv->stream_on, ret, ret_line);
    LEAVE_CS

    return ret;
}

int eys3d_esp876_stop_stream(struct eys3d *driv)
{
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    struct eys3d_api_data *priv = driv->data;
    struct i2c_client     *client = driv->client;

    EYS3D_ALERT_DBG_PRT("Enter, VC[%d]\n", priv->vc_id);

    if (priv->is_initialed == false) {
        ret_line = __LINE__;
        return -EINVAL;
    }

    ret = esp876_video_close(client);
    if (ret != EYS3D_API_RET_OK) {
        EYS3D_ERR_DBG_PRT("Failed to close video mode\n");
        ret_line = __LINE__;
        goto exit;
    }

exit:
    ENTER_CS
    if (ret == EYS3D_API_RET_OK) {
        priv->stream_on = false;
    } else {
        priv->stream_on = true;
    }
    EYS3D_ALERT_DBG_PRT("Is stream on? [%d, %d, %d]\n", priv->stream_on, ret, ret_line);
    LEAVE_CS

    return ret;
}
