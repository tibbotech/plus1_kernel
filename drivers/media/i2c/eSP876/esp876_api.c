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
#include "esp876_api.h"
#include "esp876_video_modes.c"

#define _APN_REV1_9_20220114_       1
#define SLEEP_PARM                  1 //ms
#define I2C_READ_TRY_MAX_COUNT      2000 //timeout = (1 * 2000 ) ms = 2 sec
#define RETRY_NEW_COMMAND_MAX_COUNT 10
#define MAX_FS_BUF_SIZE             32
#define MAX_ASIC_REG_BUF_SIZE       32
#define MAX_ASIC_REG_WR_LEN         128

#define ENTER_CS do { \
   if (priv->is_initialed) mutex_lock(&priv->mutex); \
} while(0);

#define LEAVE_CS do { \
   if (priv->is_initialed) mutex_unlock(&priv->mutex); \
} while(0);

/* private functions */
static int esp876_read_fw_version(struct i2c_client *client, char *version);
static int esp876_video_switch(struct i2c_client *client, uint32_t mode_index);
static int esp876_video_close(struct i2c_client *client);

/*
 * The following ctrl registers could be written and read:
 *
 * (1) 0xF1 (video path [0x00(USB out), 0x10(MIPI TX out)]): 0x00 (default value)
 * (2) 0xE0 (IR current value): 0x00 (default value)
 * (3) 0xE1 (IR min value [reserved]): 0x00 (default value)
 * (4) 0xE2 (IR max value [0..15]): 0x06 (default value)
 * (5) 0xED (Interleaved mode [0:OFF, 1:ON]): 0x00 (default value)
 *
 * The following ctrl registers could be read-only:
 *
 * (1) 0x00 : FW Major Version
 * (2) 0x01 : FW Minor Version
 * (3) 0x02 : FW Revision Version (High Byte. Only one byte in Bootloader)
 * (4) 0x03 : FW Revision Version (Low Byte. Only one byte in Bootloader)
 * (5) 0x04 : FW Parser Level
 * (6) 0x05 : ARC Version
 * (7) 0x06 : Project Number (High Byte)
 * (8) 0x07 : Project Number (Low Byte)
 * (9) 0x08 : FW Special Version (Bootloader = 0xff)
 */
static int esp876_write_fw_reg(struct i2c_client *client, uint8_t fw_reg, uint8_t fw_val);
static int esp876_read_fw_reg(struct i2c_client *client, uint8_t fw_reg, uint8_t *val);

static int esp876_read_asic_reg(struct i2c_client *client, uint16_t reg, uint8_t *data, uint16_t len);
static int esp876_write_asic_reg(struct i2c_client *client, uint16_t reg, uint8_t *data, uint16_t len);

static int esp876_read_fd(struct i2c_client *client, uint8_t memType, uint8_t Id, uint32_t Offset, uint8_t *Data, uint16_t Len);
static int esp876_write_fd(struct i2c_client *client, uint8_t memType, uint8_t Id, uint32_t Offset, uint8_t *Data, uint16_t Len);
static int esp876_sync_fd(struct i2c_client *client);

static int esp876_read_fw_data(struct i2c_client *client, uint8_t memType, uint8_t Id, uint32_t Offset, uint8_t* Data, uint16_t readLen);
static int esp876_write_fw_data(struct i2c_client *client, uint8_t memType, uint8_t Id, uint32_t Offset, uint8_t* Data, uint16_t writeLen);

static int esp876_write_i2c_reg(struct i2c_client *client, uint8_t slave_id, uint8_t format, uint16_t addr, uint16_t data);
static int esp876_read_i2c_reg(struct i2c_client *client, uint8_t slave_id, uint8_t format, uint16_t addr, uint16_t *data);

/*
 * ct_pu_id : 0x01: CT, 0x03: PU
 * type: same as UVC
 * ctrsel: same as UVC
 */
static int esp876_write_property_bar(struct i2c_client *client, uint8_t ct_pu_id, uint8_t type, uint8_t ctrsel, uint32_t val);
static int esp876_read_property_bar(struct i2c_client *client, uint8_t ct_pu_id, uint8_t type, uint8_t ctrsel, uint32_t *val);
static int esp876_read_property_bar_support_list(struct i2c_client *client, uint8_t ct_pu_id, uint16_t *val);

static int enable_mipi_output(struct i2c_client *client, bool en);
static int get_pid_vid(struct i2c_client *client, uint16_t *pid, uint16_t *vid);

static int my_sysfs_init(struct kobject *kobj);
static int my_sysfs_rls(struct kobject *kobj);
static int my_sysfs_bin_init(struct kobject *kobj);
static int my_sysfs_bin_rls(struct kobject *kobj);
static int my_proc_init(struct i2c_client *client);
static void my_proc_rls(struct i2c_client *client);

static int wait_reg_for_target(struct i2c_client *client, uint16_t reg, uint8_t val, uint8_t target);
static int set_current_video_mode(struct i2c_client *client, uint32_t mode_index);
static int set_error(struct i2c_client *client, int code);
static void my_sleep(unsigned int ms);

static const struct esp876_uvc_ctrl g_uvc_ct_ctrls[] = {
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

static const struct esp876_uvc_ctrl g_uvc_pu_ctrls[] = {
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

/* public functions */
int esp876_api_start_stream(struct esp876 *driv)
{
    int ret = 0;
    struct esp876_api_data *priv = driv->data;
    struct i2c_client    *client = driv->client;

    if (priv->is_initialed == false) {
        return -EINVAL;
    }

    ESP876_API_DEBUG("Is MIPI output enabled? (%d)\n", priv->mipi_out_en);
    if (priv->mipi_out_en == false) {
        ret = -EIO;
        goto exit;
    }

    if (priv->is_first_set_video_mode) {
        ret = esp876_video_switch(client, priv->cur_video_mode_index);
        if (ret != 0) {
            ESP876_ERR_DEBUG("Failed to switch video mode!\n");
            goto exit;
        }
    }

    if (priv->pre_video_mode_index != priv->cur_video_mode_index) {
        ESP876_API_DEBUG("switch video mode!(0x%08x -> 0x%08x)\n", priv->pre_video_mode_index, priv->cur_video_mode_index);
        priv->pre_video_mode_index = priv->cur_video_mode_index;
    }

    if (priv->is_first_set_video_mode) {
        ret = esp876_video_close(client);
        if (ret != 0) {
            ESP876_ERR_DEBUG("Failed to close video mode!\n");
            goto exit;
        }
    }

    ret = esp876_video_switch(client, priv->cur_video_mode_index);
    if (ret != 0) {
        ESP876_ERR_DEBUG("Failed to switch video mode!\n");
        goto exit;
    }

exit:
ENTER_CS
    if (priv->is_first_set_video_mode) {
        priv->is_first_set_video_mode = false;
    }

    if (ret == 0) {
        priv->stream_on = true;
    } else {
        priv->stream_on = false;
    }
LEAVE_CS
    ESP876_API_DEBUG("Is stream on? (%d)\n", priv->stream_on);
    /* NOTE: Never return error! It will cause the calling of 'vb2_start_streaming()' segmentation faul! */
    return 0;
}

int esp876_api_stop_stream(struct esp876 *driv)
{
    int ret = 0;
    struct esp876_api_data *priv = driv->data;
    struct i2c_client    *client = driv->client;

    if (priv->is_initialed == false) {
        return -EINVAL;
    }

    ret = esp876_video_close(client);
    if (ret != 0) {
        ESP876_ERR_DEBUG("Failed to close video mode!\n");
        goto exit;
    }

exit:
ENTER_CS
    if (ret == 0) {
        priv->stream_on = false;
    } else {
        priv->stream_on = true;
    }
LEAVE_CS
    ESP876_API_DEBUG("Is stream on? (%d)\n", priv->stream_on);
    return 0;
}

const struct esp876_video_mode *esp876_api_get_current_video_mode(struct esp876 *driv)
{
    uint32_t size = 0;
    struct esp876_api_data *priv = driv->data;

    size = priv->supported_video_modes_num;
    if (size == 0 || !priv->supported_video_modes) {
        return NULL;
    }

    if (priv->is_initialed == false) {
        return NULL;
    }

    if (priv->cur_video_mode_index >= size) {
        return NULL;
    } else {
        ESP876_API_DEBUG("cur_video_mode_index = %d\n", priv->cur_video_mode_index);
        return &priv->supported_video_modes[priv->cur_video_mode_index];
    }
}

const struct esp876_video_mode *esp876_api_get_video_mode_by_index(struct esp876 *driv, uint32_t index)
{
    uint32_t size = 0;
    struct esp876_api_data *priv = driv->data;

    size = priv->supported_video_modes_num;
    if (size == 0 || !priv->supported_video_modes) {
        return NULL;
    }

    if (priv->is_initialed == false) {
        return NULL;
    }

    if (index >= size) {
        return NULL;
    } else {
        return &priv->supported_video_modes[index];
    }
}

int esp876_api_set_current_video_mode(struct esp876 *driv, uint32_t width, uint32_t height)
{
    int i = 0;
    struct esp876_api_data *priv = driv->data;
    const struct esp876_video_mode *c = &priv->supported_video_modes[priv->cur_video_mode_index];
    const struct esp876_video_mode *m = NULL;
    int ret = -1;
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
                ret = 0;
                break;
            }
        }
    }

    return ret;
}

int esp876_api_init(struct esp876 *driv)
{
    int ret = 0;
    struct device *dev = NULL;
    struct esp876_api_data *priv = NULL;

    if (!driv) {
        ESP876_ERR_DEBUG("The 'struct esp876' driv handle is NULL!\n");
        goto exit;
    }

    dev = &(driv->client->dev);
    priv = devm_kzalloc(dev, sizeof(struct esp876_api_data), GFP_KERNEL);
    if (!priv) {
        ret = -ENOMEM;
        goto exit;
    }

    priv->camera_pid = ESP876_API_CAMERA_PID_UNKOWN;
    priv->camera_vid = ESP876_API_CAMERA_VID_UNKOWN;
    priv->mipi_out_en = false;
    priv->stream_on = false;
    mutex_init(&priv->mutex);

    priv->cur_error_code = 0x0;

    priv->is_read_asic_reg = false;
    priv->cur_asic_reg = 0xffff;

    priv->is_read_fw_reg = false;
    priv->cur_fw_reg = 0xff;

    priv->is_read_pb = false;
    priv->cur_pb_ct_pu_id = 0xff;
    priv->cur_pb_type = 0xff;
    priv->cur_pb_ctrsel = 0xff;

    priv->cur_fs_id = 0;
    priv->cur_fs_offset = 0;
    priv->cur_asic_id = 0;

    priv->is_read_i2c_reg = false;
    priv->cur_i2_slave_id = 0xff;
    priv->cur_i2_format = 0xff;
    priv->cur_i2_addr = 0xffff;

    priv->cur_video_mode_index = 0x1fffffff;
    priv->pre_video_mode_index = 0x1fffffff;
    priv->is_first_set_video_mode = true;

    priv->fs_bin = NULL;
    priv->flash_bin = NULL;
    priv->asic_reg_bin = NULL;

    priv->is_opened = false;

    driv->data = priv;

    (void)&esp876_read_asic_reg;
    (void)&esp876_write_asic_reg;
    (void)&esp876_video_close;
    (void)&esp876_read_fd;
    (void)&esp876_write_fd;
    (void)&esp876_sync_fd;

    (void)&esp876_write_i2c_reg;
    (void)&esp876_read_i2c_reg;

    (void)&esp876_write_property_bar;
    (void)&esp876_read_property_bar;
    (void)&esp876_read_property_bar_support_list;

    ESP876_ALERT_DEBUG("Version: [%s]\n", ESP876_API_VERSION);

    my_proc_init(driv->client);

    if (!driv->client) {
        ESP876_ERR_DEBUG("The 'i2_client' handle is NULL!\n");
        goto exit;
    }

    ESP876_API_DEBUG("I2C name: [%s]\n", driv->client->name);
    ESP876_API_DEBUG("I2C bus: [%d]\n", driv->client->adapter->nr);
    ESP876_API_DEBUG("I2C address: [0x%04x]\n", driv->client->addr);
    ESP876_API_DEBUG("I2C dev: 0x%p\n", dev);
    if (driv->client->addr != ESP876_API_I2C_ADDR) {
        ESP876_ERR_DEBUG("I2C address [0x%04x] is not [0x%04x]!\n", driv->client->addr, ESP876_API_I2C_ADDR);
        goto exit;
    }

    /* Enable the MIPI output */
    if (enable_mipi_output(driv->client, true) != 0) {
        ESP876_API_DEBUG("Failed to enable MIPI output!\n");
    } else {
        uint16_t pid = 0;
        uint16_t vid = 0;

        if (get_pid_vid(driv->client, &pid, &vid) != 0) {
            ESP876_API_DEBUG("Failed to get vid/pid!\n");
        } else {
            priv->camera_pid = pid;
            priv->camera_vid = vid;
            ESP876_API_DEBUG("[vid, pid] = [0x%04x, 0x%04x]\n", priv->camera_vid, priv->camera_pid);
        }

        if (priv->camera_pid == ESP876_API_CAMERA_PID_NORA) {
            priv->cur_video_mode_index = 11;//VM12/14 bits
        } else if (priv->camera_pid == ESP876_API_CAMERA_PID_EX8036) {
            priv->cur_video_mode_index = 0;
        } else {
            priv->cur_video_mode_index = 0;
        }
        ESP876_API_DEBUG("The default video mode index is as %u\n", priv->cur_video_mode_index);

        SUPPORTED_VIDEO_MODE(priv->camera_pid, priv->supported_video_modes, &priv->supported_video_modes_num);

        ESP876_API_DEBUG("supported_video_modes_num = [%u]\n", priv->supported_video_modes_num);
        ESP876_API_DEBUG("supported_video_modes: [0x%p]\n", priv->supported_video_modes);

        if (priv->supported_video_modes != NULL) {
            my_sysfs_init(&(dev->kobj));
            my_sysfs_bin_init(&(dev->kobj));
        } else {
            ESP876_API_DEBUG("Unable to determine supported video modes!\n");
        }
    }

    if (ret == 0) {
        priv->is_initialed = true;
    }

exit:
    return ret;
}

int esp876_api_rls(struct esp876 *driv)
{
    int ret = 0;
    struct device *dev = NULL;
    struct esp876_api_data *priv = driv->data;

    mutex_destroy(&priv->mutex);

    dev = &(driv->client->dev);

    my_sysfs_rls(&(dev->kobj));
    my_sysfs_bin_rls(&(dev->kobj));
    my_proc_rls(driv->client);

    priv->is_initialed = false;

    if (priv)
        devm_kfree(dev, priv);

    return ret;
}

#define I2C_REG_VALUE_08BIT        1
#define I2C_REG_VALUE_16BIT        2
#define I2C_REG_VALUE_24BIT        3
/* Write registers up to 4 at a time */
static int i2c_write_reg(struct i2c_client *client, uint16_t reg,
                 uint32_t len, uint32_t val)
{
    uint32_t buf_i, val_i;
    uint8_t buf[6];
    uint8_t *val_p;
    __be32 val_be;

    if (len > 4)
        return -EINVAL;

    buf[0] = reg >> 8;
    buf[1] = reg & 0xff;

    val_be = cpu_to_be32(val);
    val_p = (uint8_t *)&val_be;
    buf_i = 2;
    val_i = 4 - len;

    while (val_i < 4)
        buf[buf_i++] = val_p[val_i++];

    if (i2c_master_send(client, buf, len + 2) != len + 2)
        return -EIO;

    return 0;
}

/* Read registers up to 4 at a time */
static int i2c_read_reg(struct i2c_client *client, uint16_t reg,
                unsigned int len, uint32_t *val)
{
    struct i2c_msg msgs[2];
    uint8_t *data_be_p;
    __be32 data_be = 0;
    __be16 reg_addr_be = cpu_to_be16(reg);
    int ret;

    if (len > 4 || !len)
        return -EINVAL;

    data_be_p = (uint8_t *)&data_be;
    /* Write register address */
    msgs[0].addr = client->addr;
    msgs[0].flags = 0;
    msgs[0].len = 2;
    msgs[0].buf = (uint8_t *)&reg_addr_be;

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

static int esp876_write_reg(struct i2c_client *client, uint16_t reg, uint8_t val)
{
    int ret = 0;

    uint32_t lval = val;
    ret = i2c_write_reg(client, reg, I2C_REG_VALUE_08BIT, lval);

    ESP876_API_I2C_DEBUG("%s: [I2C_WR] [0x%04X] = [0x%02X](%d)\n", client->adapter->name, reg, val, ret);
    return ret;
}

static int esp876_read_reg(struct i2c_client *client, uint16_t reg, uint8_t *val)
{
    int ret;

    uint32_t lval = 0;
    ret = i2c_read_reg(client, reg, I2C_REG_VALUE_08BIT, &lval);
    *val = lval;

    ESP876_API_I2C_DEBUG("%s: [I2C_RD] [0x%04X] = [0x%02X](%d)\n", client->adapter->name, reg, *val, ret);
    return ret;
}

static void my_sleep(unsigned int ms)
{
    if (ms <= 20) {
        usleep_range((ms * 1000), (ms * 1000));
    } else {
        msleep(ms);
    }
}

static int wait_reg_for_target(struct i2c_client *client, uint16_t reg, uint8_t val, uint8_t target)
{
    unsigned char bufi[1] = {0x0};
    struct i2c_client *fd = client;
    unsigned int retry_new_command_count = 0;
    int ret = 0;
    int ret_line = 0;
    unsigned int try_count = 0;

    if (!fd) {
        ret = -EINVAL;
        ret_line = __LINE__;
        goto exit;
    }

retry_new_command:
    if (retry_new_command_count == RETRY_NEW_COMMAND_MAX_COUNT) {
        ret = -EBUSY;
#if defined(ESP876_API_I2C_DEBUG_ENABLE)
        ESP876_API_I2C_DEBUG("[I2C_RD] [0x%04X] ?= [0x%02X](retry %u times) (W:0x%02x)(T:0x%02x)\n", reg, bufi[0], RETRY_NEW_COMMAND_MAX_COUNT, val, target);
#endif
        ret_line = __LINE__;
        goto exit;
    }
    my_sleep(SLEEP_PARM);
    try_count = 0;
    ret = esp876_write_reg(fd, reg, val);
    ESP876_API_I2C_DEBUG("Wait for [0x%04X] == [0x%02X](%d)\n", reg, target, ret);
    // If I2C bus NOT connect to eSP876, just exit
    if (ret == -EIO) {
        ESP876_ERR_DEBUG("%s: I/O error(%d)", fd->adapter->name, ret);
        ret_line = __LINE__;
        goto exit;
    }
    while (1) {
        ret = esp876_read_reg(fd, reg, bufi);
        if (try_count == I2C_READ_TRY_MAX_COUNT) {
#if defined(ESP876_API_I2C_DEBUG_ENABLE)
            ESP876_API_I2C_DEBUG("[I2C_RD] [0x%04X] ?= [0x%02X](%ums) (W:0x%02x)(T:0x%02x)\n", reg, bufi[0], (SLEEP_PARM * I2C_READ_TRY_MAX_COUNT), val, target);
#endif
            try_count = 0;
            ret = -EBUSY;
            if (val == 0x80) {
                retry_new_command_count++;
                goto retry_new_command;
            } else {
                ret_line = __LINE__;
                goto exit;
            }
        }
        if (bufi[0] == target) {
            ret = 0;
            break;
        }
        else my_sleep(SLEEP_PARM);
        try_count++;
    }

exit:
#if defined(ESP876_API_I2C_DEBUG_ENABLE)
    ESP876_API_I2C_DEBUG("Leave(%d, %d)\n", ret, ret_line);
#endif

    return ret;
}

static int get_pid_vid(struct i2c_client *client, uint16_t *pid, uint16_t *vid)
{
    int ret = 0;
    uint8_t data[32] = {0x0};
    uint8_t nID = 1;
    uint16_t val = 0;

    if (!pid || !vid) {
        return -EINVAL;
    }

    *pid = 0;
    *vid = 0;

    /* 0x4e, 0x1e, 0x67, 0x01 */
    ret = esp876_read_fw_data(client, ESP876_API_FW_FS_MEM_TYPE_FS_TABLE, nID, 0, data, 32);
    if (ret == 0) {
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

static int enable_mipi_output(struct i2c_client *client, bool en)
{
    int ret = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    //ESP876_API_DEBUG("driv = [0x%p], sd = [0x%p], client = [%p]\n",driv, sd, client);

    if (en) {
        ret = esp876_write_fw_reg(client, 0xF1, 0x10); //MIPI output
        ENTER_CS
        if (ret == 0) {
            priv->mipi_out_en = true;
        } else {
            priv->mipi_out_en = false;
        }
        LEAVE_CS
    } else {
        ret = esp876_write_fw_reg(client, 0xF1, 0x00); //USB output
        ENTER_CS
        if (ret == 0) {
            priv->mipi_out_en = false;
        }
        LEAVE_CS
    }
    return ret;
}

static int esp876_read_fw_version(struct i2c_client *client, char *version)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_READ_REG;
    int i = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd || !version) {
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);

    esp876_write_reg(fd, 0x802B, 0x05);
    esp876_write_reg(fd, 0x802B, 0x00);
    esp876_write_reg(fd, 0x802B, 0x00);
    esp876_write_reg(fd, 0x802B, 0x00);
    esp876_write_reg(fd, 0x802B, 0x00);
    esp876_write_reg(fd, 0x802B, 0x0f);
    esp876_write_reg(fd, 0x802B, 0x00);
    esp876_write_reg(fd, 0x802B, 0x20);

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);
    for (i = 0; i < 32; i++) {
        esp876_read_reg(fd, 0x802B, (unsigned char *)&version[i]);
    }


    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

    version[32] = '\0';

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_video_switch(struct i2c_client *client, uint32_t mode_index)
{
    unsigned char cmdOpt_02 = 0;
    unsigned char cmdOpt_03 = 0;
    unsigned char cmdOpt_04 = 0;
    unsigned char cmdOpt_05 = 0;
    struct i2c_client *fd = client;
    int ret = 0;
    int ret_line = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_VIDEO_MODE;
    const struct esp876_video_mode *mode = NULL;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS

#if defined(ESP876_API_I2C_DEBUG_ENABLE)
    ESP876_API_I2C_DEBUG("Enter(%u)\n", mode_index);
#else
    ESP876_ALERT_DEBUG("Enter(0x%08x)\n", mode_index);
#endif

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
    cmdOpt_02 =  mode->param.cmdOpt_02;
    cmdOpt_03 =  mode->param.cmdOpt_03;
    cmdOpt_04 =  mode->param.cmdOpt_04;
    cmdOpt_05 =  mode->param.cmdOpt_05;

#if !defined(ESP876_API_I2C_DEBUG_ENABLE)
    ESP876_API_DEBUG("[%u][0x%02x, 0x%02x, 0x%02x, 0x%02x]\n", mode_index, cmdOpt_02, cmdOpt_03, cmdOpt_04, cmdOpt_05);
#endif

    my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        ret_line = __LINE__;
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x802B, cmdOpt_02);
    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x802B, cmdOpt_03);
    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x802B, cmdOpt_04);
    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x802B, cmdOpt_05);

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        ret_line = __LINE__;
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
#if defined(ESP876_API_I2C_DEBUG_ENABLE)
    ESP876_API_I2C_DEBUG("Leave(%d, %d)\n", ret, ret_line);
#else
    ESP876_ALERT_DEBUG("Leave(%d, %d)\n", ret, ret_line);
#endif

    LEAVE_CS

    return ret;
}

static int esp876_video_close(struct i2c_client *client)
{
    struct i2c_client *fd = client;
    int ret = 0;
    int ret_line = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_VIDEO_CLOSE;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS;

#if defined(ESP876_API_I2C_DEBUG_ENABLE)
    ESP876_API_I2C_DEBUG("Enter\n");
#else
    ESP876_ALERT_DEBUG("Enter\n");
#endif

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        ret_line = __LINE__;
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, 0xFF); //[0xDC02] = [0xff]
    esp876_write_reg(fd, 0x802B, 0x00); //[0xDC03] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); //[0xDC04] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); //[0xDC05] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); //[0xDC06] = [0x00]

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        ret_line = __LINE__;
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        ret_line = __LINE__;
        goto exit;
    }

exit:
#if defined(ESP876_API_I2C_DEBUG_ENABLE)
    ESP876_API_I2C_DEBUG("Leave(%d, %d)\n", ret, ret_line);
#else
    ESP876_ALERT_DEBUG("Leave(%d, %d)\n", ret, ret_line);
#endif

    LEAVE_CS

    return ret;
}

static int esp876_write_fw_reg(struct i2c_client *client, uint8_t fw_reg, uint8_t fw_val)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t cmd = 0x81;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x81]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ESP876_API_REG_TYPE_FW_REG); // [0xDC02] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC03] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC04] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC05] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC06] = [0x00]
    esp876_write_reg(fd, 0x802B, fw_reg); // [0xDC07] = [fw_reg]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC08] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x01); // [0xDC09] = [0x01]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);
    esp876_write_reg(fd, 0x802B, fw_val); //[0xDC20] = [0x01]


    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_read_fw_reg(struct i2c_client *client, uint8_t fw_reg, uint8_t *val)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_READ_REG;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x81]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ESP876_API_REG_TYPE_FW_REG); // [0xDC02] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC03] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC04] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC05] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC06] = [0x00]
    esp876_write_reg(fd, 0x802B, fw_reg); // [0xDC07] = [fw_reg]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC08] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x01); // [0xDC09] = [0x01]

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);
    esp876_read_reg(fd, 0x802B, val); //[0xDC20] = [val]
    ESP876_API_I2C_DEBUG("[0x802B] = [0x%02x]\n", *val);

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int read_next(struct i2c_client *client, unsigned char *input, int size, bool has_next)
{
    int length = size;
    int i = 0;
    int ret  = 0;
    struct i2c_client *fd = client;

    for (i = 0; i < length; i++) {
        esp876_read_reg(fd, 0x802B, (unsigned char *)&input[i]);
    }

    my_sleep(SLEEP_PARM);

    if (has_next) {
        ret = wait_reg_for_target(fd, 0x873f, 0x03, 0x02);
        if (ret != 0) {
            ret = -EBUSY;
            goto exit;
        }
    }

exit:
    return ret;
}

static int esp876_read_asic_reg(struct i2c_client *client, uint16_t reg, uint8_t *data, uint16_t len)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_READ_REG;
    int i = 0;
    uint32_t wr_buf = MAX_ASIC_REG_BUF_SIZE;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    if (len > MAX_ASIC_REG_WR_LEN) {
        ESP876_ERR_DEBUG("%u > %u!\n", len, MAX_ASIC_REG_WR_LEN);
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x41]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ESP876_API_REG_TYPE_HW_REG); // [0xDC02] = [0x01]
    if (len > 1) {
        esp876_write_reg(fd, 0x802B, 0x00); // [0xDC03] = [0x01]
    } else {
        esp876_write_reg(fd, 0x802B, 0x01); // [0xDC03] = [0x01]
    }
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC04] = [00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC05] = [00]
    esp876_write_reg(fd, 0x802B, reg >> 8);     // [0xDC06] = [addr(MSB)]
    esp876_write_reg(fd, 0x802B, reg & 0xff);   // [0xDC07] = [addr(LSB)]
    esp876_write_reg(fd, 0x802B, len >> 8);     //[0xDC08] = [0x00]
    esp876_write_reg(fd, 0x802B, len & 0xff);     //[0xDC09] = [0x01]

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);

    if (len <= wr_buf) {
        for (i = 0; i < len; i++) {
            esp876_read_reg(fd, 0x802B, (unsigned char *)&data[i]);
        }
    } else {
        int count = len / wr_buf;
        int diff = len - (count * wr_buf);
        bool has_next = true;

        for (i = 0; i < count; i++){
            if ((diff == 0) && (i == (count -1))) {
                has_next = false;
            }
            ret = read_next(fd, &data[i * wr_buf], wr_buf, has_next);
            if (ret != 0) {
                goto exit;
            }
        }

        if (diff != 0) {
            ret =  read_next(fd, &data[i * wr_buf], diff, false);
        }
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int write_next(struct i2c_client *client, unsigned char *input, int size, bool has_next, bool first_write)
{
    int length = size;
    int i = 0;
    int ret  = 0;
    struct i2c_client *fd = client;

    for (i = 0; i < length; i++) {
        esp876_write_reg(fd, 0x802B, input[i]);
    }

    my_sleep(SLEEP_PARM);

    if (first_write) {
        ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
        if (ret != 0) {
            ret = -EBUSY;
            goto exit;
        }
    } else {
        if (has_next) {
            ret = wait_reg_for_target(fd, 0x873f, 0x03, 0x02);
            if (ret != 0) {
                ret = -EBUSY;
                goto exit;
            }
        }
    }

exit:
    return ret;
}

static int esp876_write_asic_reg(struct i2c_client *client, uint16_t reg, uint8_t *data, uint16_t len)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t cmd = 0x81;
    int i = 0;
    uint32_t wr_buf = MAX_ASIC_REG_BUF_SIZE;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    if (len > MAX_ASIC_REG_WR_LEN) {
        ESP876_ERR_DEBUG("%u > %u!\n", len, MAX_ASIC_REG_WR_LEN);
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x81]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ESP876_API_REG_TYPE_HW_REG); // [0xDC02] = [0x01]
    if (len > 1) {
        esp876_write_reg(fd, 0x802B, 0x00); // [0xDC03] = [0x01]
    } else {
        esp876_write_reg(fd, 0x802B, 0x01); // [0xDC03] = [0x01]
    }
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC04] = [00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC05] = [00]
    esp876_write_reg(fd, 0x802B, reg >> 8);     // [0xDC06] = [addr(MSB)]
    esp876_write_reg(fd, 0x802B, reg & 0xff);   // [0xDC07] = [addr(LSB)]
    esp876_write_reg(fd, 0x802B, len >> 8);     //[0xDC08] = [0x00]
    esp876_write_reg(fd, 0x802B, len & 0xff);     //[0xDC09] = [0x01]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);

    if (len <= wr_buf) {
        for (i = 0; i < len; i++) {
            esp876_write_reg(fd, 0x802B, data[i]); //[[0xDC20] = val
        }

        ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
        if (ret != 0) {
            ret = -EBUSY;
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
            ret = write_next(fd, &data[i * wr_buf], wr_buf, has_next, first_w);
            if (ret != 0) {
                goto exit;
            }
            if (first_w) {
                first_w = false;
            }
        }

        if (diff != 0) {
            ret =  write_next(fd, &data[i * wr_buf], diff, false, first_w);
        }
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_read_fd(struct i2c_client *client, uint8_t memType, uint8_t Id, uint32_t Offset, uint8_t *Data, uint16_t Len)
{
    struct i2c_client *fd = client;
    int ret = 0;
    int i = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_READ_REG;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS

#if defined(ESP876_API_I2C_DEBUG_ENABLE)
    ESP876_API_I2C_DEBUG("Enter\n");
#else
    //ESP876_API_DEBUG("Enter, memType: 0x%x, Id: 0x%x, Offset: 0x%08x, Len: %d\n", memType, Id, Offset, Len);
#endif

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    if (!Data) {
        ret = -EINVAL;
        goto exit;
    }
/*
    //The max buffer length is MAX_FS_BUF_SIZE bytes.
    if (Len > MAX_FS_BUF_SIZE) {
        ret = -EINVAL;
        goto exit;
    }

    my_sleep(500);
*/

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

   /*
     * Data[0] = pFWFSData->nMutex;
     * Data[1] = 0x40 | 0x01; //b[7:6]=1 Read, b[5:0]=1 Memory I/O Command
     * Data[2] = 0x05;  //FileSystem Table
     * Data[3] = 0; //Acess Same Address
     * Data[4] = 0;
     * Data[5] = pFWFSData->nFsID; //FileSystem ID
     * Data[6] = (BYTE)((pFWFSData->nOffset >> 8) & 0xFF); //Offset-H
     * Data[7] = (BYTE)(pFWFSData->nOffset & 0xFF); //Offset-L
     * Data[8] = 0;
     * Data[9] = (BYTE)pFWFSData->nLength; //Read-Data Length
     */

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);                      // [0xDC01] = [0x41]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, memType);                  // [0xDC02] = MemType
    switch (memType) {
        case ESP876_API_FW_FS_MEM_TYPE_SPI_FLASH:
            esp876_write_reg(fd, 0x802B, 0x00);             // [0xDC03] = [0x00]
            esp876_write_reg(fd, 0x802B, Offset >> 24);     // [0xDC04] = [0x00]
            esp876_write_reg(fd, 0x802B, Offset >> 16);     // [0xDC05] = [Id]
            esp876_write_reg(fd, 0x802B, Offset >> 8);      // [0xDC06] = [Offset (MSB)]
            esp876_write_reg(fd, 0x802B, Offset & 0xff);    // [0xDC07] = [Offset (LSB)]
            esp876_write_reg(fd, 0x802B, Len >> 8);         // [0xDC08] = [Len (MSB)]
            esp876_write_reg(fd, 0x802B, Len & 0xff);       // [0xDC09] = [Len (LSB)]
            break;
        case ESP876_API_FW_FS_MEM_TYPE_FS_TABLE:
            esp876_write_reg(fd, 0x802B, 0x00);             // [0xDC03] = [0x00]
            esp876_write_reg(fd, 0x802B, 0x00);             // [0xDC04] = [0x00]
            esp876_write_reg(fd, 0x802B, Id);               // [0xDC05] = [Id]
            esp876_write_reg(fd, 0x802B, Offset >> 8);      // [0xDC06] = [Offset (MSB)]
            esp876_write_reg(fd, 0x802B, Offset & 0xff);    // [0xDC07] = [Offset (LSB)]
            esp876_write_reg(fd, 0x802B, Len >> 8);         // [0xDC08] = [Len (MSB)]
            esp876_write_reg(fd, 0x802B, Len & 0xff);       // [0xDC09] = [Len (LSB)]
            break;
        default:
            break;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);
    for (i = 0; i < Len; i++) {
        esp876_read_reg(fd, 0x802B, (unsigned char *)&Data[i]);
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_write_fd(struct i2c_client *client, uint8_t memType, uint8_t Id, uint32_t Offset, uint8_t *Data, uint16_t Len)
{
    struct i2c_client *fd = client;
    int ret = 0;
    int i = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_WRITE_REG;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS

#if defined(ESP876_API_I2C_DEBUG_ENABLE)
    ESP876_API_I2C_DEBUG("Enter\n");
#else
    //ESP876_API_DEBUG("Enter, memType: 0x%x, Id: 0x%x, Offset: 0x%08x, Len: %d\n", memType, Id, Offset, Len);
#endif

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    if (!Data) {
        ret = -EINVAL;
        goto exit;
    }
/*
    //The max buffer length is MAX_FS_BUF_SIZE bytes.
    if (Len > MAX_FS_BUF_SIZE) {
        ret = -EINVAL;
        goto exit;
    }

    my_sleep(500);
*/

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

   /*
     * Data[0] = pFWFSData->nMutex;
     * Data[1] = 0x80 | 0x01; //b[7:6]=2 Write, b[5:0]=1 FileSystem Table
     * Data[2] = 0x05;  //FileSystem Table
     * Data[3] = 0; //Acess Same Address
     * Data[4] = 0;
     * Data[5] = pFWFSData->nFsID; //FileSystem ID
     * Data[6] = (BYTE)((pFWFSData->nOffset >> 8) & 0xFF); //Offset-H
     * Data[7] = (BYTE)(pFWFSData->nOffset & 0xFF); //Offset-L
     * Data[8] = 0;
     * Data[9] = (BYTE)pFWFSData->nLength; //Write-Data Length
     */

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);              // [0xDC01] = [0x81]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, memType);          // [0xDC02] = MemType
    switch (memType) {
        case ESP876_API_FW_FS_MEM_TYPE_SPI_FLASH:
            esp876_write_reg(fd, 0x802B, 0x00);             // [0xDC03] = [0x00]
            esp876_write_reg(fd, 0x802B, Offset >> 24);     // [0xDC04] = [0x00]
            esp876_write_reg(fd, 0x802B, Offset >> 16);     // [0xDC05] = [Id]
            esp876_write_reg(fd, 0x802B, Offset >> 8);      // [0xDC06] = [Offset (MSB)]
            esp876_write_reg(fd, 0x802B, Offset & 0xff);    // [0xDC07] = [Offset (LSB)]
            esp876_write_reg(fd, 0x802B, Len >> 8);         // [0xDC08] = [Len (MSB)]
            esp876_write_reg(fd, 0x802B, Len & 0xff);       // [0xDC09] = [Len (LSB)]
            break;
        case ESP876_API_FW_FS_MEM_TYPE_FS_TABLE:
            esp876_write_reg(fd, 0x802B, 0x00);             // [0xDC03] = [0x00]
            esp876_write_reg(fd, 0x802B, 0x00);             // [0xDC04] = [0x00]
            esp876_write_reg(fd, 0x802B, Id);               // [0xDC05] = [Id]
            esp876_write_reg(fd, 0x802B, Offset >> 8);      // [0xDC06] = [Offset (MSB)]
            esp876_write_reg(fd, 0x802B, Offset & 0xff);    // [0xDC07] = [Offset (LSB)]
            esp876_write_reg(fd, 0x802B, Len >> 8);         // [0xDC08] = [Len (MSB)]
            esp876_write_reg(fd, 0x802B, Len & 0xff);       // [0xDC09] = [Len (LSB)]
            break;
        default:
            break;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);
    for (i = 0; i < Len; i++) {
        esp876_write_reg(fd, 0x802B, Data[i]);      //[[0xDC20] = Data
    }

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_sync_fd(struct i2c_client *client)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_SYNC_COMMAND;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x02]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ESP876_API_REG_TYPE_HW_REG); // [0xDC02] = [0x01]

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_read_fw_data(struct i2c_client *client, uint8_t memType, uint8_t Id, uint32_t Offset, uint8_t* Data, uint16_t readLen)
{
    uint8_t *ptr = Data;
    uint16_t nOffset = 0;
    uint16_t nLength = 0;
    int ret = 0;

    if (!ptr || !client) {
        ret = -EINVAL;
        goto exit;
    }

    if (readLen<= MAX_FS_BUF_SIZE) {
        ret = esp876_read_fd(client, memType, Id, Offset, ptr, readLen);
        if (ret != 0) {
            goto exit;
        }
    } else {
        nOffset = 0;
        while (nOffset < readLen) {
            nLength = readLen - nOffset;

            if (nLength > MAX_FS_BUF_SIZE) {
                nLength = MAX_FS_BUF_SIZE;
            }

            ret = esp876_read_fd(client, memType, Id, nOffset, (ptr + nOffset), nLength);
            if (ret != 0) {
                goto exit;
            }
            nOffset += nLength;
        }
    }

exit:
    return ret;
}

static int esp876_write_fw_data(struct i2c_client *client, uint8_t memType, uint8_t Id, uint32_t Offset, uint8_t* Data, uint16_t writeLen)
{
    uint8_t *ptr = Data;
    uint16_t nOffset = 0;
    uint16_t nLength = 0;
    int ret = 0;

    if (!ptr || !client) {
        ret = -EINVAL;
        goto exit;
    }

    if (writeLen <= MAX_FS_BUF_SIZE) {
        ret = esp876_write_fd(client, memType, Id, Offset, ptr, writeLen);
        if (ret != 0) {
            goto exit;
        }
    } else {
        nOffset = 0;
        while (nOffset < writeLen) {
            nLength = writeLen - nOffset;

            if (nLength > MAX_FS_BUF_SIZE) {
                nLength = MAX_FS_BUF_SIZE;
            }

            ret = esp876_write_fd(client, memType, Id, nOffset, (ptr + nOffset), nLength);
            if (ret != 0) {
                goto exit;
            }
            nOffset += nLength;
        }
    }

    ret = esp876_sync_fd(client);

exit:
    return ret;
}

static int esp876_read_i2c_reg(struct i2c_client *client, uint8_t slave_id, uint8_t format, uint16_t addr, uint16_t *data)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint16_t addr_len = 0;
    uint16_t data_len = 0;
    uint8_t tmp8 = 0;
    uint16_t tmp16 = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_READ_REG;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    if (!data) {
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

#if defined(_APN_REV1_9_20220114_)
    /* NOTE: 16-bit address/8-bit data,  16-bit address/16-bit data, 8-bit address/16-bit data, 8-bit address/8-bit data */
    if (format == 0x10) {
        //16-bit address/8-bit data
        addr_len = 2;
        data_len = 1;
    } else  if (format == 0x30) {
        //16-bit address/16-bit data
        addr_len = 2;
        data_len = 2;
    } else  if (format == 0x20) {
        //8-bit address/16-bit data
        addr_len = 1;
        data_len = 2;
    } else {
        //8-bit address/8-bit data
        addr_len = 1;
        data_len = 1;
    }
#else
    /* NOTE: 16-bit address/8-bit data,  16-bit address/16-bit data, 8-bit address/16-bit data, 8-bit address/8-bit data */
    if (format == 0x20) {
        //16-bit address/8-bit data
        addr_len = 2;
        data_len = 1;
    } else  if (format ==  0x30) {
        //16-bit address/16-bit data
        addr_len = 2;
        data_len = 2;
    } else  if (format ==  0x10) {
        //8-bit address/16-bit data
        addr_len = 1;
        data_len = 2;
    } else {
        //8-bit address/8-bit data
        addr_len = 1;
        data_len = 1;
    }
#endif

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x41]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ESP876_API_REG_TYPE_SENSOR_REG); // [0xDC02] = [0x02]
    esp876_write_reg(fd, 0x802B, format); // [0xDC03] = [format]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC04] = [0x00]
    esp876_write_reg(fd, 0x802B, slave_id); // [0xDC05] = [id]
    esp876_write_reg(fd, 0x802B, addr >> 8); // [0xDC06] = [addr (MSB)]
    esp876_write_reg(fd, 0x802B, addr & 0xff); // [0xDC07] = [addr (LSB)]
#if defined(_APN_REV1_9_20220114_)
    esp876_write_reg(fd, 0x802B, data_len >> 8); // [0xDC08] = [len (MSB)]
    esp876_write_reg(fd, 0x802B, data_len & 0xff); // [0xDC09] = [len (LSB)]
#else
    esp876_write_reg(fd, 0x802B, data_len); // [0xDC08] = [len]
    esp876_write_reg(fd, 0x802B, 0x01); // [0xDC09] = [0x01]
#endif

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);
    if (data_len == 1) {
        tmp8 = 0;
        tmp16 = 0;
        esp876_read_reg(fd, 0x802B, &tmp8);
        ESP876_API_I2C_DEBUG("[0x802B] = [0x%02x]\n", tmp8);
        tmp16 = tmp8;
        *data = (*data & ~0x00ff) | tmp16;
    } else if (data_len == 2) {
        tmp8 = 0;
        tmp16 = 0;
        esp876_read_reg(fd, 0x802B, &tmp8);    // Read MSB first
        ESP876_API_I2C_DEBUG("[0x802B] = [0x%02x]\n", tmp8);
        tmp16 = tmp8;
        *data = (*data & ~0x00ff) | tmp16;

        tmp8 = 0;
        tmp16 = 0;
        esp876_read_reg(fd, 0x802B, &tmp8);    // Read LSB
        ESP876_API_I2C_DEBUG("[0x802B] = [0x%02x]\n", tmp8);
        tmp16 = tmp8;
        *data = (*data << 8) | tmp16;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_write_i2c_reg(struct i2c_client *client, uint8_t slave_id, uint8_t format, uint16_t addr, uint16_t data)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint16_t addr_len = 0;
    uint16_t data_len = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_WRITE_REG;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

#if defined(_APN_REV1_9_20220114_)
    /* NOTE: 16-bit address/8-bit data,  16-bit address/16-bit data, 8-bit address/16-bit data, 8-bit address/8-bit data */
    if (format == 0x10) {
        //16-bit address/8-bit data
        addr_len = 2;
        data_len = 1;
    } else  if (format == 0x30) {
        //16-bit address/16-bit data
        addr_len = 2;
        data_len = 2;
    } else  if (format == 0x20) {
        //8-bit address/16-bit data
        addr_len = 1;
        data_len = 2;
    } else {
        //8-bit address/8-bit data
        addr_len = 1;
        data_len = 1;
    }
#else
    /* NOTE: 16-bit address/8-bit data,  16-bit address/16-bit data, 8-bit address/16-bit data, 8-bit address/8-bit data */
    if (format == 0x20) {
        //16-bit address/8-bit data
        addr_len = 2;
        data_len = 1;
    } else  if (format == 0x30) {
        //16-bit address/16-bit data
        addr_len = 2;
        data_len = 2;
    } else  if (format == 0x10) {
        //8-bit address/16-bit data
        addr_len = 1;
        data_len = 2;
    } else {
        //8-bit address/8-bit data
        addr_len = 1;
        data_len = 1;
    }
#endif

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x81]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ESP876_API_REG_TYPE_SENSOR_REG); // [0xDC02] = [0x02]
    esp876_write_reg(fd, 0x802B, format); // [0xDC03] = [format]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC04] = [0x00]
    esp876_write_reg(fd, 0x802B, slave_id); // [0xDC05] = [id]
    esp876_write_reg(fd, 0x802B, addr >> 8); // [0xDC06] = [addr (MSB)]
    esp876_write_reg(fd, 0x802B, addr & 0xff); // [0xDC07] = [addr (LSB)]
#if defined(_APN_REV1_9_20220114_)
    esp876_write_reg(fd, 0x802B, data_len >> 8); // [0xDC08] = [len (MSB)]
    esp876_write_reg(fd, 0x802B, data_len & 0xff); // [0xDC09] = [len (LSB)]
#else
    esp876_write_reg(fd, 0x802B, data_len); // [0xDC08] = [len]
    esp876_write_reg(fd, 0x802B, 0x01); // [0xDC09] = [0x01]
#endif

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);
    if (data_len == 1) {
        esp876_write_reg(fd, 0x802B, data & 0xff);
    } else if (data_len == 2) {
        esp876_write_reg(fd, 0x802B, data >> 8);    // Write MSB first
        esp876_write_reg(fd, 0x802B, data & 0xff);    // Write LSB
    }

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int property_bar_val_mask(uint8_t ct_pu_id, uint8_t ctrsel, uint32_t *mask)
{
    int i = 0;
    int num = 0;
    const struct esp876_uvc_ctrl *ctrl = NULL;

    if (!mask) {
        return -EINVAL;
    }

    /* 0x01: CT, 0x03: PU */
    if (ct_pu_id == 0x03) {
        ctrl = g_uvc_pu_ctrls;
        num = sizeof(g_uvc_pu_ctrls)/sizeof(struct esp876_uvc_ctrl);
    } else if (ct_pu_id == 0x01){
        ctrl = g_uvc_ct_ctrls;
        num = sizeof(g_uvc_ct_ctrls)/sizeof(struct esp876_uvc_ctrl);
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
                    *mask = 0xff;
                    break;
                case 2:
                    *mask = 0xffff;
                    break;
                case 3:
                    *mask = 0xffffff;
                    break;
                case 4:
                    *mask = 0xffffffff;
                    break;
                default:
                    *mask = 0xffffffff;
            }
            break;
        }
    }

    return 0;
}

static int esp876_write_property_bar(struct i2c_client *client, uint8_t ct_pu_id, uint8_t type, uint8_t ctrsel, uint32_t val)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_WRITE_PROPERTY_BAR;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    /* 0x01: CT, 0x03: PU */
    if ((ct_pu_id != ESP876_API_CT_PROPERTY_ID) &&
        (ct_pu_id != ESP876_API_PU_PROPERTY_ID)) {
        ret = -EINVAL;
        goto exit;
    }

    //my_sleep(500);

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x81]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ct_pu_id); // [0xDC02] = [ct_pu_val]
    esp876_write_reg(fd, 0x802B, type); // [0xDC03] = [0x01]
    esp876_write_reg(fd, 0x802B, ctrsel); // [0xDC04] = [ctrl]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC05] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC06] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC07] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC08] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC09] = [0x00]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);
    esp876_write_reg(fd, 0x802B, (val & 0x000000ff));
    esp876_write_reg(fd, 0x802B, (val & 0x0000ff00) >> 8);
    esp876_write_reg(fd, 0x802B, (val & 0x00ff0000) >> 16);
    esp876_write_reg(fd, 0x802B, (val & 0xff000000) >> 24);

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_read_property_bar(struct i2c_client *client, uint8_t ct_pu_id, uint8_t type, uint8_t ctrsel, uint32_t *val)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t tmp8 = 0x00;
    uint8_t tmp32 = 0x00;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_READ_PROPERTY_BAR;
    uint32_t mask = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    (void)mask;
    (void)property_bar_val_mask;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    /* 0x01: CT, 0x03: PU */
    if ((ct_pu_id != ESP876_API_CT_PROPERTY_ID) &&
        (ct_pu_id != ESP876_API_PU_PROPERTY_ID)) {
        ret = -EINVAL;
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x81]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ct_pu_id); // [0xDC02] = [ct_pu_val]
    esp876_write_reg(fd, 0x802B, type); // [0xDC03] = [0x01]
    esp876_write_reg(fd, 0x802B, ctrsel); // [0xDC04] = [ctrl]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC05] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC06] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC07] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC08] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC09] = [0x00]

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);

    tmp8 = 0;
    tmp32 = 0;
    esp876_read_reg(fd, 0x802B, &tmp8);
    tmp32 = tmp8;
    *val = (*val & ~0x000000ff) | tmp32;

    tmp8 = 0;
    tmp32 = 0;
    esp876_read_reg(fd, 0x802B, &tmp8);
    tmp32 = tmp8;
    *val = (*val & ~0x0000ff00) | (tmp32 << 8);

    tmp8 = 0;
    tmp32 = 0;
    esp876_read_reg(fd, 0x802B, &tmp8);
    tmp32 = tmp8;
    *val = (*val & ~0x00ff0000) | (tmp32 << 16);

    tmp8 = 0;
    tmp32 = 0;
    esp876_read_reg(fd, 0x802B, &tmp8);
    tmp32 = tmp8;
    *val = (*val & ~0xff000000) | (tmp32 << 24);

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

    if (property_bar_val_mask(ct_pu_id, ctrsel, &mask) == 0) {
        *val = *val & mask;
    }

   /* NOTE: The CAMERA_HW_GET_INFO should be 1 bytes [page 75 of USB_Video_Class_1.1.pdf]:
     * The GET_INFO request queries the capabilities and status of the specified control. When issuing
     * this request, the wLength field shall always be set to a value of 1 byte. The result returned is a
     * bit mask reporting the capabilities of the control.
     */
    if (type == CAMERA_HW_GET_INFO) {
        *val = *val & 0xff;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int esp876_read_property_bar_support_list(struct i2c_client *client, uint8_t ct_pu_id, uint16_t *val)
{
    struct i2c_client *fd = client;
    int ret = 0;
    uint8_t tmp8 = 0x00;
    uint8_t tmp16 = 0x00;
    uint8_t cmd = ESP876_API_FW_CMD_TYPE_GET_SUPPORTR_LIST;
    uint32_t mask = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    (void)mask;
    (void)property_bar_val_mask;

    ENTER_CS
    ESP876_API_I2C_DEBUG("Enter\n");

    if (!fd) {
        ret = -EINVAL;
        goto exit;
    }

    /* 0x01: CT, 0x03: PU */
    if ((ct_pu_id != ESP876_API_CT_PROPERTY_ID) &&
        (ct_pu_id != ESP876_API_PU_PROPERTY_ID)) {
        ret = -EINVAL;
        goto exit;
    }

    ret = wait_reg_for_target(fd, 0x873f, 0x80, 0x81);
    if (ret != 0) {
        goto exit;
    }

    my_sleep(SLEEP_PARM);
    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x01);
    esp876_write_reg(fd, 0x802B, cmd);// [0xDC01] = [0x81]

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x02);
    esp876_write_reg(fd, 0x802B, ct_pu_id); // [0xDC02] = [ct_pu_val]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC03] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC04] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC05] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC06] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC07] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC08] = [0x00]
    esp876_write_reg(fd, 0x802B, 0x00); // [0xDC09] = [0x00]

    ret = wait_reg_for_target(fd, 0x873f, 0x01, 0x02);
    if (ret != 0) {
        goto exit;
    }

    esp876_write_reg(fd, 0x8029, 0xDC);
    esp876_write_reg(fd, 0x802A, 0x20);

    tmp8 = 0;
    tmp16 = 0;
    esp876_read_reg(fd, 0x802B, &tmp8);
    tmp16 = tmp8;
    *val = (*val & ~0x00ff) | tmp16;

    tmp8 = 0;
    tmp16 = 0;
    esp876_read_reg(fd, 0x802B, &tmp8);
    tmp16 = tmp8;
    *val = (*val & ~0xff00) | (tmp16 << 8);

    ret = wait_reg_for_target(fd, 0x873f, 0xf0, 0x00);
    if (ret != 0) {
        goto exit;
    }

exit:
    ESP876_API_I2C_DEBUG("Leave(%d)\n", ret);
    LEAVE_CS

    return ret;
}

static int set_current_video_mode(struct i2c_client *client, uint32_t mode_index)
{
    int ret = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ENTER_CS

    priv->cur_video_mode_index = mode_index;
    ESP876_API_DEBUG("cur_video_mode_index = %d\n", priv->cur_video_mode_index);

    LEAVE_CS

    return ret;
}

static uint32_t esp876_get_current_video_mode(struct i2c_client *client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    ESP876_API_DEBUG("cur_video_mode_index = %d\n", priv->cur_video_mode_index);

    return priv->cur_video_mode_index;
}

static int set_error(struct i2c_client *client, int code)
{
    int ret = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    ENTER_CS
    priv->cur_error_code = code;
    LEAVE_CS

    return ret;
}

static int set_is_opened(struct i2c_client *client, bool val)
{
    int ret = 0;
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    ENTER_CS
    priv->is_opened = val;
    LEAVE_CS

    return ret;
}

static int parse_cmd_config(const char *cmd, char *param[], int param_count, const char *delim)
{
    char *found = NULL;
    int i = 0;
    char *cmd_tmp = NULL;
    char **param_tmp = NULL;

    cmd_tmp = (char *)cmd;

    while( (found = strsep(&cmd_tmp, delim)) != NULL ) {
        if (i >= param_count) {
            break;
        }
        param_tmp = &param[i];
        *param_tmp = found;
        i++;
    }

    return 0;
}

static ssize_t esp876_show(struct device *dev,
                      struct device_attribute *attr, char *buf)
{
    char *str = NULL;
    int str_len  = 1024;
    uint8_t bSetVal = 0;
    ssize_t ret_size = 0;
    uint32_t val_32 = 0;
    uint16_t val_16 = 0;
    uint8_t val_8 = 0;
    //int ret = 0;
    bool is_call_get_error = false;
    int ret_api = 0;
    struct i2c_client *client = NULL;
    struct v4l2_subdev *sd = NULL;
    struct esp876 *driv = NULL;
    struct esp876_api_data *priv = NULL;

    (void)driv;
    (void)priv;

    (void)val_32;
    (void)val_16;
    (void)val_8;

    if (!dev) {
         return 0;
    }

    client = to_i2c_client(dev);
    if (!client) {
         return 0;
    }

    sd = i2c_get_clientdata(client);
    if (!sd) {
         return 0;
    }

    driv = sd_to_esp876(sd);
    if (!driv) {
         return 0;
    }

    priv = driv->data;
    if (!priv) {
         return 0;
    }

    str = kzalloc(str_len, GFP_KERNEL);

    if (!str) {
        return 0;
    }

    memset(str, 0x0, str_len);

    if (strcmp(attr->attr.name,
        __stringify(fw_version)) == 0) {
        bSetVal = 1;
        ret_api = esp876_read_fw_version(driv->client, str);
    } else if (strcmp(attr->attr.name,
        __stringify(video_mode)) == 0) {
        bSetVal = 1;
        val_32 = esp876_get_current_video_mode(driv->client);
        snprintf(str, str_len, "%u", val_32);
    } else if (strcmp(attr->attr.name,
        __stringify(erro_code)) == 0) {
        bSetVal = 1;
        is_call_get_error = true;
        snprintf(str, str_len, "%d", priv->cur_error_code);
    } else if (strcmp(attr->attr.name,
        __stringify(mipi_out_en)) == 0) {
        bSetVal = 1;
        snprintf(str, str_len, "%d", priv->mipi_out_en);
    } else if (strcmp(attr->attr.name,
        __stringify(stream_on)) == 0) {
        bSetVal = 1;
        snprintf(str, str_len, "%d", priv->stream_on);
    } else if (strcmp(attr->attr.name,
        __stringify(read_asic_reg)) == 0) {
        if (priv->is_read_asic_reg) {
            bSetVal = 1;
            ret_api = esp876_read_asic_reg(driv->client, priv->cur_asic_reg, &val_8, 1);
            snprintf(str, str_len, "0x%02x", val_8);
            ENTER_CS
            priv->is_read_asic_reg = false;
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_fw_reg)) == 0) {
        if (priv->is_read_fw_reg) {
            bSetVal = 1;
            ret_api = esp876_read_fw_reg(driv->client, priv->cur_fw_reg, &val_8);
            snprintf(str, str_len, "0x%02x", val_8);
            ENTER_CS
            priv->is_read_fw_reg = false;
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_i2_reg)) == 0) {
        if (priv->is_read_i2c_reg) {
            bSetVal = 1;
            ret_api = esp876_read_i2c_reg(driv->client, priv->cur_i2_slave_id, priv->cur_i2_format, priv->cur_i2_addr, &val_16);
#if defined(_APN_REV1_9_20220114_)
            if (priv->cur_i2_format == 0x10) {
                //16-bit address/8-bit data
                snprintf(str, str_len, "0x%02x", (uint8_t)(val_16 & 0xff));
            } else  if (priv->cur_i2_format == 0x30) {
                //16-bit address/16-bit data
                snprintf(str, str_len, "0x%04x", val_16);
            } else  if (priv->cur_i2_format == 0x20) {
                //8-bit address/16-bit data
                snprintf(str, str_len, "0x%04x", val_16);
            } else {
                //8-bit address/8-bit data
                snprintf(str, str_len, "0x%02x", (uint8_t)(val_16 & 0xff));
            }
#else
            if (priv->cur_i2_format == 0x20) {
                //16-bit address/8-bit data
                snprintf(str, str_len, "0x%02x", (uint8_t)(val_16 & 0xff));
            } else  if (priv->cur_i2_format == 0x30) {
                //16-bit address/16-bit data
                snprintf(str, str_len, "0x%04x", val_16);
            } else  if (priv->cur_i2_format == 0x10) {
                //8-bit address/16-bit data
                snprintf(str, str_len, "0x%04x", val_16);
            } else {
                //8-bit address/8-bit data
                snprintf(str, str_len, "0x%02x", (uint8_t)(val_16 & 0xff));
            }
#endif
            ENTER_CS
            priv->is_read_i2c_reg = false;
            LEAVE_CS
        }
    }  else if (strcmp(attr->attr.name,
        __stringify(read_ct_pu_value)) == 0) {
        if (priv->is_read_pb) {
            bSetVal = 1;
            ret_api = esp876_read_property_bar(driv->client, priv->cur_pb_ct_pu_id, priv->cur_pb_type, priv->cur_pb_ctrsel, &val_32);
            snprintf(str, str_len, "0x%08x", val_32);
            ENTER_CS
            priv->is_read_pb = false;
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_ct_pu_support_list)) == 0) {
        if (priv->is_read_pb) {
            bSetVal = 1;
            ret_api = esp876_read_property_bar_support_list(driv->client, priv->cur_pb_ct_pu_id, &val_16);
            snprintf(str, str_len, "0x%04x", val_16);
            ENTER_CS
            priv->is_read_pb = false;
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(fs_id)) == 0) {
        bSetVal = 1;
        snprintf(str, str_len, "%u", priv->cur_fs_id);
    } else if (strcmp(attr->attr.name,
        __stringify(fs_offset)) == 0) {
        bSetVal = 1;
        snprintf(str, str_len, "%u", priv->cur_fs_offset);
    } else if (strcmp(attr->attr.name,
        __stringify(asic_id)) == 0) {
        bSetVal = 1;
        snprintf(str, str_len, "0x%04x", priv->cur_asic_id);
    } else if (strcmp(attr->attr.name,
        __stringify(video_modes)) == 0) {
        const struct esp876_video_mode *mode = NULL;
        int offset = 0;
        char *s = str;
        int i = 0;
        while(1) {
            mode =  esp876_api_get_video_mode_by_index(driv, i);
            if (!mode)
                break;
            offset  = snprintf(s, str_len, "%u:%u:%u:%u:%u:%d:%d:0x%08x:%d:%d|", mode->mode_index,
                               mode->width, mode->height, mode->fps,
                               mode->depth_bits, (int)mode->is_rectify,
                               (int)mode->is_interleave_mode, mode->pixelcode, mode->is_scale_down, mode->color_bits);
            i++;
            s += offset;
        }
        bSetVal = 1;
    } else if (strcmp(attr->attr.name,
        __stringify(is_opened)) == 0) {
        bSetVal = 1;
        snprintf(str, str_len, "%d", priv->is_opened);
    } else {
       /*DO nothing*/
    }

    if (is_call_get_error == false) {
        set_error(driv->client, ret_api);
    }

    if ((bSetVal == 1) && (buf != NULL))
        ret_size =  sprintf(buf, "%s\n", str);
    else
        ret_size =  0;

    if (!str) {
        kfree(str);
        str = NULL;
    }

    return ret_size;
}

static ssize_t esp876_store(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long val = 0;
    int ret = 0;
    bool is_call_get_error = false;
    uint8_t reg_8 = 0;
    uint16_t reg_16 = 0;
    uint8_t val_8 = 0;
    uint16_t val_16 = 0;
    char *param_strs[10] = {0};
    size_t ret_count = count;
    int ret_api = 0;
    struct i2c_client *client = NULL;
    struct v4l2_subdev *sd = NULL;
    struct esp876 *driv = NULL;
    struct esp876_api_data *priv = NULL;

    (void)driv;
    (void)priv;

    (void)reg_8;
    (void)reg_16;
    (void)val_8;
    (void)val_16;

    if (!dev) {
        goto exit;
    }

    client = to_i2c_client(dev);
    if (!client) {
        goto exit;
    }

    sd = i2c_get_clientdata(client);
    if (!sd) {
        goto exit;
    }

    driv = sd_to_esp876(sd);
    if (!driv) {
        goto exit;
    }

    priv = driv->data;
    if (!priv) {
        goto exit;
    }

    if ((!attr) || (!buf))
        goto exit;

    if (strcmp(attr->attr.name,
        __stringify(video_mode)) == 0) {
        ret = kstrtoul(buf, 10, (unsigned long *)&val);
        ESP876_ALERT_DEBUG("video_mode = [%ld](%s)\n", val, buf);
        ret_api = set_current_video_mode(driv->client, val);
    } else if (strcmp(attr->attr.name,
        __stringify(mipi_out_en)) == 0) {
        ret = kstrtoul(buf, 10, (unsigned long *)&val);
        ret_api = enable_mipi_output(driv->client, (bool)val);
    } else if (strcmp(attr->attr.name,
        __stringify(read_asic_reg)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_asic_reg = (uint16_t)(val & 0xffff);
        //ESP876_API_DEBUG("priv->cur_asic_reg = [0x%04x](%s)\n", priv->cur_asic_reg, buf);
        priv->is_read_asic_reg = true;
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(read_fw_reg)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_fw_reg =  (uint8_t)(val & 0xff);
        //ESP876_API_DEBUG("priv->cur_fw_reg = [0x%02x](%s)\n", priv->cur_fw_reg, buf);
        priv->is_read_fw_reg = true;
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(write_fw_reg)) == 0) {
        ret = parse_cmd_config(buf, param_strs, 2, " ");
        if (ret == 0) {
            if (param_strs[0] && param_strs[1]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                reg_8 = (uint8_t)(val & 0xff);
                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                val_8 = (uint8_t)(val & 0xff);
                //ESP876_API_DEBUG("reg = 0x%02x\n", reg_8);
                //ESP876_API_DEBUG("val = 0x%02x\n", val_8);
                ret_api = esp876_write_fw_reg(driv->client, reg_8, val_8);
            }
        }
    } else if (strcmp(attr->attr.name,
        __stringify(write_asic_reg)) == 0) {
        ret = parse_cmd_config(buf, param_strs, 2, " ");
        if (ret == 0) {
            if (param_strs[0] && param_strs[1]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                reg_16 = (uint16_t)(val & 0xffff);
                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                val_8 = (uint8_t)(val & 0xff);
                //ESP876_API_DEBUG("reg = 0x%04x\n", reg_16);
                //ESP876_API_DEBUG("val = 0x%02x\n", val_8);
                ret_api = esp876_write_asic_reg(driv->client, reg_16, &val_8, 1);
            }
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_i2_reg)) == 0) {
        //echo  "0x80 0x20 0x000a" > /sys/bus/i2c/devices/2-0060/read_i2_reg
        ret = parse_cmd_config(buf, param_strs, 3, " ");
        if (ret == 0) {
            ENTER_CS
            if (param_strs[0] && param_strs[1] && param_strs[2]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                priv->cur_i2_slave_id = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                priv->cur_i2_format = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[2], 16, (unsigned long *)&val);
                priv->cur_i2_addr = (uint16_t)(val & 0xffff);

                priv->is_read_i2c_reg = true;

                //ESP876_API_DEBUG("priv->cur_i2_slave_id = 0x%02x\n", priv->cur_i2_slave_id);
                //ESP876_API_DEBUG("priv->cur_i2_format = 0x%02x\n", priv->cur_i2_format);
                //ESP876_API_DEBUG("priv->cur_i2_addr = 0x%02x\n", priv->cur_i2_addr);
            }
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(write_i2_reg)) == 0) {
        //echo  "0x80 0x00 0x000c 0x01" > /sys/bus/i2c/devices/2-0060/write_i2_reg
        ret = parse_cmd_config(buf, param_strs, 4, " ");
        if (ret == 0) {
            uint8_t slave_id = 0;
            uint8_t format = 0;
            uint16_t addr = 0;
            uint16_t data = 0;
            if (param_strs[0] && param_strs[1] && param_strs[2] && param_strs[3]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                slave_id = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                format = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[2], 16, (unsigned long *)&val);
                addr = (uint16_t)(val & 0xffff);

                ret = kstrtoul(param_strs[3], 16, (unsigned long *)&val);
                data = (uint16_t)(val & 0xffff);

                ret_api = esp876_write_i2c_reg(driv->client, slave_id, format, addr, data);
            }
        }
    } else if (strcmp(attr->attr.name,
        __stringify(write_ct_pu_value)) == 0) {
     /*
        * 1. echo "[ct_pu_id] [type] [ctrsel] [val]" > /sys/bus/i2c/devices/2-0060/write_ct_pu_value
        * 2. For CT(1)/CAMERA_HW_SET_CUR(0x01)/CAMERA_HW_CT_AE_MODE_CONTROL(0x02)/CAMERA_HW_EXPOSURE_SHUTTER_PRIORITY(0x2)
        * echo  "0x01 0x81 0x02 0x00000002" > /sys/bus/i2c/devices/2-0060/write_ct_pu_value
        */
        ret = parse_cmd_config(buf, param_strs, 4, " ");
        if (ret == 0) {
            uint8_t pb_ct_pu_id = 0;
            uint8_t pb_type = 0;
            uint8_t pb_ctrsel = 0;
            uint32_t pb_val = 0;
            if (param_strs[0] && param_strs[1] && param_strs[2] && param_strs[3]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                pb_ct_pu_id = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                pb_type = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[2], 16, (unsigned long *)&val);
                pb_ctrsel = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[3], 16, (unsigned long *)&val);
                pb_val = (uint32_t)(val & 0xffffffff);

                //ESP876_API_DEBUG("pb_ct_pu_id = 0x%02x\n", pb_ct_pu_id);
                //ESP876_API_DEBUG("pb_type = 0x%02x\n", pb_type);
                //ESP876_API_DEBUG("pb_ctrsel = 0x%02x\n", pb_ctrsel);
                //ESP876_API_DEBUG("pb_val = 0x%08x\n", pb_val);

                ret_api = esp876_write_property_bar(driv->client, pb_ct_pu_id, pb_type, pb_ctrsel, pb_val);
                if (ret_api == 0) {
                    ret_api = esp876_sync_fd(driv->client);
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
        ret = parse_cmd_config(buf, param_strs, 3, " ");
        if (ret == 0) {
            ENTER_CS
            if (param_strs[0] && param_strs[1] && param_strs[2]) {
                ret = kstrtoul(param_strs[0], 16, (unsigned long *)&val);
                priv->cur_pb_ct_pu_id = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[1], 16, (unsigned long *)&val);
                priv->cur_pb_type = (uint8_t)(val & 0xff);

                ret = kstrtoul(param_strs[2], 16, (unsigned long *)&val);
                priv->cur_pb_ctrsel = (uint8_t)(val & 0xff);

                priv->is_read_pb = true;

                //ESP876_API_DEBUG("priv->cur_pb_ct_pu_id = 0x%02x\n", priv->cur_pb_ct_pu_id);
                //ESP876_API_DEBUG("priv->cur_pb_type = 0x%02x\n", priv->cur_pb_type);
                //ESP876_API_DEBUG("priv->cur_pb_ctrsel = 0x%02x\n", priv->cur_pb_ctrsel);
            }
            LEAVE_CS
        }
    } else if (strcmp(attr->attr.name,
        __stringify(read_ct_pu_support_list)) == 0) {
     /*
        * 1. echo "[ct_pu_id]" > /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        * 2. For CT(1) Support List
        * echo  "0x01" > /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        * cat /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        * 2. For PU(3) Support List
        * echo  "0x03" > /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        * cat /sys/bus/i2c/devices/2-0060/read_ct_pu_support_list
        */
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_pb_ct_pu_id = (uint8_t)(val & 0xff);
        priv->is_read_pb = true;
        //ESP876_API_DEBUG("priv->cur_pb_ct_pu_id = 0x%02x\n", priv->cur_pb_ct_pu_id);
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(fs_id)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 10, (unsigned long *)&val);
        priv->cur_fs_id = (uint8_t)(val & 0xff);
        //ESP876_API_DEBUG("priv->cur_fs_id = [0x%02x](%s)\n", priv->cur_fs_id, buf);
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(fs_offset)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_fs_offset = (uint32_t)(val & 0xffffffff);
        //ESP876_API_DEBUG("priv->cur_fs_offset = [0x%08x](%s)\n", priv->cur_fs_offset, buf);
        LEAVE_CS
    } else if (strcmp(attr->attr.name,
        __stringify(asic_id)) == 0) {
        ENTER_CS
        ret = kstrtoul(buf, 16, (unsigned long *)&val);
        priv->cur_asic_id = (uint16_t)(val & 0xffff);
        //ESP876_API_DEBUG("priv->cur_asic_id = [0x%04x](%s)\n", priv->cur_asic_id, buf);
        LEAVE_CS
    }  else if (strcmp(attr->attr.name,
        __stringify(is_opened)) == 0) {
        ret = kstrtoul(buf, 10, (unsigned long *)&val);
        ret_api = set_is_opened(driv->client, (bool)val);
    }  else {
       /*DO nothing*/
    }

    if (is_call_get_error == false) {
        set_error(driv->client, ret_api);
    }
    //ESP876_API_DEBUG("Leave\n");

exit:

    return ret_count;
}

static struct device_attribute fw_version_attribute = __ATTR(
    fw_version, 0664, esp876_show, esp876_store);

static struct device_attribute video_mode_attribute = __ATTR(
    video_mode, 0664, esp876_show, esp876_store);

static struct device_attribute video_modes_attribute = __ATTR(
    video_modes, 0664, esp876_show, esp876_store);

static struct device_attribute erro_code_attribute = __ATTR(
    erro_code, 0664, esp876_show, esp876_store);

static struct device_attribute mipi_out_en_attribute = __ATTR(
    mipi_out_en, 0664, esp876_show, esp876_store);

static struct device_attribute stream_on_attribute = __ATTR(
    stream_on, 0664, esp876_show, esp876_store);

static struct device_attribute read_fw_reg_attribute = __ATTR(
    read_fw_reg, 0664, esp876_show, esp876_store);

static struct device_attribute write_fw_reg_attribute = __ATTR(
    write_fw_reg, 0664, esp876_show, esp876_store);

static struct device_attribute read_i2_reg_attribute = __ATTR(
    read_i2_reg, 0664, esp876_show, esp876_store);

static struct device_attribute write_i2_reg_attribute = __ATTR(
    write_i2_reg, 0664, esp876_show, esp876_store);

static struct device_attribute read_asic_reg_attribute = __ATTR(
    read_asic_reg, 0664, esp876_show, esp876_store);

static struct device_attribute write_asic_reg_attribute = __ATTR(
    write_asic_reg, 0664, esp876_show, esp876_store);

static struct device_attribute read_ct_pu_value_attribute = __ATTR(
    read_ct_pu_value, 0664, esp876_show, esp876_store);

static struct device_attribute write_ct_pu_value_attribute = __ATTR(
    write_ct_pu_value, 0664, esp876_show, esp876_store);

static struct device_attribute read_ct_pu_support_list_attribute = __ATTR(
    read_ct_pu_support_list, 0664, esp876_show, esp876_store);

static struct device_attribute fs_id_attribute = __ATTR(
    fs_id, 0664, esp876_show, esp876_store);

static struct device_attribute fs_offset_attribute = __ATTR(
    fs_offset, 0664, esp876_show, esp876_store);

static struct device_attribute asic_id_attribute = __ATTR(
    asic_id, 0664, esp876_show, esp876_store);

static struct device_attribute is_opened_attribute = __ATTR(
    is_opened, 0664, esp876_show, esp876_store);

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
    (struct attribute *)&is_opened_attribute.attr,
    NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

static int my_sysfs_init(struct kobject *kobj)
{
    int retval = -1;

    if (kobj != NULL) {
        retval = sysfs_create_group(kobj, &attr_group);
    }
    return retval;
}

static int my_sysfs_rls(struct kobject *kobj)
{
    if (!kobj)
        return -EINVAL;

    sysfs_remove_group(kobj, &attr_group);

    return 0;
}

struct sys_bin_data {
    struct bin_attribute bin;
    struct mutex buffer_lock;
    uint8_t buffer[FS_BIN_BUFFER_SIZE];
};

static ssize_t my_sysfs_bin_read(
        struct file *filp,
        struct kobject *kobj,
        struct bin_attribute *attr,
        char *buf, loff_t off, size_t count);

static ssize_t my_sysfs_bin_write(
        struct file *filp,
        struct kobject *kobj,
        struct bin_attribute *attr,
        char *buf, loff_t off, size_t count);

static int my_sysfs_bin_init(struct kobject *kobj)
{
    struct sys_bin_data *fs_bin = NULL;
    struct sys_bin_data *flash_bin = NULL;
    struct sys_bin_data *asic_bin = NULL;
    int ret = 0;
    struct i2c_client *client = kobj_to_i2c_client(kobj);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    fs_bin = kzalloc(sizeof(struct sys_bin_data), GFP_KERNEL);

    if (!fs_bin)
        return -ENOMEM;

    priv->fs_bin = fs_bin;

    mutex_init(&(fs_bin->buffer_lock));
    sysfs_bin_attr_init(&fs_bin->bin);
    fs_bin->bin.attr.name = FS_BIN_FILE_NAME;
    fs_bin->bin.attr.mode = 0600;/*S_IRUSR | S_IWUSR;*/
    fs_bin->bin.read = my_sysfs_bin_read;
    fs_bin->bin.write = my_sysfs_bin_write;
    fs_bin->bin.size = FS_BIN_BUFFER_SIZE;

    ret = sysfs_create_bin_file(kobj, &fs_bin->bin);

    flash_bin = kzalloc(sizeof(struct sys_bin_data), GFP_KERNEL);

    if (!flash_bin)
        return -ENOMEM;

    priv->flash_bin = flash_bin;

    mutex_init(&(flash_bin->buffer_lock));
    sysfs_bin_attr_init(&flash_bin->bin);
    flash_bin->bin.attr.name = FLASH_BIN_FILE_NAME;
    flash_bin->bin.attr.mode = 0600;/*S_IRUSR | S_IWUSR;*/
    flash_bin->bin.read = my_sysfs_bin_read;
    flash_bin->bin.write = my_sysfs_bin_write;
    flash_bin->bin.size = FS_BIN_BUFFER_SIZE;

    ret = sysfs_create_bin_file(kobj, &flash_bin->bin);

    asic_bin = kzalloc(sizeof(struct sys_bin_data), GFP_KERNEL);

    if (!asic_bin)
        return -ENOMEM;

    priv->asic_reg_bin = asic_bin;

    mutex_init(&(asic_bin->buffer_lock));
    sysfs_bin_attr_init(&fs_bin->bin);
    asic_bin->bin.attr.name = ASIC_REG_BIN_FILE_NAME;
    asic_bin->bin.attr.mode = 0600;/*S_IRUSR | S_IWUSR;*/
    asic_bin->bin.read = my_sysfs_bin_read;
    asic_bin->bin.write = my_sysfs_bin_write;
    asic_bin->bin.size = FS_BIN_BUFFER_SIZE;

    ret = sysfs_create_bin_file(kobj, &asic_bin->bin);

    return ret;
}

static int my_sysfs_bin_rls(struct kobject *kobj)
{
    int ret = 0;
    struct i2c_client *client = kobj_to_i2c_client(kobj);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

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

static ssize_t my_sysfs_bin_read(struct file *filp, struct kobject *kobj,
        struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
    struct sys_bin_data *bin = NULL;
    unsigned int remain = 0;
    unsigned int num = 0;
    unsigned char *ptr = NULL;
    int i = 0;
    int ret = 0;
    size_t ret_count = 0;
    struct i2c_client *client = kobj_to_i2c_client(kobj);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    (void)remain;
    (void)num;
    (void)i;
    (void)ret;

    //ESP876_API_DEBUG("driv = [0x%p], sd = [0x%p], client = [0x%p], priv =[0x%p]\n", driv, sd, client, priv);

    if (!attr)
        return 0;

    if (strncmp(attr->attr.name, FS_BIN_FILE_NAME,
        strlen(FS_BIN_FILE_NAME)) == 0) {
        bin = priv->fs_bin;
    } else if (strncmp(attr->attr.name, FLASH_BIN_FILE_NAME,
        strlen(FLASH_BIN_FILE_NAME)) == 0) {
        bin = priv->flash_bin;
    } else if (strncmp(attr->attr.name, ASIC_REG_BIN_FILE_NAME,
        strlen(ASIC_REG_BIN_FILE_NAME)) == 0) {
        bin = priv->asic_reg_bin;
    }

    if (!bin)
        return 0;

    //ESP876_API_DEBUG("%s\n", attr->attr.name);

    mutex_lock(&(bin->buffer_lock));
    ret_count = count;
    ptr = &bin->buffer[off];
    memset(ptr, 0x0, count);
    if (strncmp(attr->attr.name, FS_BIN_FILE_NAME,
        strlen(FS_BIN_FILE_NAME)) == 0) {
        ret = esp876_read_fw_data(driv->client, ESP876_API_FW_FS_MEM_TYPE_FS_TABLE, priv->cur_fs_id, 0, ptr, count);
        //ESP876_API_DEBUG("(%d, %d, %d)(%d)\n", (int)count, (int)off, (int)priv->cur_fs_id, ret);
    } else if (strncmp(attr->attr.name, FLASH_BIN_FILE_NAME,
        strlen(FS_BIN_FILE_NAME)) == 0) {
        ret = esp876_read_fw_data(driv->client, ESP876_API_FW_FS_MEM_TYPE_SPI_FLASH, 0, priv->cur_fs_offset, ptr, count);
        ESP876_API_DEBUG("(%d, %d, 0x%08x)(%d)\n", (int)count, (int)off, (int)priv->cur_fs_offset, ret);
    } else if (strncmp(attr->attr.name, ASIC_REG_BIN_FILE_NAME,
        strlen(ASIC_REG_BIN_FILE_NAME)) == 0) {
        ret = esp876_read_asic_reg(driv->client, priv->cur_asic_id, ptr, count);
        //ESP876_API_DEBUG("(%d, %d, 0x%04x)(%d)\n", (int)count, (int)off, priv->cur_asic_id, ret);
    }

    if (ret != 0) {
        ret_count = 0;
        goto exit;
    }

    memcpy(buf, &bin->buffer[off], count);

exit:
    mutex_unlock(&(bin->buffer_lock));

    set_error(driv->client, ret);

    return ret_count;
}

static ssize_t my_sysfs_bin_write(struct file *filp, struct kobject *kobj,
        struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
    struct sys_bin_data *bin = NULL;
    unsigned int remain = 0;
    unsigned int num = 0;
    unsigned char *ptr = NULL;
    int i = 0;
    int ret = 0;
    size_t ret_count = 0;
    struct i2c_client *client = kobj_to_i2c_client(kobj);
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct esp876 *driv = sd_to_esp876(sd);
    struct esp876_api_data *priv = driv->data;

    (void)driv;
    (void)priv;

    (void)remain;
    (void)num;
    (void)i;
    (void)ret;

    //ESP876_API_DEBUG("driv = [0x%p], sd = [0x%p], client = [0x%p], priv =[0x%p]\n", driv, sd, client, priv);

    if (!attr)
        return 0;

    if (strncmp(attr->attr.name, FS_BIN_FILE_NAME,
        strlen(FS_BIN_FILE_NAME)) == 0) {
        bin = priv->fs_bin;
    } else if (strncmp(attr->attr.name, FLASH_BIN_FILE_NAME,
        strlen(FLASH_BIN_FILE_NAME)) == 0) {
        bin = priv->flash_bin;
    } else if (strncmp(attr->attr.name, ASIC_REG_BIN_FILE_NAME,
        strlen(ASIC_REG_BIN_FILE_NAME)) == 0) {
          bin = priv->asic_reg_bin;
    }

    if (!bin)
        return 0;

    //ESP876_API_DEBUG("%s\n", attr->attr.name);

    mutex_lock(&(bin->buffer_lock));
    memcpy(&bin->buffer[off], buf, count);
    ret_count = count;
    ptr = &bin->buffer[off];
    if (strncmp(attr->attr.name, FS_BIN_FILE_NAME,
        strlen(FS_BIN_FILE_NAME)) == 0) {
        ret = esp876_write_fw_data(driv->client, ESP876_API_FW_FS_MEM_TYPE_FS_TABLE, priv->cur_fs_id, 0, ptr, count);
        //ESP876_API_DEBUG("(%d, %d, %d)(%d)\n", (int)count, (int)off, (int)priv->cur_fs_id, ret);
    } else if (strncmp(attr->attr.name, FLASH_BIN_FILE_NAME,
        strlen(FLASH_BIN_FILE_NAME)) == 0) {
        ret = esp876_write_fw_data(driv->client, ESP876_API_FW_FS_MEM_TYPE_SPI_FLASH, 0, priv->cur_fs_offset, ptr, count);
        ESP876_API_DEBUG("(%d, %d, 0x%08x)(%d)\n", (int)count, (int)off, (int)priv->cur_fs_offset, ret);
    } else if (strncmp(attr->attr.name, ASIC_REG_BIN_FILE_NAME,
        strlen(ASIC_REG_BIN_FILE_NAME)) == 0) {
        ret = esp876_write_asic_reg(driv->client, priv->cur_asic_id, ptr, count);
        //ESP876_API_DEBUG("(%d, %d, 0x%04x)(%d)\n", (int)count, (int)off, priv->cur_asic_id, ret);
    }

    if (ret != 0) {
        ret_count = 0;
        goto exit;
    }
exit:
    mutex_unlock(&(bin->buffer_lock));

    set_error(driv->client, ret);

    return ret_count;
}

static struct proc_dir_entry *g_proc_i2c[ESP876_API_SUPPORTED_DEV_COUNT] = {NULL};
static struct proc_dir_entry *g_proc_version = NULL;
static int g_proc_created_count = 0;
static struct mutex g_proc_lock = __MUTEX_INITIALIZER(g_proc_lock);
static bool g_is_enable_version_check = true;

//NOTE: How to pass the priv to my_i2c_proc_show:https://blog.csdn.net/wbd880419/article/details/51955805
static int my_i2c_proc_show (struct seq_file *file, void *v)
{
    struct i2c_client *client = NULL;

    client = (struct i2c_client *)file->private;

    //ESP876_API_DEBUG("client = [0x%p]\n", client);

    if (client) {
        seq_printf(file, "%d-%04x", client->adapter->nr, client->addr);
    }

    return 0;
}

static ssize_t my_version_proc_write(struct file *file,
                                     const char __user *buffer,
                                     size_t count, loff_t *f_pos)
{
    unsigned long val = 0;
    char *tmp = NULL;
    int ret = 0;

    tmp = kzalloc((count+1),GFP_KERNEL);
    if(!tmp)return -ENOMEM;

    if(copy_from_user(tmp,buffer,count)){
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

static int my_version_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d:%s:%s", (int)g_is_enable_version_check, ESP876_SDK_VERSION, ESP876_API_VERSION);
    return 0;
}

static int my_i2c_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, my_i2c_proc_show, PDE_DATA(inode));
}

static int my_version_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, my_version_proc_show, PDE_DATA(inode));
}

/* For NVIDIA TX2 */
#if LINUX_VERSION_CODE == KERNEL_VERSION(4,9,140)
static const struct file_operations my_i2c_proc_ops = {
    .open = my_i2c_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static const struct file_operations my_version_proc_ops = {
    .open = my_version_proc_open,
    .write = my_version_proc_write,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#else
static const struct proc_ops my_i2c_proc_ops = {
    .proc_open = my_i2c_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops my_version_proc_ops = {
    .proc_open = my_version_proc_open,
    .proc_write = my_version_proc_write,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#endif

static int my_proc_init(struct i2c_client *client)
{
    int ret = 0;
    char prc_path[64] = {0};

    if (g_proc_created_count >= ESP876_API_SUPPORTED_DEV_COUNT) {
        ret =  -EINVAL;
        goto exit;
    }

    mutex_lock(&g_proc_lock);

    if (g_proc_created_count == 0) {
        snprintf(prc_path, sizeof(prc_path), "%s", ESP876_API_I2C_INFO_PROCFS_NODE);
    } else {
        snprintf(prc_path, sizeof(prc_path), "%s_%d", ESP876_API_I2C_INFO_PROCFS_NODE, g_proc_created_count);
    }

    g_proc_i2c[g_proc_created_count] = proc_create_data(prc_path, 0777, NULL, &my_i2c_proc_ops, (void *)client);
    if (!g_proc_i2c[g_proc_created_count]) {
        ret =  -EINVAL;
        goto exit;
    }

    if (g_proc_created_count == 0) {
        g_proc_version = proc_create_data(ESP876_API_VERSION_INFO_PROCFS_NODE, 0777, NULL, &my_version_proc_ops, (void *)client);
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

static void my_proc_rls(struct i2c_client *client)
{
    int i = 0;
    (void)client;

    for (i = 0; i < ESP876_API_SUPPORTED_DEV_COUNT; i++) {
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
