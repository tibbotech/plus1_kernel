#ifndef __EYS3D_API_H__
#define __EYS3D_API_H__

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/media-bus-format.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include "eys3d_camera_hw.h"
#include "eys3d_version.h"

#define EYS3D_CAMERA_MODULE_IS_ESP876       1
#define EYS3D_CAMERA_MODULE_IS_P2C          0

#define EYS3D_API_IGNORE_I2C_KEEP_MIPI      0
#define EYS3D_API_APN_REV1_9_20220114       1

#define EYS3D_ALERT_DBG_PRT_ENABLE          0
#define EYS3D_ERROR_DEBUG_ENABLE            1
#define EYS3D_API_DBG_PRT_ENABLE            0
#define EYS3D_I2C_DBG_PRT_ENABLE            0

#if EYS3D_CAMERA_MODULE_IS_ESP876
#define EYS3D_I2C_DEVICE_ID_NAME            "eSP876"
#define DEBUG_PRINTF(format, ...)           pr_info("[eYs3D_eSP876][%s][%d]"format, __func__, __LINE__, ##__VA_ARGS__)
#elif EYS3D_CAMERA_MODULE_IS_P2C
#define EYS3D_I2C_DEVICE_ID_NAME            "eSP876"//"P2C"
#define DEBUG_PRINTF(format, ...)           pr_info("[eYs3D_P2C][%s][%d]"format, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if EYS3D_ALERT_DBG_PRT_ENABLE
#define EYS3D_ALERT_DBG_PRT                 DEBUG_PRINTF
#else
#define EYS3D_ALERT_DBG_PRT(fmt, args...)   do {} while (0)
#endif

#if EYS3D_ERROR_DEBUG_ENABLE
#define EYS3D_ERR_DBG_PRT                   DEBUG_PRINTF
#else
#define EYS3D_ERR_DBG_PRT(fmt, args...)     do {} while (0)
#endif

#if EYS3D_API_DBG_PRT_ENABLE
#define EYS3D_API_DBG_PRT                   DEBUG_PRINTF
#else
#define EYS3D_API_DBG_PRT(fmt, args...)     do {} while (0)
#endif

#if EYS3D_I2C_DBG_PRT_ENABLE
#define EYS3D_I2C_DBG_PRT                   DEBUG_PRINTF
#else
#define EYS3D_I2C_DBG_PRT(fmt, args...)     do {} while (0)
#endif

#define EYS3D_API_RET_OK                    0

#define EYS3D_API_SLEEP_PARM                1       // ms
#define EYS3D_API_I2C_READ_TRY_MAX_CNT      2000    // timeout = (1 * 2000)ms = 2sec
#define EYS3D_API_CHK_VID_STAT_TRY_MAX_CNT  10000   // timeout = (1 * 10000)ms = 10sec
#define EYS3D_API_RETRY_NEW_CMD_MAX_CNT     10
#define EYS3D_API_MAX_FS_BUF_SZ             32
#define EYS3D_API_MAX_ASIC_REG_BUF_SZ       32
#define EYS3D_API_MAX_ASIC_REG_WR_LEN       128

#define EYS3D_API_VERSION_INFO_PROCFS_NODE  "eys3d_version_info"
#define EYS3D_API_I2C_INFO_PROCFS_NODE      "eys3d_i2c_info"
#define EYS3D_API_FS_BIN_FILE_NAME          "fs_data"
#define EYS3D_API_FLASH_BIN_FILE_NAME       "flash_data"
#define EYS3D_API_ASIC_REG_BIN_FILE_NAME    "asic_data"

#define EYS3D_API_I2C_REG_VALUE_08BIT       1
#define EYS3D_API_I2C_REG_VALUE_16BIT       2 
#define EYS3D_API_I2C_REG_VALUE_24BIT       3

#define EYS3D_API_CAMERA_PID_UNKOWN         0xffff
#define EYS3D_API_CAMERA_PID_SANDRA         0x0167
#define EYS3D_API_CAMERA_PID_NORA           0x0168
#define EYS3D_API_CAMERA_PID_EX8036         0x0120
#define EYS3D_API_CAMERA_PID_HELEN          0x0171
#define EYS3D_API_CAMERA_PID_GRAPE          0x0188

#define EYS3D_API_CAMERA_VID_UNKOWN         0xffff
#define EYS3D_API_CAMERA_VID_EEVER          0x1e4e
#define EYS3D_API_CAMERA_VID_EYS3D          0x3438
#define EYS3D_API_CAMERA_VID_EX8036         0x1e4e

#define EYS3D_API_SP_DEV_CNT                8
#define EYS3D_API_VC_0_I2C_ADDR             0x60
#define EYS3D_API_VC_1_I2C_ADDR             0x61
#define EYS3D_API_VC_2_I2C_ADDR             0x62
#define EYS3D_API_VC_3_I2C_ADDR             0x63

#define EYS3D_API_VC_ID_0                   0
#define EYS3D_API_VC_ID_1                   1
#define EYS3D_API_VC_ID_2                   2
#define EYS3D_API_VC_ID_3                   3

#define EYS3D_API_FS_BIN_BUF_SZ             (EYS3D_API_MAX_FS_BUF_SZ * 1024)

#if EYS3D_CAMERA_MODULE_IS_ESP876
/* Refer: MIPI TX I2C Data Format.doc */
#define EYS3D_API_CMD_REQ                   0xDC01
#define EYS3D_API_CMD_OPT                   0xDC02
#define EYS3D_API_CMD_DAT                   0xDC20
/* 2 Byte Address */
#define EYS3D_API_CMD_DAT_REG               0x802B
#define EYS3D_API_CMD_TRIG_REG              0x873F
#elif EYS3D_CAMERA_MODULE_IS_P2C
/* Refer: eSP876_APN_MIPIOut_Interface_rev1.10-20220715.pdf */
#define EYS3D_API_AUTO_INCREASE_ADDR        0x80
#define EYS3D_API_CMD_REQ                   0x01
#define EYS3D_API_CMD_OPT                   (0x02 | EYS3D_API_AUTO_INCREASE_ADDR)
#define EYS3D_API_CMD_DAT                   (0x10 | EYS3D_API_AUTO_INCREASE_ADDR)
/* 1 Byte Address */
#define EYS3D_API_CMD_DAT_REG               (0x8000 | ((0xF0BE << 2) & 0x3F00) | (0xF0BE & 0x003F))
#define EYS3D_API_CMD_TRIG_REG              (0x8000 | ((0xF0BF << 2) & 0x3F00) | (0xF0BF & 0x003F))
#endif
/* For eSP876 2 Byte Address */
#define EYS3D_API_CMD_ADR_MSB_REG           0x8029
#define EYS3D_API_CMD_ADR_LSB_REG           0x802A
/* For P2C 1 Byte Address */
#define EYS3D_API_CMD_ADR_REG               (0x8000 | ((0xF0BD << 2) & 0x3F00) | (0xF0BD & 0x003F))
#define EYS3D_API_CMD_TRIG_INT_REG          (0x8000 | ((0xFE01 << 2) & 0x3F00) | (0xFE01 & 0x003F))

/* Command Request Flow */
#define EYS3D_API_CRF_DEV_RDY_ENTER         0x80
#define EYS3D_API_CRF_DEV_RDY_LEAVE         0x81
#define EYS3D_API_CRF_CMD_PRE_ENTER         0x01
#define EYS3D_API_CRF_CMD_PRE_LEAVE         0x02
#define EYS3D_API_CRF_CMD_NEX_ENTER         0x03
#define EYS3D_API_CRF_CMD_CLS_ENTER         0xF0
#define EYS3D_API_CRF_CMD_CLS_LEAVE         0x00

/* Command Request */
#define EYS3D_API_CMD_REQ_DEV_IO_REQ        0x01
#define EYS3D_API_CMD_REQ_DEV_IO_DAT_SYNC   0x02
#define EYS3D_API_CMD_REQ_SUSPEND_MODE_CTRL 0x03
#define EYS3D_API_CMD_REQ_RE_DEV_INFO       0x11
#define EYS3D_API_CMD_REQ_RE_SENSOR_INFO    0x12
#define EYS3D_API_CMD_REQ_RE_VID_INFO       0x13
#define EYS3D_API_CMD_REQ_RE_FRA_RATE_INFO  0x14
#define EYS3D_API_CMD_REQ_RE_DEPTH_INFO     0x15
#define EYS3D_API_CMD_REQ_OPEN_VIDEO        0x21
#define EYS3D_API_CMD_REQ_COLSE_VIDEO       0x22
#define EYS3D_API_CMD_REQ_OPEN_DEPTH        0x23
#define EYS3D_API_CMD_REQ_COLSE_DEPTH       0x24
#define EYS3D_API_CMD_REQ_CHK_VID_STAT      0x25
#define EYS3D_API_CMD_REQ_GET_SP_LIST       0x26
#define EYS3D_API_CMD_REQ_GET_PROP_BAR      0x27
#define EYS3D_API_CMD_REQ_SET_PROP_BAR      0x28

/* Command Data Direction */
#define EYS3D_API_CMD_DAT_DIR_NO_DAT        (0x00 << 6)
#define EYS3D_API_CMD_DAT_DIR_RD            (0x01 << 6)
#define EYS3D_API_CMD_DAT_DIR_WR            (0x02 << 6)

/* Command Type = Command Requset | Command Data Direction */
#define EYS3D_API_CMD_TP_DEV_REQ_IO_NO_DAT  (EYS3D_API_CMD_REQ_DEV_IO_REQ | EYS3D_API_CMD_DAT_DIR_NO_DAT)
#define EYS3D_API_CMD_TP_DEV_IO_REQ_RD      (EYS3D_API_CMD_REQ_DEV_IO_REQ | EYS3D_API_CMD_DAT_DIR_RD)
#define EYS3D_API_CMD_TP_DEV_IO_REQ_WR      (EYS3D_API_CMD_REQ_DEV_IO_REQ | EYS3D_API_CMD_DAT_DIR_WR)

#define EYS3D_API_CMD_TP_CHK_VID_STAT_RD    (EYS3D_API_CMD_REQ_CHK_VID_STAT | EYS3D_API_CMD_DAT_DIR_RD)

#define EYS3D_API_CMD_TP_DEV_IO_DAT_SYNC    (EYS3D_API_CMD_REQ_DEV_IO_DAT_SYNC | EYS3D_API_CMD_DAT_DIR_NO_DAT)
#define EYS3D_API_CMD_TP_OPEN_VIDEO         (EYS3D_API_CMD_REQ_OPEN_VIDEO | EYS3D_API_CMD_DAT_DIR_NO_DAT)
#define EYS3D_API_CMD_TP_COLSE_VIDEO        (EYS3D_API_CMD_REQ_COLSE_VIDEO | EYS3D_API_CMD_DAT_DIR_NO_DAT)

#define EYS3D_API_CMD_TP_GET_SP_LIST        (EYS3D_API_CMD_REQ_GET_SP_LIST | EYS3D_API_CMD_DAT_DIR_RD)
#define EYS3D_API_CMD_TP_GET_PROP_BAR       (EYS3D_API_CMD_REQ_GET_PROP_BAR | EYS3D_API_CMD_DAT_DIR_RD)
#define EYS3D_API_CMD_TP_SET_PROP_BAR       (EYS3D_API_CMD_REQ_SET_PROP_BAR | EYS3D_API_CMD_DAT_DIR_WR)

/* Command Memory Type */
#define EYS3D_API_IO_REQ_MEM_TP_FW_REG      0x00 // FW Register
#define EYS3D_API_IO_REQ_MEM_TP_FW_SRAM     0x01 // FW SRAM/HW Register
#define EYS3D_API_IO_REQ_MEM_TP_I2C         0x02 // I2C
#define EYS3D_API_IO_REQ_MEM_TP_SPI_FLASH   0x03 // SPI-Flash
#define EYS3D_API_IO_REQ_MEM_TP_PIN_DAT     0x04 // Plugin Data
#define EYS3D_API_IO_REQ_MEM_TP_FS_TABLE    0x05 // Filesystem Table
#define EYS3D_API_IO_REQ_MEM_TP_VID_INFO    0x06 // Video Information
#define EYS3D_API_IO_REQ_MEM_TP_USR_AREA    0x07 // Flash User Area

#define EYS3D_API_PROP_ID_CT                0x01
#define EYS3D_API_PROP_ID_PU                0x03

/* Check Video State */
#define EYS3D_API_CHK_VID_STAT_BUSY_BIT     0x80
#define EYS3D_API_CHK_VID_STAT_DMON_BIT     0x02
#define EYS3D_API_CHK_VID_STAT_VIDEOON_BIT  0x01
#define EYS3D_API_CHK_VID_STAT_NO_BUSY      0x00
#define EYS3D_API_CHK_VID_STAT_BUSY         0x01

/* For Grape Select Device */
#define EYS3D_API_FW_REG_SEL_DEV            0x0F
#define EYS3D_API_FW_REG_SEL_DEV0           0x01
#define EYS3D_API_FW_REG_SEL_DEV1           0x02
#define EYS3D_API_FW_REG_SEL_MONO_DEV       0x03

/* For Grape Enable SensorIF */
#define EYS3D_API_HW_REG_SENSORIF           0xF100
#define EYS3D_API_HW_REG_SENSORIF_ENABLE    0x10

#define ENTER_CS do { \
   if (priv->is_initialed) mutex_lock(priv->eys3d_api_mutex); \
} while(0);

#define LEAVE_CS do { \
   if (priv->is_initialed) mutex_unlock(priv->eys3d_api_mutex); \
} while(0);

#define I2C_ERR_HANDLE(ret, ret_line) do { \
    if ((ret == -EINVAL) || (ret == -EIO)) { \
        EYS3D_ERR_DBG_PRT("%s: I/O ERR(%d)", fd->adapter->name, ret); \
        ret_line = __LINE__; \
        goto exit; \
    } \
} while(0);

#if defined(EYS3D_FOR_NVIDIA_TX2)
#include <media/tegra-v4l2-camera.h>
#include <media/tegracam_core.h>
#include <media/camera_common.h>

#define sd_to_eys3d(sd) ({ \
    struct camera_common_data *s_data = container_of(sd, struct camera_common_data, subdev); \
    (struct eys3d *)s_data->priv; \
})

struct eys3d {
    struct i2c_client           *client;
    struct v4l2_subdev          *subdev;
    struct media_pad            pad;
    struct v4l2_ctrl_handler    ctrl_handler;
    struct mutex                eys3d_mutex;
    bool                        streaming;
    struct camera_common_i2c    i2c_dev;
    struct camera_common_data   *s_data;
    struct tegracam_device      *tc_dev;
    struct eys3d_api_data       *data;
};
#else
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#define sd_to_eys3d(sd) container_of(sd, struct eys3d, subdev)

struct eys3d {
    struct i2c_client           *client;
    struct v4l2_subdev          subdev; //With subdev, use container_of to get the 'struct eys3d' object. The 'subdev' should not be pointer.
    struct media_pad            pad;
    struct v4l2_ctrl_handler    ctrl_handler;
    struct mutex                eys3d_mutex;
    struct v4l2_rect            crop;  /* Sensor window */
    struct v4l2_mbus_framefmt   format;
    bool                        streaming;
    struct eys3d_api_data       *data;
};

struct camera_common_frmfmt {
    struct v4l2_frmsize_discrete size;
    const int *framerates;
    int num_framerates;
    bool hdr_en;
    int mode;
};
#endif

struct eys3d_i2c_info {
    int nr;
    uint16_t addr;
};

struct eys3d_uvc_ctrl {
    uint8_t id;
    uint8_t val_len;
};

struct eys3d_video_mode_param {
    uint8_t cmdOpt_02;
    uint8_t cmdOpt_03;
    uint8_t cmdOpt_04;
    uint8_t cmdOpt_05;
};

struct eys3d_video_mode {
    uint32_t mode_index;
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t depth_bits;
    uint32_t color_bits;
    bool is_rectify;
    bool is_interleave_mode;
    uint32_t pixelcode;
    struct eys3d_video_mode_param param;
    uint32_t support_fps[24];
    uint32_t support_fps_num;
    bool is_scale_down;
};

struct eys3d_api_data {
    uint16_t camera_pid;
    uint16_t camera_vid;

    bool mipi_out_en;
    bool stream_on;

    struct mutex *eys3d_api_mutex;
    bool is_initialed;

    int cur_error_code;

    bool is_rd_asic_reg;
    uint16_t cur_asic_reg;

    bool is_rd_fw_reg;
    uint8_t cur_fw_reg;

    bool is_rd_pb;
    uint8_t cur_pb_ct_pu_id;
    uint8_t cur_pb_type;
    uint8_t cur_pb_ctrsel;

    uint8_t cur_fs_id;
    uint32_t cur_fs_offset;
    uint16_t cur_asic_id;

    bool is_rd_i2c_reg;
    uint8_t cur_i2_slave_id;
    uint8_t cur_i2_format;
    uint16_t cur_i2_addr;

    bool is_enable_version_check;
    uint32_t cur_video_mode_index;
    uint32_t pre_video_mode_index;
    bool is_first_set_video_mode;

    struct sys_bin_data *fs_bin;
    struct sys_bin_data *flash_bin;
    struct sys_bin_data *asic_reg_bin;

    const struct eys3d_video_mode *supported_video_modes;
    uint32_t supported_video_modes_num;

    bool is_opened;
    
    uint8_t vc_id;
    struct i2c_client *client;
};

struct sys_bin_data {
    struct bin_attribute bin;
    struct mutex buffer_lock;
    uint8_t buffer[EYS3D_API_FS_BIN_BUF_SZ];
};

void eys3d_sleep(unsigned int ms);

int eys3d_rd_reg(struct i2c_client* client, uint16_t reg, uint8_t *val);
int eys3d_wr_reg(struct i2c_client* client, uint16_t reg, uint8_t val);

int eys3d_api_start_stream(struct eys3d *driv);
int eys3d_api_stop_stream(struct eys3d *driv);
const struct eys3d_video_mode *eys3d_api_get_current_video_mode(struct eys3d *driv);
const struct eys3d_video_mode *eys3d_api_get_video_mode_by_index(struct eys3d *driv, uint32_t index);
int eys3d_api_eys3d_set_current_vid_mode(struct eys3d *driv, uint32_t width, uint32_t height);
int eys3d_api_init(struct eys3d *driv);
int eys3d_api_rls(struct eys3d *driv);
#endif
