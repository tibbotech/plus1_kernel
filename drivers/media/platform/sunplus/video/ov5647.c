/*
 * ov5647.c - ov5647 Image Sensor Driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include "ov5647.h"

// BAYER, RAW8
static struct regval ov5647_setting_VGA_640_480[] = {
	{0x0100, 0x00},	{0x0103, 0x01},	{0x3034, 0x08},
	{0x3035, 0x21},	{0x3036, 0x46},	{0x303c, 0x11},
	{0x3106, 0xf5},	{0x3821, 0x07},	{0x3820, 0x41},
	{0x3827, 0xec},	{0x370c, 0x0f},	{0x3612, 0x59},
	{0x3618, 0x00},	{0x5000, 0x06},	{0x5001, 0x01},
	{0x5002, 0x41},	{0x5003, 0x08},	{0x5a00, 0x08},
	{0x3000, 0x00},	{0x3001, 0x00},	{0x3002, 0x00},
	{0x3016, 0x08},	{0x3017, 0xe0},	{0x3018, 0x44},
	{0x301c, 0xf8},	{0x301d, 0xf0},	{0x3a18, 0x00},
	{0x3a19, 0xf8},	{0x3c01, 0x80},	{0x3b07, 0x0c},
	{0x380c, 0x07},	{0x380d, 0x68},	{0x380e, 0x03},
	{0x380f, 0xd8},	{0x3814, 0x31},	{0x3815, 0x31},
	{0x3708, 0x64},	{0x3709, 0x52},	{0x3808, 0x02},
	{0x3809, 0x80},	{0x380a, 0x01},	{0x380b, 0xE0},
	{0x3800, 0x00},	{0x3801, 0x00},	{0x3802, 0x00},	
	{0x3803, 0x00},	{0x3804, 0x0a},	{0x3805, 0x3f},	
	{0x3806, 0x07},	{0x3807, 0xa1},	{0x3811, 0x08},	
	{0x3813, 0x02},	{0x3630, 0x2e},	{0x3632, 0xe2},	
	{0x3633, 0x23},	{0x3634, 0x44},	{0x3636, 0x06},	
	{0x3620, 0x64},	{0x3621, 0xe0},	{0x3600, 0x37},	
	{0x3704, 0xa0},	{0x3703, 0x5a},	{0x3715, 0x78},	
	{0x3717, 0x01},	{0x3731, 0x02},	{0x370b, 0x60},	
	{0x3705, 0x1a},	{0x3f05, 0x02},	{0x3f06, 0x10},	
	{0x3f01, 0x0a},	{0x3a08, 0x01},	{0x3a09, 0x27},	
	{0x3a0a, 0x00},	{0x3a0b, 0xf6},	{0x3a0d, 0x04},	
	{0x3a0e, 0x03},	{0x3a0f, 0x58},	{0x3a10, 0x50},	
	{0x3a1b, 0x58},	{0x3a1e, 0x50},	{0x3a11, 0x60},	
	{0x3a1f, 0x28},	{0x4001, 0x02},	{0x4004, 0x02},	
	{0x4000, 0x09},	{0x4837, 0x24},	{0x4050, 0x6e},	
	{0x4051, 0x8f},	{0x0100, 0x01},	{REG_NULL, 0x00}
};

static struct regval ov5647_setting_720P_1280_720 [] = {
	{0x0100, 0x00},	{0x0103, 0x01},	{0x3035, 0x11}, 
	{0x303c, 0x11}, {0x370c, 0x03}, {0x5000, 0x06}, 
	{0x5003, 0x08}, {0x5a00, 0x08}, {0x3000, 0xff}, 
	{0x3001, 0xff}, {0x3002, 0x00}, {0x301d, 0xf0}, 
	{0x3a18, 0x00}, {0x3a19, 0xf8}, {0x3c01, 0x80}, 
	{0x3b07, 0x0c}, {0x3708, 0x64}, {0x3630, 0x2e}, 
	{0x3632, 0xe2}, {0x3633, 0x23}, {0x3634, 0x44}, 
	{0x3620, 0x64}, {0x3621, 0xe0}, {0x3600, 0x37}, 
	{0x3704, 0xa0}, {0x3703, 0x5a}, {0x3715, 0x78}, 
	{0x3717, 0x01}, {0x3731, 0x02}, {0x370b, 0x60}, 
	{0x3705, 0x1a}, {0x3f05, 0x02}, {0x3f06, 0x10}, 
	{0x3f01, 0x0a}, {0x3a08, 0x01}, {0x3a0f, 0x58}, 
	{0x3a10, 0x50}, {0x3a1b, 0x58}, {0x3a1e, 0x50}, 
	{0x3a11, 0x60}, {0x3a1f, 0x28}, {0x4001, 0x02}, 
	{0x4000, 0x09}, {0x3000, 0x00}, {0x3001, 0x00}, 
	{0x3002, 0x00}, {0x3017, 0xe0}, {0x301c, 0xfc}, 
	{0x3636, 0x06}, {0x3016, 0x08}, {0x3827, 0xec}, 
	{0x3018, 0x44}, {0x3035, 0x21}, {0x3106, 0xf5}, 
	{0x3034, 0x18}, {0x301c, 0xf8}, 
	/*lens setting*/ 
	{0x5000, 0x86}, {0x5800, 0x11}, {0x5801, 0x0c}, 
	{0x5802, 0x0a}, {0x5803, 0x0b}, {0x5804, 0x0d}, 
	{0x5805, 0x13}, {0x5806, 0x09}, {0x5807, 0x05}, 
	{0x5808, 0x03}, {0x5809, 0x03}, {0x580a, 0x06}, 
	{0x580b, 0x08}, {0x580c, 0x05}, {0x580d, 0x01}, 
	{0x580e, 0x00}, {0x580f, 0x00}, {0x5810, 0x02}, 
	{0x5811, 0x06}, {0x5812, 0x05}, {0x5813, 0x01}, 
	{0x5814, 0x00}, {0x5815, 0x00}, {0x5816, 0x02}, 
	{0x5817, 0x06}, {0x5818, 0x09}, {0x5819, 0x05}, 
	{0x581a, 0x04}, {0x581b, 0x04}, {0x581c, 0x06}, 
	{0x581d, 0x09}, {0x581e, 0x11}, {0x581f, 0x0c}, 
	{0x5820, 0x0b}, {0x5821, 0x0b}, {0x5822, 0x0d}, 
	{0x5823, 0x13}, {0x5824, 0x22}, {0x5825, 0x26}, 
	{0x5826, 0x26}, {0x5827, 0x24}, {0x5828, 0x24}, 
	{0x5829, 0x24}, {0x582a, 0x22}, {0x582b, 0x20}, 
	{0x582c, 0x22}, {0x582d, 0x26}, {0x582e, 0x22}, 
	{0x582f, 0x22}, {0x5830, 0x42}, {0x5831, 0x22}, 
	{0x5832, 0x02}, {0x5833, 0x24}, {0x5834, 0x22}, 
	{0x5835, 0x22}, {0x5836, 0x22}, {0x5837, 0x26}, 
	{0x5838, 0x42}, {0x5839, 0x26}, {0x583a, 0x06}, 
	{0x583b, 0x26}, {0x583c, 0x24}, {0x583d, 0xce}, 
	/* manual AWB,manual AE,close Lenc,open WBC*/ 
	{0x3503, 0x03}, {0x3501, 0x10}, {0x3502, 0x80}, 
	{0x350a, 0x00}, {0x350b, 0x7f}, {0x5001, 0x01},
	{0x5180, 0x08}, {0x5186, 0x04}, {0x5187, 0x00}, 
	{0x5188, 0x04}, {0x5189, 0x00}, {0x518a, 0x04}, 
	{0x518b, 0x00}, {0x5000, 0x06},                                                              
	/*1280*720 Reference Setting 24M MCLK 2lane 280Mbps/lane 30fps for back to preview*/
	/*Display Out setting : 0x3808~0x3809 (Width)	/0x380A~0x380B (Height)*/                                      
	{0x3035, 0x21}, {0x3036, 0x37}, {0x3821, 0x07},
	{0x3820, 0x41}, {0x3612, 0x09}, {0x3618, 0x00},
	{0x380c, 0x07}, {0x380d, 0x68}, {0x380e, 0x03},
	{0x380f, 0xd8}, {0x3814, 0x31}, {0x3815, 0x31},
	{0x3709, 0x52}, {0x3808, 0x05}, {0x3809, 0x00},
	{0x380a, 0x02}, {0x380b, 0xd0}, {0x3800, 0x00},
	{0x3801, 0x18}, {0x3802, 0x00}, {0x3803, 0x0e},
	{0x3804, 0x0a}, {0x3805, 0x27}, {0x3806, 0x07},
	{0x3807, 0x95}, {0x4004, 0x02}, {0x0100, 0x01},
	{REG_NULL, 0x00}
}; 

static struct regval ov5647_setting_960P_1280_960 [] = {
	{0x0100, 0x00},	{0x0103, 0x01},	{0x3035, 0x11}, 
	{0x303c, 0x11}, {0x370c, 0x03}, {0x5000, 0x06}, 
	{0x5003, 0x08}, {0x5a00, 0x08}, {0x3000, 0xff}, 
	{0x3001, 0xff}, {0x3002, 0x00}, {0x301d, 0xf0}, 
	{0x3a18, 0x00}, {0x3a19, 0xf8}, {0x3c01, 0x80}, 
	{0x3b07, 0x0c}, {0x3708, 0x64}, {0x3630, 0x2e}, 
	{0x3632, 0xe2}, {0x3633, 0x23}, {0x3634, 0x44}, 
	{0x3620, 0x64}, {0x3621, 0xe0}, {0x3600, 0x37}, 
	{0x3704, 0xa0}, {0x3703, 0x5a}, {0x3715, 0x78}, 
	{0x3717, 0x01}, {0x3731, 0x02}, {0x370b, 0x60}, 
	{0x3705, 0x1a}, {0x3f05, 0x02}, {0x3f06, 0x10}, 
	{0x3f01, 0x0a}, {0x3a08, 0x01}, {0x3a0f, 0x58}, 
	{0x3a10, 0x50}, {0x3a1b, 0x58}, {0x3a1e, 0x50}, 
	{0x3a11, 0x60}, {0x3a1f, 0x28}, {0x4001, 0x02}, 
	{0x4000, 0x09}, {0x3000, 0x00}, {0x3001, 0x00}, 
	{0x3002, 0x00}, {0x3017, 0xe0}, {0x301c, 0xfc}, 
	{0x3636, 0x06}, {0x3016, 0x08}, {0x3827, 0xec}, 
	{0x3018, 0x44}, {0x3035, 0x21}, {0x3106, 0xf5}, 
	{0x3034, 0x18}, {0x301c, 0xf8}, 
	/*lens setting*/ 
	{0x5000, 0x86}, {0x5800, 0x11}, {0x5801, 0x0c}, 
	{0x5802, 0x0a}, {0x5803, 0x0b}, {0x5804, 0x0d}, 
	{0x5805, 0x13}, {0x5806, 0x09}, {0x5807, 0x05}, 
	{0x5808, 0x03}, {0x5809, 0x03}, {0x580a, 0x06}, 
	{0x580b, 0x08}, {0x580c, 0x05}, {0x580d, 0x01}, 
	{0x580e, 0x00}, {0x580f, 0x00}, {0x5810, 0x02}, 
	{0x5811, 0x06}, {0x5812, 0x05}, {0x5813, 0x01}, 
	{0x5814, 0x00}, {0x5815, 0x00}, {0x5816, 0x02}, 
	{0x5817, 0x06}, {0x5818, 0x09}, {0x5819, 0x05}, 
	{0x581a, 0x04}, {0x581b, 0x04}, {0x581c, 0x06}, 
	{0x581d, 0x09}, {0x581e, 0x11}, {0x581f, 0x0c}, 
	{0x5820, 0x0b}, {0x5821, 0x0b}, {0x5822, 0x0d}, 
	{0x5823, 0x13}, {0x5824, 0x22}, {0x5825, 0x26}, 
	{0x5826, 0x26}, {0x5827, 0x24}, {0x5828, 0x24}, 
	{0x5829, 0x24}, {0x582a, 0x22}, {0x582b, 0x20}, 
	{0x582c, 0x22}, {0x582d, 0x26}, {0x582e, 0x22}, 
	{0x582f, 0x22}, {0x5830, 0x42}, {0x5831, 0x22}, 
	{0x5832, 0x02}, {0x5833, 0x24}, {0x5834, 0x22}, 
	{0x5835, 0x22}, {0x5836, 0x22}, {0x5837, 0x26}, 
	{0x5838, 0x42}, {0x5839, 0x26}, {0x583a, 0x06}, 
	{0x583b, 0x26}, {0x583c, 0x24}, {0x583d, 0xce}, 
	/* manual AWB,manual AE,close Lenc,open WBC*/ 
	{0x3503, 0x03}, {0x3501, 0x10}, {0x3502, 0x80}, 
	{0x350a, 0x00}, {0x350b, 0x7f}, {0x5001, 0x01},
	{0x5180, 0x08}, {0x5186, 0x04}, {0x5187, 0x00}, 
	{0x5188, 0x04}, {0x5189, 0x00}, {0x518a, 0x04}, 
	{0x518b, 0x00}, {0x5000, 0x06},                                                              
	/*1280*960 Reference Setting 24M MCLK 2lane 280Mbps/lane 30fps for back to preview*/
	/*Display Out setting : 0x3808~0x3809 (Width)	/0x380A~0x380B (Height)*/                                      
	{0x3035, 0x21}, {0x3036, 0x37}, {0x3821, 0x07},
	{0x3820, 0x41}, {0x3612, 0x09}, {0x3618, 0x00},
	{0x380c, 0x07}, {0x380d, 0x68}, {0x380e, 0x03},
	{0x380f, 0xd8}, {0x3814, 0x31}, {0x3815, 0x31},
	{0x3709, 0x52}, {0x3808, 0x05}, {0x3809, 0x00},
	{0x380a, 0x03}, {0x380b, 0xc0}, {0x3800, 0x00},
	{0x3801, 0x18}, {0x3802, 0x00}, {0x3803, 0x0e},
	{0x3804, 0x0a}, {0x3805, 0x27}, {0x3806, 0x07},
	{0x3807, 0x95}, {0x4004, 0x02}, {0x0100, 0x01},
	{REG_NULL, 0x00}
}; 

static struct regval ov5647_setting_QSXGA_2592_1944 [] = {
	{0x0100, 0x00},	{0x0103, 0x01},	{0x3035, 0x11}, 
	{0x303c, 0x11}, {0x370c, 0x03}, {0x5000, 0x06}, 
	{0x5003, 0x08}, {0x5a00, 0x08}, {0x3000, 0xff}, 
	{0x3001, 0xff}, {0x3002, 0x00}, {0x301d, 0xf0}, 
	{0x3a18, 0x00}, {0x3a19, 0xf8}, {0x3c01, 0x80}, 
	{0x3b07, 0x0c}, {0x3708, 0x64}, {0x3630, 0x2e}, 
	{0x3632, 0xe2}, {0x3633, 0x23}, {0x3634, 0x44}, 
	{0x3620, 0x64}, {0x3621, 0xe0}, {0x3600, 0x37}, 
	{0x3704, 0xa0}, {0x3703, 0x5a}, {0x3715, 0x78}, 
	{0x3717, 0x01}, {0x3731, 0x02}, {0x370b, 0x60}, 
	{0x3705, 0x1a}, {0x3f05, 0x02}, {0x3f06, 0x10}, 
	{0x3f01, 0x0a}, {0x3a08, 0x01}, {0x3a0f, 0x58}, 
	{0x3a10, 0x50}, {0x3a1b, 0x58}, {0x3a1e, 0x50}, 
	{0x3a11, 0x60}, {0x3a1f, 0x28}, {0x4001, 0x02}, 
	{0x4000, 0x09}, {0x3000, 0x00}, {0x3001, 0x00}, 
	{0x3002, 0x00}, {0x3017, 0xe0}, {0x301c, 0xfc}, 
	{0x3636, 0x06}, {0x3016, 0x08}, {0x3827, 0xec}, 
	{0x3018, 0x44}, {0x3035, 0x21}, {0x3106, 0xf5}, 
	{0x3034, 0x18}, {0x301c, 0xf8}, 
	/*lens setting*/ 
	{0x5000, 0x86}, {0x5800, 0x11}, {0x5801, 0x0c}, 
	{0x5802, 0x0a}, {0x5803, 0x0b}, {0x5804, 0x0d}, 
	{0x5805, 0x13}, {0x5806, 0x09}, {0x5807, 0x05}, 
	{0x5808, 0x03}, {0x5809, 0x03}, {0x580a, 0x06}, 
	{0x580b, 0x08}, {0x580c, 0x05}, {0x580d, 0x01}, 
	{0x580e, 0x00}, {0x580f, 0x00}, {0x5810, 0x02}, 
	{0x5811, 0x06}, {0x5812, 0x05}, {0x5813, 0x01}, 
	{0x5814, 0x00}, {0x5815, 0x00}, {0x5816, 0x02}, 
	{0x5817, 0x06}, {0x5818, 0x09}, {0x5819, 0x05}, 
	{0x581a, 0x04}, {0x581b, 0x04}, {0x581c, 0x06}, 
	{0x581d, 0x09}, {0x581e, 0x11}, {0x581f, 0x0c}, 
	{0x5820, 0x0b}, {0x5821, 0x0b}, {0x5822, 0x0d}, 
	{0x5823, 0x13}, {0x5824, 0x22}, {0x5825, 0x26}, 
	{0x5826, 0x26}, {0x5827, 0x24}, {0x5828, 0x24}, 
	{0x5829, 0x24}, {0x582a, 0x22}, {0x582b, 0x20}, 
	{0x582c, 0x22}, {0x582d, 0x26}, {0x582e, 0x22}, 
	{0x582f, 0x22}, {0x5830, 0x42}, {0x5831, 0x22}, 
	{0x5832, 0x02}, {0x5833, 0x24}, {0x5834, 0x22}, 
	{0x5835, 0x22}, {0x5836, 0x22}, {0x5837, 0x26}, 
	{0x5838, 0x42}, {0x5839, 0x26}, {0x583a, 0x06}, 
	{0x583b, 0x26}, {0x583c, 0x24}, {0x583d, 0xce}, 
	/* manual AWB,manual AE,close Lenc,open WBC*/ 
	{0x3503, 0x03}, {0x3501, 0x10}, {0x3502, 0x80}, 
	{0x350a, 0x00}, {0x350b, 0x7f}, {0x5001, 0x01},
	{0x5180, 0x08}, {0x5186, 0x04}, {0x5187, 0x00}, 
	{0x5188, 0x04}, {0x5189, 0x00}, {0x518a, 0x04}, 
	{0x518b, 0x00}, {0x5000, 0x06},                                                              
	/*2592x1944 Reference Setting 24M MCLK 2lane 280Mbps/lane 30fps*/ 
	/*Display Out setting
	  0x3808~0x3809 : Width	/0x380A~0x380B : Height*/
	{0x3035, 0x21}, {0x3036, 0x4f}, {0x3821, 0x06},
	{0x3820, 0x00}, {0x3612, 0x0b}, {0x3618, 0x04},
	{0x380c, 0x0a}, {0x380d, 0x8c}, {0x380e, 0x07},
	{0x380f, 0xb0}, {0x3814, 0x11}, {0x3815, 0x11},
	{0x3709, 0x12}, {0x3808, 0x0a}, {0x3809, 0x20},
	{0x380a, 0x07}, {0x380b, 0x98}, {0x3800, 0x00},
	{0x3801, 0x04}, {0x3802, 0x00}, {0x3803, 0x00},
	{0x3804, 0x0a}, {0x3805, 0x3b}, {0x3806, 0x07},
	{0x3807, 0xa3}, {0x4004, 0x04}, {0x0100, 0x01},
	{REG_NULL, 0x00}
}; 

struct ov5647_mode_info ov5647_mode_data[OV5647_NUM_MODES] = {
	{OV5647_MODE_VGA_640_480, SUBSAMPLING,
	 640, 2592, 480, 1944,
	 ov5647_setting_VGA_640_480,
	 ARRAY_SIZE(ov5647_setting_VGA_640_480)},
	{OV5647_MODE_720P_1280_720, SUBSAMPLING,
	 1280, 2592, 720, 1944,
	 ov5647_setting_720P_1280_720,
	 ARRAY_SIZE(ov5647_setting_720P_1280_720)},
	{OV5647_MODE_960P_1280_960, SUBSAMPLING,
	 1280, 2592, 960, 1944,
	 ov5647_setting_960P_1280_960,
	 ARRAY_SIZE(ov5647_setting_960P_1280_960)},
	{OV5647_MODE_QSXGA_2592_1944, SUBSAMPLING,
	 2592, 2592, 1944, 1944,
	 ov5647_setting_QSXGA_2592_1944,
	 ARRAY_SIZE(ov5647_setting_QSXGA_2592_1944)},
};

static const int ov5647_framerates[] = {
	[OV5647_15_FPS] = 15,
	[OV5647_30_FPS] = 30,
	[OV5647_60_FPS] = 60,
};

static const struct regval ov5647_start_settings[] = {
	{0x0100, 0x01}, {0x4800, 0x04},  {0x4202, 0x00},
	{0x300D, 0x00}, {REG_NULL, 0x00}
};

static const struct regval ov5647_stop_settings[] = {
	{0x0100, 0x00}, {0x4800, 0x25},  {0x4202, 0x0F},
	{0x300D, 0x01}, {REG_NULL, 0x00}
};

struct ov5647_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct ov5647_pixfmt ov5647_formats[] = {
	{ MEDIA_BUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB, },
};

/* Read registers up to 4 at a time */
static int ov5647_read_reg(struct i2c_client *client, u16 reg, unsigned int len, u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if ((len > 4) || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

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

/* Write registers up to 4 at a time */
static int ov5647_write_reg(struct i2c_client *client, u16 reg, u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

static int ov5647_write_array(struct i2c_client *client, const struct regval *regs)
{
	u32 i;
	int ret = 0;
	//int val;

	FUNC_DEBUG();

	for (i = 0; ((ret == 0) && (regs[i].addr != REG_NULL)); i++) {
		ret = ov5647_write_reg(client, regs[i].addr,
					OV5647_REG_VALUE_08BIT,
					regs[i].val);
		//ov5647_read_reg(client, regs[i].addr, OV5647_REG_VALUE_08BIT, &val);
		//	printk("i=%4d:, reg=%4x, val=%4x, rb_val=%4x\n",
		//	i, regs[i].addr, regs[i].val, val);
	}

	return ret;
}

static int ov5647_set_mode(struct ov5647 *ov5647)
{
	const struct ov5647_mode_info *mode = ov5647->current_mode;
	const struct ov5647_mode_info *orig_mode = ov5647->last_mode;
	enum ov5647_downsize_mode dn_mode, orig_dn_mode;
	unsigned long rate;
	int ret = 0;

	FUNC_DEBUG();
	DBG_INFO("%s, id: %d, dn_mode: %d, %dx%d\n", __FUNCTION__,
		mode->id, mode->dn_mode, mode->hact, mode->vact);

	dn_mode = mode->dn_mode;
	orig_dn_mode = orig_mode->dn_mode;

	/*
	 * All the formats we support have 16 bits per pixel, seems to require
	 * the same rate than YUV, so we can just use 16 bpp all the time.
	 */
	rate = mode->vtot * mode->htot * 16;
	rate *= ov5647_framerates[ov5647->current_fr];


	//if ((dn_mode == SUBSAMPLING && orig_dn_mode == SCALING) ||
	//    (dn_mode == SCALING && orig_dn_mode == SUBSAMPLING)) {
	//	/*
	//	 * change between subsampling and scaling
	//	 * go through exposure calculation
	//	 */
	//	ret = ov5647_set_mode_exposure_calc(ov5647, mode);
	//} else {
	//	/*
	//	 * change inside subsampling or scaling
	//	 * download firmware directly
	//	 */
	//	ret = ov5647_set_mode_direct(ov5647, mode);
	//}

	ret = ov5647_write_array(ov5647->i2c_client, mode->reg_data);

	ov5647->pending_mode_change = false;
	ov5647->last_mode = mode;

	return ret;
}

static int ov5647_set_framefmt(struct ov5647 *ov5647,
			       struct v4l2_mbus_framefmt *format)
{
	int ret = 0;
	const struct regval *reg_list;

	FUNC_DEBUG();
	DBG_INFO("%s, format->code: 0x%04x\n", __FUNCTION__, format->code);

	switch (format->code) {
		case MEDIA_BUS_FMT_SBGGR8_1X8:
			/* SBGGR, RAW10 */
			reg_list = ov5647_setting_QSXGA_2592_1944;
			break;

		default:
			return -EINVAL;
	}

	//ret = ov5647_write_array(ov5647->i2c_client, reg_list);

	return ret;
}

static int ov5647_set_stream_mipi(struct ov5647 *ov5647, bool on)
{
	int ret;
	const struct regval *reg_list;

	FUNC_DEBUG();
	DBG_INFO("%s, on: %d\n", __FUNCTION__, on);

	if (on){
		reg_list = ov5647_start_settings;
	} else {
		reg_list = ov5647_stop_settings;
	}

	ret = ov5647_write_array(ov5647->i2c_client, reg_list);
	if (ret) {
		return ret;
	}

	return 0;
}

static int ov5647_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov5647 *ov5647 = to_ov5647(sd);
	int ret = 0;

	FUNC_DEBUG();
	DBG_INFO("%s, streaming: %d, pending_mode_change: %d, pending_fmt_change: %d\n",
			__FUNCTION__, ov5647->streaming, ov5647->pending_mode_change,
			ov5647->pending_fmt_change);

	mutex_lock(&ov5647->lock);

	if (ov5647->streaming == !enable) {
		if (enable && ov5647->pending_mode_change) {
			ret = ov5647_set_mode(ov5647);
			if (ret)
				goto out;
		}

		if (enable && ov5647->pending_fmt_change) {
			ret = ov5647_set_framefmt(ov5647, &ov5647->fmt);
			if (ret)
				goto out;
			ov5647->pending_fmt_change = false;
		}

		//if (ov5647->ep.bus_type == V4L2_MBUS_CSI2_DPHY)
			ret = ov5647_set_stream_mipi(ov5647, enable);
		//else
		//	ret = ov5647_set_stream_dvp(ov5647, enable);

		if (!ret)
			ov5647->streaming = enable;
	}
out:
	if (ret) {
		DBG_ERR("Start streaming failed while write sensor registers!\n");
	}
	mutex_unlock(&ov5647->lock);
	return ret;
}

static struct v4l2_subdev_video_ops ov5647_subdev_video_ops = {
	.s_stream       = ov5647_s_stream,
};

static const struct ov5647_mode_info *
ov5647_find_mode(struct ov5647 *ov5647, enum ov5647_frame_rate fr,
		 int width, int height, bool nearest)
{
	const struct ov5647_mode_info *mode;

	mode = v4l2_find_nearest_size(ov5647_mode_data,
				      ARRAY_SIZE(ov5647_mode_data),
				      hact, vact,
				      width, height);

	DBG_INFO("%s, mode: %px, width: %d, height: %d, nearest: %d\n",
		__FUNCTION__, mode, width, height, nearest);

	if (!mode ||
	    (!nearest && (mode->hact != width || mode->vact != height)))
		return NULL;

	/* Only 640x480 can operate at 60fps (for now) */
	if (fr == OV5647_60_FPS &&
	    !(mode->hact == 640 && mode->vact == 480))
		return NULL;

	/* 2592x1944 only works at 15fps max */
	if ((mode->hact == 2592 && mode->vact == 1944) &&
	    fr > OV5647_15_FPS)
		return NULL;

	return mode;
}

static int ov5647_enum_mbus_code(struct v4l2_subdev *sd,
								struct v4l2_subdev_pad_config *cfg,
								struct v4l2_subdev_mbus_code_enum *code)
{
	FUNC_DEBUG();

	if (code->pad != 0)
		return -EINVAL;
	if (code->index >= ARRAY_SIZE(ov5647_formats))
		return -EINVAL;

	code->code = ov5647_formats[code->index].code;

	DBG_INFO("%s, index: %d, code: 0x%04x\n",
		__FUNCTION__, code->index, code->code);
	return 0;
}

static int ov5647_enum_frame_size(struct v4l2_subdev *sd,
								struct v4l2_subdev_pad_config *cfg,
								struct v4l2_subdev_frame_size_enum *fse)
{
	FUNC_DEBUG();

	if (fse->pad != 0)
		return -EINVAL;
	if (fse->index >= OV5647_NUM_MODES)
		return -EINVAL;

	fse->min_width = ov5647_mode_data[fse->index].hact;
	fse->max_width = fse->min_width;
	fse->min_height = ov5647_mode_data[fse->index].vact;
	fse->max_height = fse->min_height;

	DBG_INFO("%s, index: %d, min_w: %d, max_w: %d, min_h: %d, max_h: %d\n",
		__FUNCTION__, fse->index,
		fse->min_width, fse->max_width,
		fse->min_height, fse->max_height);
	return 0;
}

static int ov5647_try_frame_interval(struct ov5647 *ov5647,
				     struct v4l2_fract *fi,
				     u32 width, u32 height)
{
	const struct ov5647_mode_info *mode;
	enum ov5647_frame_rate rate = OV5647_15_FPS;
	int minfps, maxfps, best_fps, fps;
	int i;

	minfps = ov5647_framerates[OV5647_15_FPS];
	maxfps = ov5647_framerates[OV5647_60_FPS];

	if (fi->numerator == 0) {
		fi->denominator = maxfps;
		fi->numerator = 1;
		rate = OV5647_60_FPS;
		goto find_mode;
	}

	fps = clamp_val(DIV_ROUND_CLOSEST(fi->denominator, fi->numerator),
			minfps, maxfps);

	DBG_INFO("%s, fps: %d, numerator: %d, denominator = %d\n",
		__FUNCTION__, fps, fi->numerator, fi->denominator);

	best_fps = minfps;
	for (i = 0; i < ARRAY_SIZE(ov5647_framerates); i++) {
		int curr_fps = ov5647_framerates[i];

		if (abs(curr_fps - fps) < abs(best_fps - fps)) {
			best_fps = curr_fps;
			rate = i;
		}
	}

	fi->numerator = 1;
	fi->denominator = best_fps;

find_mode:
	mode = ov5647_find_mode(ov5647, rate, width, height, false);
	return mode ? rate : -EINVAL;
}

static int ov5647_enum_frame_interval(
	struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct ov5647 *ov5647 = to_ov5647(sd);
	struct v4l2_fract tpf;
	int ret;

	FUNC_DEBUG();

	if (fie->pad != 0)
		return -EINVAL;
	if (fie->index >= OV5647_NUM_FRAMERATES)
		return -EINVAL;

	tpf.numerator = 1;
	tpf.denominator = ov5647_framerates[fie->index];

	ret = ov5647_try_frame_interval(ov5647, &tpf,
					fie->width, fie->height);
	if (ret < 0)
		return -EINVAL;

	DBG_INFO("%s, index: %d, numerator: %d, denominator = %d\n",
		__FUNCTION__, fie->index, tpf.numerator, tpf.denominator);

	fie->interval = tpf;
	return 0;
}

static int ov5647_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct ov5647 *ov5647 = to_ov5647(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&ov5647->lock);

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt = v4l2_subdev_get_try_format(&ov5647->subdev, cfg,
						 format->pad);
	else
		fmt = &ov5647->fmt;
#else
	fmt = &ov5647->fmt;
#endif

	format->format = *fmt;

	mutex_unlock(&ov5647->lock);

	return 0;
}

static int ov5647_try_fmt_internal(struct v4l2_subdev *sd,
				   struct v4l2_mbus_framefmt *fmt,
				   enum ov5647_frame_rate fr,
				   const struct ov5647_mode_info **new_mode)
{
	struct ov5647 *ov5647 = to_ov5647(sd);
	const struct ov5647_mode_info *mode;
	int i;

	FUNC_DEBUG();

	mode = ov5647_find_mode(ov5647, fr, fmt->width, fmt->height, true);
	if (!mode)
		return -EINVAL;
	fmt->width = mode->hact;
	fmt->height = mode->vact;

	if (new_mode)
		*new_mode = mode;

	for (i = 0; i < ARRAY_SIZE(ov5647_formats); i++)
		if (ov5647_formats[i].code == fmt->code)
			break;
	if (i >= ARRAY_SIZE(ov5647_formats))
		i = 0;

	fmt->code = ov5647_formats[i].code;
	fmt->colorspace = ov5647_formats[i].colorspace;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);

	DBG_INFO("%s, code: 0x%04x, width: %d, height: %d\n",
		__FUNCTION__, fmt->code, fmt->width, fmt->height);

	return 0;
}

static int ov5647_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct ov5647 *ov5647 = to_ov5647(sd);
	const struct ov5647_mode_info *new_mode;
	struct v4l2_mbus_framefmt orig_fmt = ov5647->fmt;
	struct v4l2_mbus_framefmt *mbus_fmt = &format->format;
	struct v4l2_mbus_framefmt *fmt;
	int ret;

	FUNC_DEBUG();
	DBG_INFO("%s, mbus_fmt code: 0x%04x, %dx%d\n",
			__FUNCTION__, mbus_fmt->code, mbus_fmt->width, mbus_fmt->height);

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&ov5647->lock);

	if (ov5647->streaming) {
		ret = -EBUSY;
		goto out;
	}

	ret = ov5647_try_fmt_internal(sd, mbus_fmt,
				      ov5647->current_fr, &new_mode);
	if (ret)
		goto out;

	DBG_INFO("%s, mbus_fmt->code: 0x%04x, ov5647->fmt.code: 0x%04x\n",
			__FUNCTION__, mbus_fmt->code, ov5647->fmt.code);

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt = v4l2_subdev_get_try_format(sd, cfg, 0);
	else
		fmt = &ov5647->fmt;
#else
	fmt = &ov5647->fmt;
#endif

	*fmt = *mbus_fmt;

	if (new_mode != ov5647->current_mode) {
		ov5647->current_mode = new_mode;
		ov5647->pending_mode_change = true;
	}
	if ((mbus_fmt->code != ov5647->fmt.code) ||
		(orig_fmt.code != ov5647->fmt.code))
		ov5647->pending_fmt_change = true;

	DBG_INFO("%s, code: 0x%04x, pending_mode_change: %d, pending_fmt_change: %d\n",
			__FUNCTION__, ov5647->fmt.code, ov5647->pending_mode_change,
			ov5647->pending_fmt_change);

out:
	mutex_unlock(&ov5647->lock);
	return ret;
}

static struct v4l2_subdev_pad_ops ov5647_subdev_pad_ops = {
	.enum_mbus_code      = ov5647_enum_mbus_code,
	.get_fmt             = ov5647_get_fmt,
	.set_fmt             = ov5647_set_fmt,
	.enum_frame_size     = ov5647_enum_frame_size,
	.enum_frame_interval = ov5647_enum_frame_interval,
};

static struct v4l2_subdev_ops ov5647_subdev_ops = {
	.video          = &ov5647_subdev_video_ops,
	.pad            = &ov5647_subdev_pad_ops,
};

static int ov5647_check_sensor_id(struct ov5647 *ov5647, struct i2c_client *client)
{
	u32 val = 0;
	int ret;

	ret = ov5647_read_reg(client, OV5647_REG_CHIP_ID,
			      OV5647_REG_VALUE_16BIT, &val);
	if ((ret != 0) || (val != CHIP_ID)) {
		DBG_ERR("Unexpected sensor (id = 0x%04x, ret = %d)!\n", val, ret);
		return -1;
	}
	DBG_INFO("Check sensor id success (id = 0x%04x).\n", val);

	return 0;
}

static int ov5647_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ov5647 *ov5647;
	struct v4l2_subdev *sd;
	int ret;

	FUNC_DEBUG();

	ov5647 = devm_kzalloc(dev, sizeof(*ov5647), GFP_KERNEL);
	if (!ov5647) {
		DBG_ERR("Failed to allocate memory for \'ov5647\'!\n");
		return -ENOMEM;
	}

	ov5647->i2c_client = client;

	mutex_init(&ov5647->lock);

	sd = &ov5647->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov5647_subdev_ops);
	DBG_INFO("Initialized V4L2 I2C subdevice.\n");

	ret = ov5647_check_sensor_id(ov5647, client);
	if (ret) {
		return ret;
	}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &ov5647_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

	ret = v4l2_async_register_subdev(sd);
	if (ret) {
		DBG_ERR("V4L2 async register subdevice failed.\n");
		goto err_clean_entity;
	}
	DBG_INFO("Registered V4L2 sub-device successfully.\n");

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif

	return ret;
}

static int ov5647_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5647 *ov5647 = to_ov5647(sd);

	FUNC_DEBUG();

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&ov5647->ctrl_handler);
	mutex_destroy(&ov5647->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ov5647_of_match[] = {
	{ .compatible = "ov5647" },
	{},
};
MODULE_DEVICE_TABLE(of, ov5647_of_match);
#endif

static const struct i2c_device_id ov5647_match_id[] = {
	{ "ov5647", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ov5647_match_id);

static struct i2c_driver ov5647_i2c_driver = {
	.probe          = ov5647_probe,
	.remove         = ov5647_remove,
	.id_table       = ov5647_match_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "ov5647",
		.of_match_table = of_match_ptr(ov5647_of_match),
	},
};

module_i2c_driver(ov5647_i2c_driver);

MODULE_DESCRIPTION("Sunplus ov5647 sensor driver");
MODULE_LICENSE("GPL v2");
