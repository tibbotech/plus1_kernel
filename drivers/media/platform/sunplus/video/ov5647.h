#ifndef __OV5647_H__
#define __OV5647_H__

#if 0
#define FUNC_DEBUG()                            printk(KERN_INFO "[OV5647] %s (L:%d)\n", __FUNCTION__, __LINE__)
#else
#define FUNC_DEBUG()
#endif
#define DBG_INFO(fmt, args ...)                 printk(KERN_INFO "[OV5647] " fmt, ## args)
#define DBG_ERR(fmt, args ...)                  printk(KERN_ERR "[OV5647] ERR: " fmt, ## args)

#define REG_NULL                                0xFFFF

/* OV5647 Registers */
#define OV5647_REG_CHIP_ID                      0x300A
#define CHIP_ID                                 0x5647

#define OV5647_REG_TIMING_X_OUTPUT_SIZE         0x3808
#define OV5647_REG_TIMING_Y_OUTPUT_SIZE         0x380A
#define OV5647_REG_ISP_CTRL3D                   0x503D

#define OV5647_SW_STANDBY                       0x0100
#define OV5647_MODE_SW_STANDBY                  0x0
#define OV5647_MODE_STREAMING                   BIT(0)

#define OV5647_REG_VALUE_08BIT                  1
#define OV5647_REG_VALUE_16BIT                  2
#define OV5647_REG_VALUE_24BIT                  3


#define OV5647_SW_RESET                         0x0103
#define OV5640_REG_PAD_OUT                      0x300D
#define OV5647_REG_FRAME_OFF_NUMBER             0x4202
#define OV5647_REG_MIPI_CTRL00                  0x4800
#define OV5647_REG_MIPI_CTRL14                  0x4814

#define MIPI_CTRL00_CLOCK_LANE_GATE             BIT(5)
#define MIPI_CTRL00_LINE_SYNC_ENABLE            BIT(4)
#define MIPI_CTRL00_BUS_IDLE                    BIT(2)
#define MIPI_CTRL00_CLOCK_LANE_DISABLE          BIT(0)

#define to_ov5647(sd)                           container_of(sd, struct ov5647, subdev)


enum ov5647_mode_id {
	OV5647_MODE_VGA_640_480  = 0,
	OV5647_MODE_720P_1280_720,
	OV5647_MODE_960P_1280_960,
	OV5647_MODE_QSXGA_2592_1944,
	OV5647_NUM_MODES,
};

enum ov5647_frame_rate {
	OV5647_15_FPS = 0,
	OV5647_30_FPS,
	OV5647_60_FPS,
	OV5647_NUM_FRAMERATES,
};

/*
 * Image size under 1280 * 960 are SUBSAMPLING
 * Image size upper 1280 * 960 are SCALING
 */
enum ov5647_downsize_mode {
	SUBSAMPLING,
	SCALING,
};


struct regval {
	u16     addr;
	u8      val;
};

struct ov5647_mode_info {
	enum ov5647_mode_id             id;
	enum ov5647_downsize_mode       dn_mode;
	u32                             hact;       // Active horizontal pixels
	u32                             htot;       // Total horizontal pixels
	u32                             vact;       // Active vertical pixels
	u32                             vtot;       // Total vertical pixels
	const struct regval             *reg_data;
	u32                             reg_data_size;
};

struct ov5647 {
	//struct i2c_client               *client;

	//struct gpio_desc                *reset_gpio;
	//struct gpio_desc                *pwdn_gpio;
	//struct gpio_desc                *pwr_gpio;
	//struct gpio_desc                *pwr_snd_gpio;
	//struct pinctrl                  *pinctrl;
	//struct pinctrl_state            *pins_default;
	//struct pinctrl_state            *pins_sleep;

	//struct v4l2_subdev              subdev;
	struct v4l2_ctrl_handler        ctrl_handler;
	//struct v4l2_ctrl                *exposure;
	//struct v4l2_ctrl                *anal_gain;
	//struct v4l2_ctrl                *digi_gain;
	//struct v4l2_ctrl                *hblank;
	//struct v4l2_ctrl                *vblank;
	//struct v4l2_ctrl                *test_pattern;

	//struct mutex                    mutex;
	//bool                            streaming;

	//const struct ov5647_mode        *cur_mode;
	//struct sp_subdev_sensor_data    sensor_data;


	/* 
	 * from ov5640 driver
	 */
	struct i2c_client               *i2c_client;
	struct v4l2_subdev              subdev;
	//struct media_pad pad;
	//struct v4l2_fwnode_endpoint ep; /* the parsed DT endpoint info */
	//struct clk *xclk; /* system clock to OV5640 */
	//u32 xclk_freq;

	//struct regulator_bulk_data supplies[OV5640_NUM_SUPPLIES];
	//struct gpio_desc *reset_gpio;
	//struct gpio_desc *pwdn_gpio;
	//bool   upside_down;

	/* lock to protect all members below */
	struct mutex                    lock;

	//int power_count;

	struct v4l2_mbus_framefmt       fmt;
	bool                            pending_fmt_change;

	const struct ov5647_mode_info   *current_mode;
	const struct ov5647_mode_info   *last_mode;
	enum ov5647_frame_rate          current_fr;
	struct v4l2_fract               frame_interval;

	//struct ov5640_ctrls ctrls;

	//u32 prev_sysclk, prev_hts;
	//u32 ae_low, ae_high, ae_target;

	bool                            pending_mode_change;
	bool							streaming;
};

#endif
