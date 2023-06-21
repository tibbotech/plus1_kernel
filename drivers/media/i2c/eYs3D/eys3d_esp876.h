#ifndef __EYS3D_ESP876_H__
#define __EYS3D_ESP876_H__

int esp876_get_pid_vid(struct i2c_client* client, uint16_t *pid, uint16_t *vid);
int esp876_get_fw_ver(struct i2c_client* client, char *version);
int esp876_enable_mipi_output(struct i2c_client* client, bool en);

int esp876_video_switch(struct i2c_client* client, uint32_t mode_index);
int esp876_video_close(struct i2c_client* client);
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
int esp876_rd_fw_reg(struct i2c_client* client, uint8_t reg, uint8_t *val);
int esp876_wr_fw_reg(struct i2c_client* client, uint8_t reg, uint8_t val);

int esp876_rd_asic_reg(struct i2c_client* client, uint16_t reg, uint8_t *data, uint16_t len);
int esp876_wr_asic_reg(struct i2c_client* client, uint16_t reg, uint8_t *data, uint16_t len);

int esp876_rd_fd(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t *data, uint16_t len);
int esp876_wr_fd(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t *data, uint16_t len);
int esp876_sync_fd(struct i2c_client* client);

int esp876_rd_fw_i2c_reg(struct i2c_client* client, uint8_t id, uint8_t format, uint16_t addr, uint16_t *data);
int esp876_wr_fw_i2c_reg(struct i2c_client* client, uint8_t id, uint8_t format, uint16_t addr, uint16_t data);

/*
 * ct_pu_id : 0x01: CT, 0x03: PU
 * type: same as UVC
 * ctrsel: same as UVC
 */
int esp876_wr_prop_bar(struct i2c_client* client, uint8_t id, uint8_t type, uint8_t ctrsel, uint32_t val);
int esp876_rd_prop_bar(struct i2c_client* client, uint8_t id, uint8_t type, uint8_t ctrsel, uint32_t *val);
int esp876_rd_prop_bar_sp_list(struct i2c_client* client, uint8_t id, uint16_t *val);

int esp876_rd_fw_data(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t* data, uint16_t len);
int esp876_wr_fw_data(struct i2c_client* client, uint8_t type, uint8_t id, uint32_t offset, uint8_t* data, uint16_t len);

int eys3d_esp876_start_stream(struct eys3d *driv);
int eys3d_esp876_stop_stream(struct eys3d *driv);
#endif
