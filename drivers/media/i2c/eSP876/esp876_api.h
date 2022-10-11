#ifndef    __ESP876_API_H__
#define    __ESP876_API_H__

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/media-bus-format.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include "esp876_camera_hw.h"
#include "esp876_version.h"

#define ESP876_ALERT_DEBUG_ENABLE   1
#define ESP876_ERROR_DEBUG_ENABLE   1
//#define ESP876_API_DEBUG_ENABLE     1
//#define ESP876_API_I2C_DEBUG_ENABLE 1

#define my_libc_api_printf pr_info

#define DEBUG_PRINTF(format, ...) \
    my_libc_api_printf("[eYs3D_eSP876][%s][%d]"format, __func__, __LINE__, ##__VA_ARGS__)

#ifdef ESP876_ALERT_DEBUG_ENABLE
#define ESP876_ALERT_DEBUG DEBUG_PRINTF
#else
#define ESP876_ALERT_DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef ESP876_ERROR_DEBUG_ENABLE
#define ESP876_ERR_DEBUG DEBUG_PRINTF
#else
#define ESP876_ERR_DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef ESP876_API_DEBUG_ENABLE
#define ESP876_API_DEBUG DEBUG_PRINTF
#else
#define ESP876_API_DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef ESP876_API_I2C_DEBUG_ENABLE
#define ESP876_API_I2C_DEBUG DEBUG_PRINTF
#else
#define ESP876_API_I2C_DEBUG(fmt, args...) do {} while (0)
#endif

#define ESP876_API_VERSION_INFO_PROCFS_NODE         "esp876_version_info"
#define ESP876_API_I2C_INFO_PROCFS_NODE             "esp876_i2c_info"
#define FLASH_BIN_FILE_NAME                         "flash_data"
#define FS_BIN_FILE_NAME                            "fs_data"
#define ASIC_REG_BIN_FILE_NAME                      "asic_data"

#define ESP876_API_CAMERA_PID_UNKOWN                0xffff
#define ESP876_API_CAMERA_PID_SANDRA                0x0167
#define ESP876_API_CAMERA_PID_NORA                  0x0168
#define ESP876_API_CAMERA_PID_EX8036                0x0120
#define ESP876_API_CAMERA_PID_HELEN                 0x0171

#define ESP876_API_CAMERA_VID_UNKOWN                0xffff
#define ESP876_API_CAMERA_VID_EEVER                 0x1e4e
#define ESP876_API_CAMERA_VID_EYS3D                 0x3438
#define ESP876_API_CAMERA_VID_EX8036                0x1e4e

#define ESP876_API_SUPPORTED_DEV_COUNT              2
#define ESP876_API_I2C_ADDR                         0x0060

#define FS_BIN_BUFFER_SIZE                          (MAX_FS_BUF_SIZE * 1024)

#define ESP876_API_FW_CMD_TYPE_VIDEO_MODE           0x21
#define ESP876_API_FW_CMD_TYPE_VIDEO_CLOSE          0x22
#define ESP876_API_FW_CMD_TYPE_SYNC_COMMAND         0x02
#define ESP876_API_FW_CMD_TYPE_READ_REG             0x41
#define ESP876_API_FW_CMD_TYPE_WRITE_REG            0x81
#define ESP876_API_FW_CMD_TYPE_READ_PROPERTY_BAR    0x67
#define ESP876_API_FW_CMD_TYPE_WRITE_PROPERTY_BAR   0xA8
#define ESP876_API_FW_CMD_TYPE_GET_SUPPORTR_LIST    0x66

#define ESP876_API_REG_TYPE_FW_REG                  0x00
#define ESP876_API_REG_TYPE_HW_REG                  0x01
#define ESP876_API_REG_TYPE_SENSOR_REG              0x02

#define ESP876_API_CT_PROPERTY_ID                   0x01
#define ESP876_API_PU_PROPERTY_ID                   0x03

#define ESP876_API_FW_FS_MEM_TYPE_FW_REGISTER       0x00 // FW Register
#define ESP876_API_FW_FS_MEM_TYPE_FW_SRAM           0x01 // FW SRAM
#define ESP876_API_FW_FS_MEM_TYPE_I2C               0x02 // I2C
#define ESP876_API_FW_FS_MEM_TYPE_SPI_FLASH         0x03 // SPI-Flash
#define ESP876_API_FW_FS_MEM_TYPE_PLUGIN_DATA       0x04 // Plugin Data
#define ESP876_API_FW_FS_MEM_TYPE_FS_TABLE          0x05 // Filesystem Table
#define ESP876_API_FW_FS_MEM_TYPE_VIDEO_INFO        0x06 // Video Information
#define ESP876_API_FW_FS_MEM_TYPE_FLASH_USER_AREA   0x07 // Flash User Area

#if defined(ESP876_FOR_NVIDIA_TX2)
#include <media/tegra-v4l2-camera.h>
#include <media/tegracam_core.h>
#include <media/camera_common.h>

#define sd_to_esp876(sd) ({ \
    struct camera_common_data *s_data = container_of(sd, struct camera_common_data, subdev);\
    (struct esp876 *)s_data->priv;\
})\

struct esp876 {
    struct i2c_client           *client;
    struct v4l2_subdev          *subdev;
    struct media_pad            pad;
    struct v4l2_ctrl_handler    ctrl_handler;
    struct mutex                mutex;
    bool                        streaming;
    struct camera_common_i2c    i2c_dev;
    struct camera_common_data   *s_data;
    struct tegracam_device      *tc_dev;
    struct esp876_api_data      *data;

};
#else
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#define sd_to_esp876(sd) container_of(sd, struct esp876, subdev)

struct esp876 {
    struct i2c_client           *client;
    struct v4l2_subdev          subdev; //With subdev, use container_of to get the 'struct esp876' object. The 'subdev' should not be pointer.
    struct media_pad            pad;
    struct v4l2_ctrl_handler    ctrl_handler;
    struct mutex                mutex;
    struct v4l2_rect            crop;  /* Sensor window */
    struct v4l2_mbus_framefmt   format;
    bool                        streaming;
    struct esp876_api_data      *data;
};

struct camera_common_frmfmt {
    struct v4l2_frmsize_discrete size;
    const int *framerates;
    int num_framerates;
    bool hdr_en;
    int mode;
};
#endif

struct esp876_i2c_info {
    int nr;
    uint16_t addr;
};

struct esp876_uvc_ctrl {
    uint8_t id;
    uint8_t val_len;
};

struct esp876_video_mode_param {
    uint8_t cmdOpt_02;
    uint8_t cmdOpt_03;
    uint8_t cmdOpt_04;
    uint8_t cmdOpt_05;
};

struct esp876_video_mode {
    uint32_t mode_index;
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t depth_bits;
    uint32_t color_bits;
    bool is_rectify;
    bool is_interleave_mode;
    uint32_t pixelcode;
    struct esp876_video_mode_param param;
    uint32_t support_fps[24];
    uint32_t support_fps_num;
    bool is_scale_down;
};

struct esp876_api_data {

    uint16_t camera_pid;
    uint16_t camera_vid;

    bool mipi_out_en;
    bool stream_on;

    struct mutex mutex;
    bool is_initialed;

    int cur_error_code;

    bool is_read_asic_reg;
    uint16_t cur_asic_reg;

    bool is_read_fw_reg;
    uint8_t cur_fw_reg;

    bool is_read_pb;
    uint8_t cur_pb_ct_pu_id;
    uint8_t cur_pb_type;
    uint8_t cur_pb_ctrsel;

    uint8_t cur_fs_id;
    uint32_t cur_fs_offset;
    uint16_t cur_asic_id;

    bool is_read_i2c_reg;
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

    const struct esp876_video_mode *supported_video_modes;
    uint32_t supported_video_modes_num;

    bool is_opened;

};

int esp876_api_init(struct esp876 *driv);
int esp876_api_set_current_video_mode(struct esp876 *driv, uint32_t width, uint32_t height);
const struct esp876_video_mode *esp876_api_get_current_video_mode(struct esp876 *driv);
const struct esp876_video_mode *esp876_api_get_video_mode_by_index(struct esp876 *driv, uint32_t index);
int esp876_api_stop_stream(struct esp876 *driv);
int esp876_api_start_stream(struct esp876 *driv);
int esp876_api_rls(struct esp876 *driv);
#endif
