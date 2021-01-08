#ifndef __VEYE290_H__
#define __VEYE290_H__

#if 0
#define FUNC_DEBUG()                            printk(KERN_INFO "[VEYE290] %s (L:%d)\n", __FUNCTION__, __LINE__)
#else
#define FUNC_DEBUG()
#endif
#define DBG_INFO(fmt, args ...)                 printk(KERN_INFO "[VEYE290] " fmt, ## args)
#define DBG_ERR(fmt, args ...)                  printk(KERN_ERR "[VEYE290] ERR: " fmt, ## args)

#define REG_NULL                                0xFFFF

/* VEYE290 Registers */
#define VEYE290_REG_CHIP_ID                     0x0001
#define CHIP_ID                                 0x06
#define VEYE290_REG_CTRL_MODE                   0x000B
#define VEYE290_MODE_SW_STANDBY                 0x0
#define VEYE290_MODE_STREAMING                  BIT(0)

#define VEYE290_REG_VALUE_08BIT                 1
#define VEYE290_REG_VALUE_16BIT                 2
#define VEYE290_REG_VALUE_24BIT                 3

#define to_veye290(sd)                          container_of(sd, struct veye290, subdev)


enum veye290_mode_id {
	VEYE290_MODE_1080P_1920_1080  = 0,
	VEYE290_NUM_MODES,
};

enum veye290_frame_rate {
	VEYE290_30_FPS = 0,
	VEYE290_NUM_FRAMERATES,
};

/*
 * Image size under 1280 * 960 are SUBSAMPLING
 * Image size upper 1280 * 960 are SCALING
 */
enum veye290_downsize_mode {
	SUBSAMPLING,
	SCALING,
};


struct regval {
	u16     addr;
	u8      val;
};

struct veye290_mode_info {
	enum veye290_mode_id            id;
	enum veye290_downsize_mode      dn_mode;
	u32                             hact;       // Active horizontal pixels
	u32                             htot;       // Total horizontal pixels
	u32                             vact;       // Active vertical pixels
	u32                             vtot;       // Total vertical pixels
};

struct veye290 {
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

	//const struct veye290_mode        *cur_mode;
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

	const struct veye290_mode_info  *current_mode;
	const struct veye290_mode_info  *last_mode;
	enum veye290_frame_rate         current_fr;
	struct v4l2_fract               frame_interval;

	//struct ov5640_ctrls ctrls;

	//u32 prev_sysclk, prev_hts;
	//u32 ae_low, ae_high, ae_target;

	bool                            pending_mode_change;
	bool							streaming;
};

#endif
