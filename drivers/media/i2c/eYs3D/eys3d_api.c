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
#include "eys3d_video_modes.c"
#if EYS3D_CAMERA_MODULE_IS_ESP876
#include "eys3d_esp876.h"
#elif EYS3D_CAMERA_MODULE_IS_P2C
#include "eys3d_p2c.h"
#endif

static struct proc_dir_entry *g_proc_i2c[EYS3D_API_SP_DEV_CNT]  = {NULL};
static struct proc_dir_entry *g_proc_version                    = NULL;
static int g_proc_created_count                                 = 0;
static struct mutex g_proc_lock                                 = __MUTEX_INITIALIZER(g_proc_lock);
static bool g_is_enable_version_check                           = true;
struct i2c_client* g_eys3d_i2c_client                           = NULL;
struct mutex g_eys3d_api_mutex                                  = __MUTEX_INITIALIZER(g_eys3d_api_mutex);

void eys3d_sleep(unsigned int ms)
{
    if (ms <= 20)
        usleep_range((ms * 1000), (ms * 1000));
    else
        msleep(ms);
}

static int eys3d_set_error(struct i2c_client* client, int code)
{
    int ret = EYS3D_API_RET_OK;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    ENTER_CS
    priv->cur_error_code = code;
    LEAVE_CS

    return ret;
}

static int eys3d_set_is_opened(struct i2c_client* client, bool val)
{
    int ret = EYS3D_API_RET_OK;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    ENTER_CS
    priv->is_opened = val;
    LEAVE_CS

    return ret;
}

static int eys3d_parse_cmd_cfg(const char *cmd, char *param[], int param_count, const char *delim)
{
    char *found = NULL;
    int i = 0;
    char *cmd_tmp = NULL;
    char **param_tmp = NULL;

    cmd_tmp = (char *)cmd;

    while( (found = strsep(&cmd_tmp, delim)) != NULL ) {
        if (i >= param_count)
            break;

        param_tmp = &param[i];
        *param_tmp = found;
        i++;
    }

    return EYS3D_API_RET_OK;
}

/* Read registers up to 4 at a time */
static int eys3d_rd_host_i2c_reg(struct i2c_client* client, uint16_t reg, unsigned int len, uint32_t *val)
{
    struct i2c_msg msgs[2];
    uint8_t *data_be_p;
    __be32 data_be = 0;
    __be16 reg_addr_be = cpu_to_be16(reg);
    int ret = EYS3D_API_RET_OK;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    if (len > 4 || !len)
        return -EINVAL;

    data_be_p = (uint8_t *)&data_be;
    /* Write register address */
    msgs[0].addr    = priv->client->addr;
    msgs[0].flags   = 0;
    msgs[0].len     = 2;
    msgs[0].buf     = (uint8_t *)&reg_addr_be;

    /* Read data from register */
    msgs[1].addr    = priv->client->addr;
    msgs[1].flags   = I2C_M_RD;
    msgs[1].len     = len;
    msgs[1].buf     = &data_be_p[4 - len];

    ret = i2c_transfer(priv->client->adapter, msgs, ARRAY_SIZE(msgs));
    if (ret != ARRAY_SIZE(msgs))
        return -EIO;

    *val = be32_to_cpu(data_be);

    return ret;
}

/* Write registers up to 4 at a time */
static int eys3d_wr_host_i2c_reg(struct i2c_client* client, uint16_t reg, uint32_t len, uint32_t val)
{
    uint32_t buf_i, val_i;
    uint8_t buf[6];
    uint8_t *val_p;
    __be32 val_be;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    if (len > 4)
        return -EINVAL;

    buf[0]  = reg >> 8;
    buf[1]  = reg & 0xFF;
    val_be  = cpu_to_be32(val);
    val_p   = (uint8_t *)&val_be;
    buf_i   = 2;
    val_i   = 4 - len;

    while (val_i < 4)
        buf[buf_i++] = val_p[val_i++];

    if (i2c_master_send(priv->client, buf, len + 2) != len + 2)
        return -EIO;

    return EYS3D_API_RET_OK;
}

int eys3d_rd_reg(struct i2c_client* client, uint16_t reg, uint8_t *val)
{
    int ret = EYS3D_API_RET_OK;

    uint32_t lval = 0;
    ret = eys3d_rd_host_i2c_reg(client, reg, EYS3D_API_I2C_REG_VALUE_08BIT, &lval);
    *val = lval;

    EYS3D_I2C_DBG_PRT("%s: [0x%04X] [0x%02X](%d)\n", client->adapter->name, reg, *val, ret);

    return ret;
}

int eys3d_wr_reg(struct i2c_client* client, uint16_t reg, uint8_t val)
{
    int ret = EYS3D_API_RET_OK;

    uint32_t lval = val;
    ret = eys3d_wr_host_i2c_reg(client, reg, EYS3D_API_I2C_REG_VALUE_08BIT, lval);

    EYS3D_I2C_DBG_PRT("%s: [0x%04X] [0x%02X](%d)\n", client->adapter->name, reg, val, ret);

    return ret;
}

static int eys3d_get_pid_vid(struct i2c_client* client, uint16_t *pid, uint16_t *vid)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_get_pid_vid(client, pid, vid);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_get_pid_vid(client, pid, vid);
#endif

    return ret;
}

static int eys3d_get_fw_ver(struct i2c_client* client, char *version)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_get_fw_ver(client, version);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_get_fw_ver(client, version);
#endif

    return ret;
}

static int eys3d_enable_mipi_output(struct i2c_client* client, bool en)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_enable_mipi_output(client, en);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_enable_mipi_output(client, en);
#endif
        
    return ret;
}

static int eys3d_enable_sensor_if(struct i2c_client* client)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    // Nothing to do
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_enable_sensor_if(client);
#endif
        
    return ret;
}

static int eys3d_video_switch(struct i2c_client* client, uint32_t mode_index)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_video_switch(client, mode_index);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_video_switch(client, mode_index);
#endif

    return ret;
}

static int eys3d_video_close(struct i2c_client* client)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_video_close(client);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_video_close(client);
#endif
    return ret;
}

static int eys3d_rd_fw_reg(struct i2c_client* client, uint8_t reg, uint8_t *val)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_rd_fw_reg(client, reg, val);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_rd_fw_reg(client, reg, val);
#endif

    return ret;
}

static int eys3d_wr_fw_reg(struct i2c_client* client, uint8_t reg, uint8_t val)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_wr_fw_reg(client, reg, val);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_wr_fw_reg(client, reg, val);
#endif
    return ret;
}

static int eys3d_rd_asic_reg(struct i2c_client* client, uint16_t reg, uint8_t *data, uint16_t len)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_rd_asic_reg(client, reg, data, len);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_rd_asic_reg(client, reg, data, len);
#endif

    return ret;
}

static int eys3d_wr_asic_reg(struct i2c_client* client, uint16_t reg, uint8_t *data, uint16_t len)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_wr_asic_reg(client, reg, data, len);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_wr_asic_reg(client, reg, data, len);
#endif

    return ret;
}

static int eys3d_rd_fd(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t *data, uint16_t len)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_rd_fd(client, type, id, offset, data, len);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_rd_fd(client, type, id, offset, data, len);
#endif
    return ret;
}

static int eys3d_wr_fd(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t *data, uint16_t len)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_wr_fd(client, type, id, offset, data, len);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_wr_fd(client, type, id, offset, data, len);
#endif

    return ret;
}

static int eys3d_sync_fd(struct i2c_client* client)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_sync_fd(client);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_sync_fd(client);
#endif

    return ret;
}

static int eys3d_rd_fw_i2c_reg(struct i2c_client* client, uint8_t id, uint8_t format, uint16_t addr, uint16_t *data)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_rd_fw_i2c_reg(client, id, format, addr, data);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_rd_fw_i2c_reg(client, id, format, addr, data);
#endif
    return ret;
}

static int eys3d_wr_fw_i2c_reg(struct i2c_client* client, uint8_t id, uint8_t format, uint16_t addr, uint16_t data)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_wr_fw_i2c_reg(client, id, format, addr, data);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_wr_fw_i2c_reg(client, id, format, addr, data);
#endif

    return ret;
}

static int eys3d_rd_prop_bar(struct i2c_client* client, uint8_t id, uint8_t type, uint8_t ctrsel, uint32_t *val)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_rd_prop_bar(client, id, type, ctrsel, val);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_rd_prop_bar(client, id, type, ctrsel, val);
#endif

    return ret;
}

static int eys3d_wr_prop_bar(struct i2c_client* client, uint8_t id, uint8_t type, uint8_t ctrsel, uint32_t val)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_wr_prop_bar(client, id, type, ctrsel, val);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_wr_prop_bar(client, id, type, ctrsel, val);
#endif

    return ret;
}

static int eys3d_rd_prop_bar_sp_list(struct i2c_client* client, uint8_t id, uint16_t *val)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_rd_prop_bar_sp_list(client, id, val);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_rd_prop_bar_sp_list(client, id, val);
#endif

    return ret;
}

static int eys3d_rd_fw_data(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t* data, uint16_t len)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_rd_fw_data(client, type, id, offset, data, len);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_rd_fw_data(client, type, id, offset, data, len);
#endif

    return ret;
}

static int eys3d_wr_fw_data(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t* data, uint16_t len)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = esp876_wr_fw_data(client, type, id, offset, data, len);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = p2c_wr_fw_data(client, type, id, offset, data, len);
#endif

    return ret;
}

static uint32_t eys3d_get_current_vid_mode(struct i2c_client* client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    EYS3D_API_DBG_PRT("cur_video_mode_index[%d]\n", priv->cur_video_mode_index);

    return priv->cur_video_mode_index;
}

static int eys3d_set_current_vid_mode(struct i2c_client* client, uint32_t mode_index)
{
    int ret = EYS3D_API_RET_OK;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS

    priv->cur_video_mode_index = mode_index;
    EYS3D_API_DBG_PRT("cur_video_mode_index[%d]\n", priv->cur_video_mode_index);

    LEAVE_CS

    return ret;
}

static ssize_t eys3d_fs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    char *str = NULL;
    int str_len  = 1024;
    uint8_t set_val = 0;
    ssize_t ret_size = 0;
    uint32_t val_32 = 0;
    uint16_t val_16 = 0;
    uint8_t val_8 = 0;
    //int ret = EYS3D_API_RET_OK;
    bool is_call_get_error = false;
    int ret_api = 0;
    struct i2c_client* client = NULL;
    struct v4l2_subdev *sd = NULL;
    struct eys3d *driv = NULL;
    struct eys3d_api_data *priv = NULL;

    (void)driv;
    (void)priv;
    (void)val_32;
    (void)val_16;
    (void)val_8;

    if (!dev)
        return -EINVAL;

    client = to_i2c_client(dev);
    if (!client)
        return -EINVAL;

    sd = i2c_get_clientdata(client);
    if (!sd)
        return -EINVAL;

    driv = sd_to_eys3d(sd);
    if (!driv)
        return -EINVAL;

    priv = driv->data;
    if (!priv)
        return -EINVAL;

    str = kzalloc(str_len, GFP_KERNEL);
    if (!str)
        return -EINVAL;

    memset(str, 0x0, str_len);

    if (strcmp(attr->attr.name,
        __stringify(fw_version)) == 0) {
        set_val = 1;
        ret_api = eys3d_get_fw_ver(driv->client, str);
    } else if (strcmp(attr->attr.name,
        __stringify(video_mode)) == 0) {
        set_val = 1;
        val_32 = eys3d_get_current_vid_mode(driv->client);
        snprintf(str, str_len, "%u", val_32);
    } else if (strcmp(attr->attr.name,
        __stringify(erro_code)) == 0) {
        set_val = 1;
        is_call_get_error = true;
        snprintf(str, str_len, "%d", priv->cur_error_code);
    } else if (strcmp(attr->attr.name,
        __stringify(mipi_out_en)) == 0) {
        set_val = 1;
        snprintf(str, str_len, "%d", priv->mipi_out_en);
    } else if (strcmp(attr->attr.name,
        __stringify(stream_on)) == 0) {
        set_val = 1;
        snprintf(str, str_len, "%d", priv->stream_on);
    } else if (strcmp(attr->attr.name,
        __stringify(read_asic_reg)) == 0) {
        if (priv->is_rd_asic_reg) {
            set_val = 1;
            ret_api = eys3d_rd_asic_reg(driv->client, priv->cur_asic_reg, &val_8, 1);
            snprintf(str, str_len, "0x%02X", val_8);
            ENTER_CS
            priv->is_rd_asic_reg = false;
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_fw_reg)) == 0) {
        if (priv->is_rd_fw_reg) {
            set_val = 1;
            ret_api = eys3d_rd_fw_reg(driv->client, priv->cur_fw_reg, &val_8);
            snprintf(str, str_len, "0x%02X", val_8);
            ENTER_CS
            priv->is_rd_fw_reg = false;
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_i2_reg)) == 0) {
        if (priv->is_rd_i2c_reg) {
            set_val = 1;
            ret_api = eys3d_rd_fw_i2c_reg(driv->client, priv->cur_i2_slave_id, priv->cur_i2_format, priv->cur_i2_addr, &val_16);
#if defined(EYS3D_API_APN_REV1_9_20220114)
            if (priv->cur_i2_format == 0x10) {
                //16-Bit Address/8-Bit Data
                snprintf(str, str_len, "0x%02X", (uint8_t)(val_16 & 0xFF));
            } else  if (priv->cur_i2_format == 0x30) {
                //16-Bit Address/16-Bit Data
                snprintf(str, str_len, "0x%04X", val_16);
            } else  if (priv->cur_i2_format == 0x20) {
                //8-Bit Address/16-Bit Data
                snprintf(str, str_len, "0x%04X", val_16);
            } else {
                //8-Bit Address/8-Bit Data
                snprintf(str, str_len, "0x%02X", (uint8_t)(val_16 & 0xFF));
            }
#else
            if (priv->cur_i2_format == 0x20) {
                //16-Bit Address/8-Bit Data
                snprintf(str, str_len, "0x%02X", (uint8_t)(val_16 & 0xFF));
            } else  if (priv->cur_i2_format == 0x30) {
                //16-Bit Address/16-Bit Data
                snprintf(str, str_len, "0x%04X", val_16);
            } else  if (priv->cur_i2_format == 0x10) {
                //8-Bit Address/16-Bit Data
                snprintf(str, str_len, "0x%04X", val_16);
            } else {
                //8-Bit Address/8-Bit Data
                snprintf(str, str_len, "0x%02X", (uint8_t)(val_16 & 0xFF));
            }
#endif
            ENTER_CS
            priv->is_rd_i2c_reg = false;
            LEAVE_CS
        }
    }  else if (strcmp(attr->attr.name,
        __stringify(read_ct_pu_value)) == 0) {
        if (priv->is_rd_pb) {
            set_val = 1;
            ret_api = eys3d_rd_prop_bar(driv->client, priv->cur_pb_ct_pu_id, priv->cur_pb_type, priv->cur_pb_ctrsel, &val_32);
            snprintf(str, str_len, "0x%08X", val_32);
            ENTER_CS
            priv->is_rd_pb = false;
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_ct_pu_support_list)) == 0) {
        if (priv->is_rd_pb) {
            set_val = 1;
            ret_api = eys3d_rd_prop_bar_sp_list(driv->client, priv->cur_pb_ct_pu_id, &val_16);
            snprintf(str, str_len, "0x%04X", val_16);
            ENTER_CS
            priv->is_rd_pb = false;
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(fs_id)) == 0) {
        set_val = 1;
        snprintf(str, str_len, "%u", priv->cur_fs_id);
    } else if (strcmp(attr->attr.name,
        __stringify(fs_offset)) == 0) {
        set_val = 1;
        snprintf(str, str_len, "%u", priv->cur_fs_offset);
    } else if (strcmp(attr->attr.name,
        __stringify(asic_id)) == 0) {
        set_val = 1;
        snprintf(str, str_len, "0x%04X", priv->cur_asic_id);
    } else if (strcmp(attr->attr.name,
        __stringify(video_modes)) == 0) {
        const struct eys3d_video_mode *mode = NULL;
        int offset = 0;
        char *s = str;
        int i = 0;
        while(1) {
            mode = eys3d_api_get_video_mode_by_index(driv, i);
            if (!mode)
                break;
            offset  = snprintf(s, str_len, "%u:%u:%u:%u:%u:%d:%d:0x%08X:%d:%d|", mode->mode_index,
                               mode->width, mode->height, mode->fps,
                               mode->depth_bits, (int)mode->is_rectify,
                               (int)mode->is_interleave_mode, mode->pixelcode, mode->is_scale_down, mode->color_bits);
            i++;
            s += offset;
        }
        set_val = 1;
    } else if (strcmp(attr->attr.name,
        __stringify(is_opened)) == 0) {
        set_val = 1;
        snprintf(str, str_len, "%d", priv->is_opened);
    } else {
       /*DO nothing*/
    }

    if (is_call_get_error == false) {
        eys3d_set_error(driv->client, ret_api);
    }

    if ((set_val == 1) && (buf != NULL))
        ret_size = sprintf(buf, "%s\n", str);
    else
        ret_size = 0;

    if (!str) {
        kfree(str);
        str = NULL;
    }

    return ret_size;
}

static ssize_t eys3d_fs_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long val = 0;
    int ret = EYS3D_API_RET_OK;
    bool is_call_get_error = false;
    uint8_t reg_8 = 0;
    uint16_t reg_16 = 0;
    uint8_t val_8 = 0;
    uint16_t val_16 = 0;
    char *param_strs[10] = {0};
    size_t ret_count = count;
    int ret_api = EYS3D_API_RET_OK;
    struct i2c_client* client = NULL;
    struct v4l2_subdev *sd = NULL;
    struct eys3d *driv = NULL;
    struct eys3d_api_data *priv = NULL;

    (void)driv;
    (void)priv;
    (void)reg_8;
    (void)reg_16;
    (void)val_8;
    (void)val_16;

    if (!dev)
        goto exit;

    client = to_i2c_client(dev);
    if (!client)
        goto exit;

    sd = i2c_get_clientdata(client);
    if (!sd)
        goto exit;

    driv = sd_to_eys3d(sd);
    if (!driv)
        goto exit;

    priv = driv->data;
    if (!priv)
        goto exit;

    if ((!attr) || (!buf))
        goto exit;

    if (strcmp(attr->attr.name,
        __stringify(video_mode)) == 0) {
        ret = kstrtoul(buf, 10, (unsigned long *)&val);
        EYS3D_ALERT_DBG_PRT("video_mode[%ld][%s]\n", val, buf);
        ret_api = eys3d_set_current_vid_mode(driv->client, val);
    } else if (strcmp(attr->attr.name,
        __stringify(mipi_out_en)) == 0) {
        ret = kstrtoul(buf, 10, (unsigned long *)&val);
        ret_api = eys3d_enable_mipi_output(driv->client, (bool)val);
    } else if (strcmp(attr->attr.name,
        __stringify(read_asic_reg)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_asic_reg = (uint16_t)(val & 0xFFFF);
        //EYS3D_API_DBG_PRT("priv->cur_asic_reg [0x%04X][%s]\n", priv->cur_asic_reg, buf);
        priv->is_rd_asic_reg = true;
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(read_fw_reg)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_fw_reg = (uint8_t)(val & 0xFF);
        //EYS3D_API_DBG_PRT("priv->cur_fw_reg [0x%02X][%s]\n", priv->cur_fw_reg, buf);
        priv->is_rd_fw_reg = true;
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(write_fw_reg)) == 0) {
        ret = eys3d_parse_cmd_cfg(buf, param_strs, 2, " ");
        if (ret == EYS3D_API_RET_OK) {
            if (param_strs[0] && param_strs[1]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                reg_8 = (uint8_t)(val & 0xFF);
                
                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                val_8 = (uint8_t)(val & 0xFF);
                
                //EYS3D_API_DBG_PRT("reg 0x%02X\n", reg_8);
                //EYS3D_API_DBG_PRT("val 0x%02X\n", val_8);
                ret_api = eys3d_wr_fw_reg(driv->client, reg_8, val_8);
            }
        }
    } else if (strcmp(attr->attr.name,
        __stringify(write_asic_reg)) == 0) {
        ret = eys3d_parse_cmd_cfg(buf, param_strs, 2, " ");
        if (ret == EYS3D_API_RET_OK) {
            if (param_strs[0] && param_strs[1]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                reg_16 = (uint16_t)(val & 0xFFFF);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                val_8 = (uint8_t)(val & 0xFF);

                //EYS3D_API_DBG_PRT("reg 0x%04X\n", reg_16);
                //EYS3D_API_DBG_PRT("val 0x%02X\n", val_8);
                ret_api = eys3d_wr_asic_reg(driv->client, reg_16, &val_8, 1);
            }
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_i2_reg)) == 0) {
        //echo  "0x80 0x20 0x000a" > /sys/bus/i2c/devices/2-0060/read_i2_reg
        ret = eys3d_parse_cmd_cfg(buf, param_strs, 3, " ");
        if (ret == EYS3D_API_RET_OK) {
            ENTER_CS
            if (param_strs[0] && param_strs[1] && param_strs[2]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                priv->cur_i2_slave_id = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                priv->cur_i2_format = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[2], 16, (unsigned long *)&val);
                priv->cur_i2_addr = (uint16_t)(val & 0xFFFF);

                priv->is_rd_i2c_reg = true;

                //EYS3D_API_DBG_PRT("priv->cur_i2_slave_id 0x%02X\n", priv->cur_i2_slave_id);
                //EYS3D_API_DBG_PRT("priv->cur_i2_format 0x%02X\n", priv->cur_i2_format);
                //EYS3D_API_DBG_PRT("priv->cur_i2_addr 0x%02X\n", priv->cur_i2_addr);
            }
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(write_i2_reg)) == 0) {
        //echo  "0x80 0x00 0x000c 0x01" > /sys/bus/i2c/devices/2-0060/write_i2_reg
        ret = eys3d_parse_cmd_cfg(buf, param_strs, 4, " ");
        if (ret == EYS3D_API_RET_OK) {
            uint8_t id = 0;
            uint8_t format = 0;
            uint16_t addr = 0;
            uint16_t data = 0;
            if (param_strs[0] && param_strs[1] && param_strs[2] && param_strs[3]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                id = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                format = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[2], 16, (unsigned long *)&val);
                addr = (uint16_t)(val & 0xFFFF);

                ret = kstrtoul(param_strs[3], 16, (unsigned long *)&val);
                data = (uint16_t)(val & 0xFFFF);

                ret_api = eys3d_wr_fw_i2c_reg(driv->client, id, format, addr, data);
            }
        }
    } else if (strcmp(attr->attr.name,
        __stringify(write_ct_pu_value)) == 0) {
       /*
        * 1. echo "[id] [type] [ctrsel] [val]" > /sys/bus/i2c/devices/2-0060/write_ct_pu_value
        * 2. For CT(1)/CAMERA_HW_SET_CUR(0x01)/CAMERA_HW_CT_AE_MODE_CONTROL(0x02)/CAMERA_HW_EXPOSURE_SHUTTER_PRIORITY(0x2)
        * echo  "0x01 0x81 0x02 0x00000002" > /sys/bus/i2c/devices/2-0060/write_ct_pu_value
        */
        ret = eys3d_parse_cmd_cfg(buf, param_strs, 4, " ");
        if (ret == EYS3D_API_RET_OK) {
            uint8_t pb_ct_pu_id = 0;
            uint8_t pb_type = 0;
            uint8_t pb_ctrsel = 0;
            uint32_t pb_val = 0;
            if (param_strs[0] && param_strs[1] && param_strs[2] && param_strs[3]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                pb_ct_pu_id = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                pb_type = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[2], 16, (unsigned long *)&val);
                pb_ctrsel = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[3], 16, (unsigned long *)&val);
                pb_val = (uint32_t)(val & 0xFFFFFFFF);

                //EYS3D_API_DBG_PRT("pb_ct_pu_id 0x%02X\n", pb_ct_pu_id);
                //EYS3D_API_DBG_PRT("pb_type 0x%02X\n", pb_type);
                //EYS3D_API_DBG_PRT("pb_ctrsel 0x%02X\n", pb_ctrsel);
                //EYS3D_API_DBG_PRT("pb_val 0x%08X\n", pb_val);
                ret_api = eys3d_wr_prop_bar(driv->client, pb_ct_pu_id, pb_type, pb_ctrsel, pb_val);
                if (ret_api == 0) {
                    ret_api = eys3d_sync_fd(driv->client);
                }
            }
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_ct_pu_value)) == 0) {
       /*
        * 1. echo "[ct_pu_id] [type] [ctrsel]" > /sys/bus/i2c/devices/2-0060/read_ct_pu_value
        * 2. For CT(1)/CAMERA_HW_GET_CUR(0x81)/CAMERA_HW_CT_AE_MODE_CONTROL(0x02)
        * echo  "0x01 0x81 0x02" > /sys/bus/i2c/devices/2-0060/read_ct_pu_value
        * cat /sys/bus/i2c/devices/2-0060/read_ct_pu_value
        * 2. For PU(3)/CAMERA_HW_GET_CUR(0x81)/CAMERA_HW_PU_BRIGHTNESS_CONTROL(0x02)
        * echo  "0x03 0x81 0x02" > /sys/bus/i2c/devices/2-0060/read_ct_pu_value
        * cat /sys/bus/i2c/devices/2-0060/read_ct_pu_value
        */
        ret = eys3d_parse_cmd_cfg(buf, param_strs, 3, " ");
        if (ret == EYS3D_API_RET_OK) {
            ENTER_CS
            if (param_strs[0] && param_strs[1] && param_strs[2]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                priv->cur_pb_ct_pu_id = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                priv->cur_pb_type = (uint8_t)(val & 0xFF);

                ret = kstrtoul(param_strs[2], 16, (unsigned long *)&val);
                priv->cur_pb_ctrsel = (uint8_t)(val & 0xFF);

                priv->is_rd_pb = true;

                //EYS3D_API_DBG_PRT("priv->cur_pb_ct_pu_id 0x%02X\n", priv->cur_pb_ct_pu_id);
                //EYS3D_API_DBG_PRT("priv->cur_pb_type 0x%02X\n", priv->cur_pb_type);
                //EYS3D_API_DBG_PRT("priv->cur_pb_ctrsel 0x%02X\n", priv->cur_pb_ctrsel);
            }
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_ct_pu_support_list)) == 0) {
       /*
        * 1. echo "[id]" > /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        * 2. For CT(1) Support List
        * echo  "0x01" > /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        * cat /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        * 2. For PU(3) Support List
        * echo  "0x03" > /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        * cat /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        */
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_pb_ct_pu_id = (uint8_t)(val & 0xFF);
        priv->is_rd_pb = true;
        //EYS3D_API_DBG_PRT("priv->cur_pb_ct_pu_id 0x%02X\n", priv->cur_pb_ct_pu_id);
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(fs_id)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 10, (unsigned long *)&val);
        priv->cur_fs_id = (uint8_t)(val & 0xFF);
        //EYS3D_API_DBG_PRT("priv->cur_fs_id [0x%02X][%s]\n", priv->cur_fs_id, buf);
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(fs_offset)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_fs_offset = (uint32_t)(val & 0xFFFFFFFF);
        //EYS3D_API_DBG_PRT("priv->cur_fs_offset [0x%08X][%s]\n", priv->cur_fs_offset, buf);
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(asic_id)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_asic_id = (uint16_t)(val & 0xFFFF);
        //EYS3D_API_DBG_PRT("priv->cur_asic_id [0x%04X][%s]\n", priv->cur_asic_id, buf);
        LEAVE_CS
    }  else if (strcmp(attr->attr.name,
        __stringify(en_senif)) == 0) {
        ret_api = eys3d_enable_sensor_if(driv->client);
    }  else if (strcmp(attr->attr.name,
        __stringify(is_opened)) == 0) {
        ret = kstrtoul(buf, 10, (unsigned long *)&val);
        ret_api = eys3d_set_is_opened(driv->client, (bool)val);
    }  else {
       /*DO nothing*/
    }

    if (is_call_get_error == false) {
        eys3d_set_error(driv->client, ret_api);
    }
    //EYS3D_API_DBG_PRT("Leave\n");

exit:
    return ret_count;
}

static struct device_attribute fw_version_attribute = __ATTR (
    fw_version, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute video_mode_attribute = __ATTR (
    video_mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute video_modes_attribute = __ATTR (
    video_modes, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute erro_code_attribute = __ATTR (
    erro_code, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute mipi_out_en_attribute = __ATTR (
    mipi_out_en, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute stream_on_attribute = __ATTR (
    stream_on, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute read_fw_reg_attribute = __ATTR (
    read_fw_reg, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute write_fw_reg_attribute = __ATTR (
    write_fw_reg, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute read_i2_reg_attribute = __ATTR (
    read_i2_reg, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute write_i2_reg_attribute = __ATTR (
    write_i2_reg, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute read_asic_reg_attribute = __ATTR (
    read_asic_reg, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute write_asic_reg_attribute = __ATTR (
    write_asic_reg, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute read_ct_pu_value_attribute = __ATTR (
    read_ct_pu_value, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute write_ct_pu_value_attribute = __ATTR (
    write_ct_pu_value, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute read_ct_pu_support_list_attribute = __ATTR (
    read_ct_pu_support_list, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute fs_id_attribute = __ATTR (
    fs_id, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute fs_offset_attribute = __ATTR (
    fs_offset, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute asic_id_attribute = __ATTR (
    asic_id, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute en_senif_attribute = __ATTR (
    en_senif, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct device_attribute is_opened_attribute = __ATTR (
    is_opened, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, eys3d_fs_show, eys3d_fs_store);

static struct attribute *attrs[] = {
    (struct attribute *)&fw_version_attribute.attr,
    (struct attribute *)&video_mode_attribute.attr,
    (struct attribute *)&erro_code_attribute.attr,
    (struct attribute *)&stream_on_attribute.attr,
    (struct attribute *)&mipi_out_en_attribute.attr,
    (struct attribute *)&read_fw_reg_attribute.attr,
    (struct attribute *)&write_fw_reg_attribute.attr,
    (struct attribute *)&read_i2_reg_attribute.attr,
    (struct attribute *)&write_i2_reg_attribute.attr,
    (struct attribute *)&read_asic_reg_attribute.attr,
    (struct attribute *)&write_asic_reg_attribute.attr,
    (struct attribute *)&read_ct_pu_value_attribute.attr,
    (struct attribute *)&write_ct_pu_value_attribute.attr,
    (struct attribute *)&read_ct_pu_support_list_attribute.attr,
    (struct attribute *)&fs_id_attribute.attr,
    (struct attribute *)&fs_offset_attribute.attr,
    (struct attribute *)&asic_id_attribute.attr,
    (struct attribute *)&video_modes_attribute.attr,
    (struct attribute *)&en_senif_attribute.attr,
    (struct attribute *)&is_opened_attribute.attr,
    NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

static ssize_t eys3d_version_proc_wr(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos)
{
    unsigned long val = 0;
    char *tmp = NULL;
    int ret = EYS3D_API_RET_OK;

    tmp = kzalloc((count + 1), GFP_KERNEL);
    if (!tmp)
        return -ENOMEM;

    if (copy_from_user(tmp, buffer, count)) {
        kfree(tmp);
        return EFAULT;
    }

    ret = kstrtoul(tmp, 10, (unsigned long *)&val);

    mutex_lock(&g_proc_lock);
    g_is_enable_version_check = val;
    mutex_unlock(&g_proc_lock);

    if (tmp) {
        kfree(tmp);
        tmp = NULL;
    }

    return count;
}

static int eys3d_version_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d:%s:%s", (int)g_is_enable_version_check, EYS3D_SDK_VERSION, EYS3D_API_VERSION);
    return EYS3D_API_RET_OK;
}

static int eys3d_version_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, eys3d_version_proc_show, PDE_DATA(inode));
}

//NOTE: How to pass the priv to eys3d_i2c_proc_show:https://blog.csdn.net/wbd880419/article/details/51955805
static int eys3d_i2c_proc_show(struct seq_file *file, void *v)
{
    struct i2c_client* client = NULL;

    client = (struct i2c_client *)file->private;

    //EYS3D_API_DBG_PRT("Client [0x%p]\n", client);

    if (client)
        seq_printf(file, "%d-%04X", client->adapter->nr, client->addr);

    return EYS3D_API_RET_OK;
}

static int eys3d_i2c_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, eys3d_i2c_proc_show, PDE_DATA(inode));
}

/* For NVIDIA TX2 */
#if LINUX_VERSION_CODE == KERNEL_VERSION(4,9,140)
static const struct file_operations eys3d_i2c_proc_ops = {
    .open = eys3d_i2c_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static const struct file_operations eys3d_version_proc_ops = {
    .open = eys3d_version_proc_open,
    .write = eys3d_version_proc_wr,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#else
static const struct proc_ops eys3d_i2c_proc_ops = {
    .proc_open = eys3d_i2c_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops eys3d_version_proc_ops = {
    .proc_open = eys3d_version_proc_open,
    .proc_write = eys3d_version_proc_wr,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#endif

static int eys3d_proc_init(struct i2c_client* client)
{
    int ret = EYS3D_API_RET_OK;
    char prc_path[64] = {0};

    if (g_proc_created_count >= EYS3D_API_SP_DEV_CNT) {
        ret = -EINVAL;
        goto exit;
    }

    mutex_lock(&g_proc_lock);

    if (g_proc_created_count == 0) {
        snprintf(prc_path, sizeof(prc_path), "%s", EYS3D_API_I2C_INFO_PROCFS_NODE);
    } else {
        snprintf(prc_path, sizeof(prc_path), "%s_%d", EYS3D_API_I2C_INFO_PROCFS_NODE, g_proc_created_count);
    }

    g_proc_i2c[g_proc_created_count] = proc_create_data(prc_path, S_IRWXU | S_IRWXG | S_IRWXO, NULL, &eys3d_i2c_proc_ops, (void *)client);
    if (!g_proc_i2c[g_proc_created_count]) {
        ret = -EINVAL;
        goto exit;
    }

    if (g_proc_created_count == 0) {
        g_proc_version = proc_create_data(EYS3D_API_VERSION_INFO_PROCFS_NODE, S_IRWXU | S_IRWXG | S_IRWXO, NULL, &eys3d_version_proc_ops, (void *)client);
        if (!g_proc_version) {
            ret = -EINVAL;
            goto exit;
        }
    }

    g_proc_created_count++;

exit:
    mutex_unlock(&g_proc_lock);

    return ret;
}

static void eys3d_proc_rls(struct i2c_client* client)
{
    int i = 0;

    (void)client;

    for (i = 0; i < EYS3D_API_SP_DEV_CNT; i++) {
        if (g_proc_i2c[i]) {
            proc_remove(g_proc_i2c[i]);
            g_proc_i2c[i] = NULL;
        }
    }

    if (g_proc_version) {
        proc_remove(g_proc_version);
        g_proc_version = NULL;
    }

    g_proc_created_count = 0;
}

static void eys3d_assign_vc_i2c_client(struct eys3d *driv, struct eys3d_api_data *priv)
{
    eys3d_proc_init(driv->client);
    priv->vc_id = driv->client->addr & ~EYS3D_API_VC_0_I2C_ADDR;
    priv->client = g_eys3d_i2c_client;
    
    EYS3D_API_DBG_PRT("VC[%d] Addr 0x%02X 0x%p[0x%p]\n", priv->vc_id, driv->client->addr, driv->client, priv->client);
}

static int eys3d_sysfs_init(struct kobject *kobj)
{
    int ret = EYS3D_API_RET_OK;

    if (kobj != NULL)
        ret = sysfs_create_group(kobj, &attr_group);

    return ret;
}

static int eys3d_sysfs_rls(struct kobject *kobj)
{
    if (!kobj)
        return -EINVAL;

    sysfs_remove_group(kobj, &attr_group);

    return EYS3D_API_RET_OK;
}

static ssize_t eys3d_sysfs_bin_rd(struct file *filp, struct kobject *kobj,
        struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
    struct sys_bin_data *bin = NULL;
    unsigned int remain = 0;
    unsigned int num = 0;
    unsigned char *ptr = NULL;
    int i = 0;
    int ret = EYS3D_API_RET_OK;
    size_t ret_count = 0;
    struct i2c_client* client = kobj_to_i2c_client(kobj);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;
    (void)remain;
    (void)num;
    (void)i;
    (void)ret;

    EYS3D_API_DBG_PRT("driv[0x%p] sd[0x%p] client[0x%p] priv[0x%p]\n", driv, sd, client, priv);

    if (!attr)
        return -EINVAL;

    if (strncmp(attr->attr.name, EYS3D_API_FS_BIN_FILE_NAME,
        strlen(EYS3D_API_FS_BIN_FILE_NAME)) == 0) {
        bin = priv->fs_bin;
    } else if (strncmp(attr->attr.name, EYS3D_API_FLASH_BIN_FILE_NAME,
        strlen(EYS3D_API_FLASH_BIN_FILE_NAME)) == 0) {
        bin = priv->flash_bin;
    } else if (strncmp(attr->attr.name, EYS3D_API_ASIC_REG_BIN_FILE_NAME,
        strlen(EYS3D_API_ASIC_REG_BIN_FILE_NAME)) == 0) {
        bin = priv->asic_reg_bin;
    }

    if (!bin)
        return -EINVAL;

    //EYS3D_API_DBG_PRT("%s\n", attr->attr.name);

    mutex_lock(&(bin->buffer_lock));
    ret_count = count;
    ptr = &bin->buffer[off];
    memset(ptr, 0x0, count);
    if (strncmp(attr->attr.name, EYS3D_API_FS_BIN_FILE_NAME,
        strlen(EYS3D_API_FS_BIN_FILE_NAME)) == 0) {
        ret = eys3d_rd_fw_data(driv->client, EYS3D_API_IO_REQ_MEM_TP_FS_TABLE, priv->cur_fs_id, 0, ptr, count);
        //EYS3D_API_DBG_PRT("[%d, %d, %d][%d]\n", (int)count, (int)off, (int)priv->cur_fs_id, ret);
    } else if (strncmp(attr->attr.name, EYS3D_API_FLASH_BIN_FILE_NAME,
        strlen(EYS3D_API_FS_BIN_FILE_NAME)) == 0) {
        ret = eys3d_rd_fw_data(driv->client, EYS3D_API_IO_REQ_MEM_TP_SPI_FLASH, 0, priv->cur_fs_offset, ptr, count);
        EYS3D_API_DBG_PRT("[%d, %d, 0x%08X][%d]\n", (int)count, (int)off, (int)priv->cur_fs_offset, ret);
    } else if (strncmp(attr->attr.name, EYS3D_API_ASIC_REG_BIN_FILE_NAME,
        strlen(EYS3D_API_ASIC_REG_BIN_FILE_NAME)) == 0) {
        ret = eys3d_rd_asic_reg(driv->client, priv->cur_asic_id, ptr, count);
        //EYS3D_API_DBG_PRT("[%d, %d, 0x%04X][%d]\n", (int)count, (int)off, priv->cur_asic_id, ret);
    }

    if (ret != EYS3D_API_RET_OK) {
        ret_count = 0;
        goto exit;
    }

    memcpy(buf, &bin->buffer[off], count);

exit:
    mutex_unlock(&(bin->buffer_lock));
    eys3d_set_error(driv->client, ret);

    return ret_count;
}

static ssize_t eys3d_sysfs_bin_wr(struct file *filp, struct kobject *kobj,
        struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
    struct sys_bin_data *bin = NULL;
    unsigned int remain = 0;
    unsigned int num = 0;
    unsigned char *ptr = NULL;
    int i = 0;
    int ret = EYS3D_API_RET_OK;
    size_t ret_count = 0;
    struct i2c_client* client = kobj_to_i2c_client(kobj);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;
    (void)remain;
    (void)num;
    (void)i;
    (void)ret;

    EYS3D_API_DBG_PRT("driv[0x%p] sd[0x%p] client[0x%p] priv[0x%p]\n", driv, sd, client, priv);

    if (!attr)
        return -EINVAL;

    if (strncmp(attr->attr.name, EYS3D_API_FS_BIN_FILE_NAME,
        strlen(EYS3D_API_FS_BIN_FILE_NAME)) == 0) {
        bin = priv->fs_bin;
    } else if (strncmp(attr->attr.name, EYS3D_API_FLASH_BIN_FILE_NAME,
        strlen(EYS3D_API_FLASH_BIN_FILE_NAME)) == 0) {
        bin = priv->flash_bin;
    } else if (strncmp(attr->attr.name, EYS3D_API_ASIC_REG_BIN_FILE_NAME,
        strlen(EYS3D_API_ASIC_REG_BIN_FILE_NAME)) == 0) {
        bin = priv->asic_reg_bin;
    }

    if (!bin)
        return -EINVAL;

    //EYS3D_API_DBG_PRT("%s\n", attr->attr.name);

    mutex_lock(&(bin->buffer_lock));
    memcpy(&bin->buffer[off], buf, count);
    ret_count = count;
    ptr = &bin->buffer[off];
    if (strncmp(attr->attr.name, EYS3D_API_FS_BIN_FILE_NAME,
        strlen(EYS3D_API_FS_BIN_FILE_NAME)) == 0) {
        ret = eys3d_wr_fw_data(driv->client, EYS3D_API_IO_REQ_MEM_TP_FS_TABLE, priv->cur_fs_id, 0, ptr, count);
        //EYS3D_API_DBG_PRT("[%d, %d, %d][%d]\n", (int)count, (int)off, (int)priv->cur_fs_id, ret);
    } else if (strncmp(attr->attr.name, EYS3D_API_FLASH_BIN_FILE_NAME,
        strlen(EYS3D_API_FLASH_BIN_FILE_NAME)) == 0) {
        ret = eys3d_wr_fw_data(driv->client, EYS3D_API_IO_REQ_MEM_TP_SPI_FLASH, 0, priv->cur_fs_offset, ptr, count);
        EYS3D_API_DBG_PRT("[%d, %d, 0x%08X][%d]\n", (int)count, (int)off, (int)priv->cur_fs_offset, ret);
    } else if (strncmp(attr->attr.name, EYS3D_API_ASIC_REG_BIN_FILE_NAME,
        strlen(EYS3D_API_ASIC_REG_BIN_FILE_NAME)) == 0) {
        ret = eys3d_wr_asic_reg(driv->client, priv->cur_asic_id, ptr, count);
        //EYS3D_API_DBG_PRT("[%d, %d, 0x%04X][%d]\n", (int)count, (int)off, priv->cur_asic_id, ret);
    }

    if (ret != EYS3D_API_RET_OK) {
        ret_count = 0;
        goto exit;
    }

exit:
    mutex_unlock(&(bin->buffer_lock));
    eys3d_set_error(driv->client, ret);

    return ret_count;
}

static int eys3d_sysfs_bin_init(struct kobject *kobj)
{
    struct sys_bin_data *fs_bin = NULL;
    struct sys_bin_data *flash_bin = NULL;
    struct sys_bin_data *asic_bin = NULL;
    int ret = EYS3D_API_RET_OK;
    struct i2c_client* client = kobj_to_i2c_client(kobj);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    fs_bin = kzalloc(sizeof(struct sys_bin_data), GFP_KERNEL);

    if (!fs_bin)
        return -ENOMEM;

    priv->fs_bin = fs_bin;

    mutex_init(&(fs_bin->buffer_lock));
    sysfs_bin_attr_init(&fs_bin->bin);
    fs_bin->bin.attr.name = EYS3D_API_FS_BIN_FILE_NAME;
    fs_bin->bin.attr.mode = S_IRUSR | S_IWUSR;
    fs_bin->bin.read = eys3d_sysfs_bin_rd;
    fs_bin->bin.write = eys3d_sysfs_bin_wr;
    fs_bin->bin.size = EYS3D_API_FS_BIN_BUF_SZ;

    ret = sysfs_create_bin_file(kobj, &fs_bin->bin);

    flash_bin = kzalloc(sizeof(struct sys_bin_data), GFP_KERNEL);

    if (!flash_bin)
        return -ENOMEM;

    priv->flash_bin = flash_bin;

    mutex_init(&(flash_bin->buffer_lock));
    sysfs_bin_attr_init(&flash_bin->bin);
    flash_bin->bin.attr.name = EYS3D_API_FLASH_BIN_FILE_NAME;
    flash_bin->bin.attr.mode = S_IRUSR | S_IWUSR;
    flash_bin->bin.read = eys3d_sysfs_bin_rd;
    flash_bin->bin.write = eys3d_sysfs_bin_wr;
    flash_bin->bin.size = EYS3D_API_FS_BIN_BUF_SZ;

    ret = sysfs_create_bin_file(kobj, &flash_bin->bin);

    asic_bin = kzalloc(sizeof(struct sys_bin_data), GFP_KERNEL);

    if (!asic_bin)
        return -ENOMEM;

    priv->asic_reg_bin = asic_bin;

    mutex_init(&(asic_bin->buffer_lock));
    sysfs_bin_attr_init(&fs_bin->bin);
    asic_bin->bin.attr.name = EYS3D_API_ASIC_REG_BIN_FILE_NAME;
    asic_bin->bin.attr.mode = S_IRUSR | S_IWUSR;
    asic_bin->bin.read = eys3d_sysfs_bin_rd;
    asic_bin->bin.write = eys3d_sysfs_bin_wr;
    asic_bin->bin.size = EYS3D_API_FS_BIN_BUF_SZ;

    ret = sysfs_create_bin_file(kobj, &asic_bin->bin);

    return ret;
}

static int eys3d_sysfs_bin_rls(struct kobject *kobj)
{
    int ret = EYS3D_API_RET_OK;
    struct i2c_client* client = kobj_to_i2c_client(kobj);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct eys3d *driv = sd_to_eys3d(sd);
    struct eys3d_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    if (priv->fs_bin) {
        mutex_destroy(&(priv->fs_bin->buffer_lock));
        kfree(priv->fs_bin);
    }

    if (priv->flash_bin) {
        mutex_destroy(&(priv->flash_bin->buffer_lock));
        kfree(priv->flash_bin);
    }

    if (priv->asic_reg_bin) {
        mutex_destroy(&(priv->asic_reg_bin->buffer_lock));
        kfree(priv->asic_reg_bin);
    }

    return ret;
}

int eys3d_api_start_stream(struct eys3d *driv)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = eys3d_esp876_start_stream(driv);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = eys3d_p2c_start_stream(driv);
#endif

    return EYS3D_API_RET_OK;
}

int eys3d_api_stop_stream(struct eys3d *driv)
{
    int ret = EYS3D_API_RET_OK;

#if EYS3D_CAMERA_MODULE_IS_ESP876
    ret = eys3d_esp876_stop_stream(driv);
#elif EYS3D_CAMERA_MODULE_IS_P2C
    ret = eys3d_p2c_stop_stream(driv);
#endif
    
    return ret;
}

const struct eys3d_video_mode *eys3d_api_get_current_video_mode(struct eys3d *driv)
{
    uint32_t size = 0;
    struct eys3d_api_data *priv = driv->data;

    size = priv->supported_video_modes_num;
    if (size == 0 || !priv->supported_video_modes) {
        EYS3D_ERR_DBG_PRT("Get video mode err[%d]\n", priv->cur_video_mode_index);
        return NULL;
    }

    if (priv->is_initialed == false) {
        EYS3D_ERR_DBG_PRT("Device is NOT initialized[%d]\n", priv->is_initialed);
        return NULL;
    }

    if (priv->cur_video_mode_index >= size) {
        EYS3D_ERR_DBG_PRT("Get size of video mode err[%d]\n", priv->cur_video_mode_index);
        return NULL;
    } else {
        EYS3D_API_DBG_PRT("cur_video_mode_index[%d]\n", priv->cur_video_mode_index);
        return &priv->supported_video_modes[priv->cur_video_mode_index];
    }
}

const struct eys3d_video_mode *eys3d_api_get_video_mode_by_index(struct eys3d *driv, uint32_t index)
{
    uint32_t size = 0;
    struct eys3d_api_data *priv = driv->data;

    size = priv->supported_video_modes_num;
    if (size == 0 || !priv->supported_video_modes) {
        EYS3D_ERR_DBG_PRT("Get video mode err[%d]\n", priv->cur_video_mode_index);
        return NULL;
    }

    if (priv->is_initialed == false) {
        EYS3D_ERR_DBG_PRT("Device is NOT initialized[%d]\n", priv->is_initialed);
        return NULL;
    }

    if (index >= size) {
        EYS3D_ERR_DBG_PRT("Get index of video mode err[%d]\n", index);
        return NULL;
    } else {
        EYS3D_API_DBG_PRT("video_mode_index[%d]\n", index);
        return &priv->supported_video_modes[index];
    }
}

int eys3d_api_eys3d_set_current_vid_mode(struct eys3d *driv, uint32_t width, uint32_t height)
{
    int i = 0;
    struct eys3d_api_data *priv = driv->data;
    const struct eys3d_video_mode *c = &priv->supported_video_modes[priv->cur_video_mode_index];
    const struct eys3d_video_mode *m = NULL;
    int ret = -EPERM;
    bool got = false;

    for (i = 0; i < priv->supported_video_modes_num; i++) {
        m = &priv->supported_video_modes[i];
        if (m) {
            if ((m->depth_bits == c->depth_bits) && (m->color_bits == c->color_bits) && (m->pixelcode == c->pixelcode)) {
                got = true;
            } else if ((m->color_bits == c->color_bits) && (m->pixelcode == c->pixelcode)) {
                got = true;
            }
            if ((m->width == width) && (m->height == height) && got) {
                priv->cur_video_mode_index = m->mode_index;
                ret = EYS3D_API_RET_OK;
                break;
            }
        }
    }

    return ret;
}

int eys3d_api_init(struct eys3d *driv)
{
    int ret = EYS3D_API_RET_OK;
    int ret_line = 0;
    struct device *dev = NULL;
    struct eys3d_api_data *priv = NULL;

    EYS3D_API_DBG_PRT("Enter\n");

    if (!driv) {
        EYS3D_ERR_DBG_PRT("Driv is NULL\n");
        ret = -ENXIO;
        ret_line = __LINE__;
        goto exit;
    }

    dev = &(driv->client->dev);
    priv = devm_kzalloc(dev, sizeof(struct eys3d_api_data), GFP_KERNEL);
    if (!priv) {
        ret = -ENOMEM;
        ret_line = __LINE__;
        goto exit;
    }

    priv->camera_pid = EYS3D_API_CAMERA_PID_UNKOWN;
    priv->camera_vid = EYS3D_API_CAMERA_VID_UNKOWN;
    priv->mipi_out_en = false;
    priv->stream_on = false;

    /* Get private mutex of VC#X from the global mutex */
    priv->eys3d_api_mutex = &g_eys3d_api_mutex;

    priv->cur_error_code = 0x0;

    priv->is_rd_asic_reg = false;
    priv->cur_asic_reg = 0xFFFF;

    priv->is_rd_fw_reg = false;
    priv->cur_fw_reg = 0xFF;

    priv->is_rd_pb = false;
    priv->cur_pb_ct_pu_id = 0xFF;
    priv->cur_pb_type = 0xFF;
    priv->cur_pb_ctrsel = 0xFF;

    priv->cur_fs_id = 0;
    priv->cur_fs_offset = 0;
    priv->cur_asic_id = 0;

    priv->is_rd_i2c_reg = false;
    priv->cur_i2_slave_id = 0xFF;
    priv->cur_i2_format = 0xFF;
    priv->cur_i2_addr = 0xFFFF;

    priv->cur_video_mode_index = 0x1FFFFFFf;
    priv->pre_video_mode_index = 0x1FFFFFFF;
    priv->is_first_set_video_mode = true;

    priv->fs_bin = NULL;
    priv->flash_bin = NULL;
    priv->asic_reg_bin = NULL;

    priv->is_opened = false;

    priv->vc_id = 0;

    driv->data = priv;
    priv->client = NULL;

    (void)&eys3d_get_pid_vid;
    (void)&eys3d_get_fw_ver;
    (void)&eys3d_enable_mipi_output;

    (void)&eys3d_video_switch;
    (void)&eys3d_video_close;

    (void)&eys3d_wr_fw_reg;
    (void)&eys3d_rd_fw_reg;
    
    (void)&eys3d_rd_asic_reg;
    (void)&eys3d_wr_asic_reg;

    (void)&eys3d_rd_fd;
    (void)&eys3d_wr_fd;
    (void)&eys3d_sync_fd;

    (void)&eys3d_rd_fw_i2c_reg;
    (void)&eys3d_wr_fw_i2c_reg;

    (void)&eys3d_rd_prop_bar;
    (void)&eys3d_wr_prop_bar;
    (void)&eys3d_rd_prop_bar_sp_list;

    (void)&eys3d_proc_init;
    (void)&eys3d_sysfs_init;
    (void)&eys3d_sysfs_bin_init;

    EYS3D_ALERT_DBG_PRT("Version [%s]\n", EYS3D_API_VERSION);
#if EYS3D_API_IGNORE_I2C_KEEP_MIPI
    priv->camera_pid = EYS3D_API_CAMERA_PID_GRAPE;
    priv->camera_vid = EYS3D_API_CAMERA_VID_GRAPE;
    priv->cur_video_mode_index = 0;

    EYS3D_ALERT_DBG_PRT("IGNORE_I2C_KEEP_MIPI[0x%04X, 0x%04X]\n", priv->camera_vid, priv->camera_pid);
    EYS3D_API_DBG_PRT("The default video mode index[%u]\n", priv->cur_video_mode_index);

    SUPPORTED_VIDEO_MODE(priv->camera_pid, priv->supported_video_modes, &priv->supported_video_modes_num);
    EYS3D_API_DBG_PRT("supported_video_modes_num[%u]\n", priv->supported_video_modes_num);
#else
    if (!driv->client) {
        EYS3D_ERR_DBG_PRT("I2C client is NULL\n");
        ret = -ENXIO;
        ret_line = __LINE__;
        goto exit;
    }

    EYS3D_API_DBG_PRT("I2C Name [%s]\n", driv->client->name);
    EYS3D_API_DBG_PRT("I2C Bus  [%d]\n", driv->client->adapter->nr);
    EYS3D_API_DBG_PRT("I2C Addr [0x%02X]\n", driv->client->addr);

    /* Store VC#0 I2C client into global variable */
    if (driv->client->addr == EYS3D_API_VC_0_I2C_ADDR)
        g_eys3d_i2c_client = driv->client;

    /* Store I2C client from VC#0 into private variable */
    eys3d_assign_vc_i2c_client(driv, priv);

    /* Enable the MIPI output */
    if (eys3d_enable_mipi_output(driv->client, true) != EYS3D_API_RET_OK) {
        EYS3D_API_DBG_PRT("Failed to enable MIPI output\n");
    } else {
        uint16_t pid = 0;
        uint16_t vid = 0;

        if (eys3d_get_pid_vid(driv->client, &pid, &vid) != EYS3D_API_RET_OK) {
            EYS3D_API_DBG_PRT("Failed to get vid/pid\n");
        } else {
            priv->camera_pid = pid;
            priv->camera_vid = vid;
            EYS3D_API_DBG_PRT("[0x%04X, 0x%04X]\n", priv->camera_vid, priv->camera_pid);
        }

        if (priv->camera_pid == EYS3D_API_CAMERA_PID_NORA) {
            priv->cur_video_mode_index = 11;
        } else if (priv->camera_pid == EYS3D_API_CAMERA_PID_EX8036) {
            priv->cur_video_mode_index = 0;
        } else if (priv->camera_pid == EYS3D_API_CAMERA_PID_GRAPE) {
            priv->cur_video_mode_index = 0;
        } else {
            priv->cur_video_mode_index = 0;
        }
        EYS3D_API_DBG_PRT("The default video mode index[%u]\n", priv->cur_video_mode_index);

        SUPPORTED_VIDEO_MODE(priv->camera_pid, priv->supported_video_modes, &priv->supported_video_modes_num);
        EYS3D_API_DBG_PRT("supported_video_modes_num[%u]\n", priv->supported_video_modes_num);
        if (priv->supported_video_modes != NULL) {
            eys3d_sysfs_init(&(dev->kobj));
            eys3d_sysfs_bin_init(&(dev->kobj));
        } else {
            EYS3D_API_DBG_PRT("Unable to determine supported video modes\n");
        }
    }
#endif
    if (ret == EYS3D_API_RET_OK) {
        priv->is_initialed = true;
    }

exit:
    EYS3D_API_DBG_PRT("Leave[%d, %d]\n", ret, ret_line);

    return ret;
}

int eys3d_api_rls(struct eys3d *driv)
{
    int ret = EYS3D_API_RET_OK;
    struct device *dev = NULL;
    struct eys3d_api_data *priv = driv->data;

    //mutex_destroy(priv->eys3d_api_mutex);

    dev = &(driv->client->dev);

    eys3d_proc_rls(driv->client);
    eys3d_sysfs_rls(&(dev->kobj));
    eys3d_sysfs_bin_rls(&(dev->kobj));

    priv->is_initialed = false;

    if (priv)
        devm_kfree(dev, priv);

    return ret;
}
