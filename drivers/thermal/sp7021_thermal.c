/*
 * SP7021 THERMAL function:
 *
 */

#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <linux/module.h>
#include <linux/err.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <mach/io_map.h>
#include <linux/clk.h>
#include <linux/reset.h> 

#include "thermal_core.h"


#if 0
/* For code development on SP7021 */
#define VA_B_REG		0xF8000000

#define MOON5_ADDR		(VA_B_REG + (0x80 * 5))

#endif


/* ---------------------------------------------------------------------------------------------- */
//#define THERMAL_FUNC_DEBUG
//#define THERMAL_DBG_INFO
//#define THERMAL_DBG_ERR

#ifdef THERMAL_FUNC_DEBUG
	#define FUNC_DEBUG()    printk(KERN_INFO "[THERMAL] Debug: %s(%d)\n", __FUNCTION__, __LINE__)
#else
	#define FUNC_DEBUG()
#endif

#ifdef THERMAL_DBG_INFO
#define DBG_INFO(fmt, args ...)	printk(KERN_INFO "[THERMAL] Info: " fmt, ## args)
#else
#define DBG_INFO(fmt, args ...)
#endif

#ifdef THERMAL_DBG_ERR
#define DBG_ERR(fmt, args ...)	printk(KERN_ERR  "[THERMAL] Error: " fmt, ## args)
#else
#define DBG_ERR(fmt, args ...)
#endif
/* ---------------------------------------------------------------------------------------------- */


#define DISABLE_THREMAL  (1<<31 | 1<<15)
#define ENABLE_THREMAL  (1<<31)

#define THERMAL_M_MODE_EN  (1<<30 | 1<<14)
#define THERMAL_M_MODE_DISABLE (1<<30)



#define TEMP_MASK  (0x7FF)
#define OTP_THERMAL_L  20
#define OTP_THERMAL_H  21
#define OTP_THERMAL_FT  22




#define TEMP_RATE  608

extern int read_otp_data(int addr, char *value);


/* common data structures */
struct sp_thermal_data {

	struct thermal_zone_device *pcb_tz;
	enum thermal_device_mode mode;
	struct work_struct thermal_wq;

	struct mutex thermal_lock;    /* protects register data */
	struct platform_device *pdev;

	long sensor_temp;
	uint32_t id;
	uint32_t thres_temp;
	void __iomem *regs;
};

struct sp_thermal_data sp_thermal;
//static struct sunplus_rtc *sp_rtc;

//static struct clk *rtc_clk;

#define MOO5_REG_NAME             "thermal_reg"


#define MOO4_REG_NAME             "thermal_moon4"


struct sp_thermal_reg {
	volatile unsigned int mo5_thermal_ctl0;
	volatile unsigned int mo5_thermal_ctl1;
	volatile unsigned int mo5_thermal_ctl2;
	volatile unsigned int mo5_thermal_ctl3;
	volatile unsigned int mo5_tmds_l2sw_ctl;
	volatile unsigned int mo5_l2sw_clksw_ctl;
	volatile unsigned int mo5_i2c2bus_ctl;
	volatile unsigned int mo5_pfcnt_ctl;
	volatile unsigned int mo5_pfcntl_sensor_ctl0;
	volatile unsigned int mo5_pfcntl_sensor_ctl1;
	volatile unsigned int mo5_pfcnt_sts0;
	volatile unsigned int mo5_pfcnt_sts1;
	volatile unsigned int mo5_thermal_sts0;
	volatile unsigned int mo5_thermal_sts1;
	volatile unsigned int mo5_rsv13;
	volatile unsigned int mo5_rsv14;
	volatile unsigned int mo5_rsv15;
	volatile unsigned int mo5_rsv16;
	volatile unsigned int mo5_rsv17;
	volatile unsigned int mo5_rsv18;
	volatile unsigned int mo5_rsv19;
	volatile unsigned int mo5_rsv20;
	volatile unsigned int mo5_rsv21;
	volatile unsigned int mo5_dc09_ctl0;
	volatile unsigned int mo5_dc09_ctl1;
	volatile unsigned int mo5_dc09_ctl2;
	volatile unsigned int mo5_dc12_ctl0;
	volatile unsigned int mo5_dc12_ctl1;
	volatile unsigned int mo5_dc12_ctl2;
	volatile unsigned int mo5_dc15_ctl0;
	volatile unsigned int mo5_dc15_ctl1;
	volatile unsigned int mo5_dc15_ctl2;
};

static volatile struct sp_thermal_reg *thermal_reg_ptr = NULL;


struct sp_ctl_reg {
	volatile unsigned int mo4_pllsp_ctl0;
	volatile unsigned int mo4_pllsp_ctl1;
	volatile unsigned int mo4_pllsp_ctl2;
	volatile unsigned int mo4_pllsp_ctl3;
	volatile unsigned int mo4_pllsp_ctl4;
	volatile unsigned int mo4_pllsp_ctl5;
	volatile unsigned int mo4_pllsp_ctl6;
	volatile unsigned int mo5_pfcnt_ctl;
	volatile unsigned int mo4_plla_ctl0;
	volatile unsigned int mo4_plla_ctl1;
	volatile unsigned int mo4_plla_ctl2;
	volatile unsigned int mo4_plla_ctl3;
	volatile unsigned int mo4_plla_ctl4;
	volatile unsigned int mo4_plle_ctl;
	volatile unsigned int mo4_pllf_ctl;
	volatile unsigned int mo4_plltv_ctl0;
	volatile unsigned int mo4_plltv_ctl1;
	volatile unsigned int mo4_plltv_ctl2;
	volatile unsigned int mo4_usbc_ctl;
	volatile unsigned int mo4_uphy0_ctl0;
	volatile unsigned int mo4_uphy0_ctl1;
	volatile unsigned int mo4_uphy0_ctl2;
	volatile unsigned int mo4_uphy1_ctl0;
	volatile unsigned int mo4_uphy1_ctl1;
	volatile unsigned int mo4_uphy1_ctl2;	
	volatile unsigned int mo4_uphy1_ctl3;
	volatile unsigned int mo4_pllsys;
	volatile unsigned int mo_clk_sel0;
	volatile unsigned int mo_probe_sel;
	volatile unsigned int mo4_misc_ctl0;
	volatile unsigned int mo4_uphy0_sts;
	volatile unsigned int otp_st;

};

static volatile struct sp_ctl_reg *sp_ctl_reg_ptr = NULL;


int thermal_count = 0;




int sp_thermal_get_temp(int *temp)
{
    int temp_code,temp100;
	int otp_thermal_t0,otp_thermal_t1;
    char otp_temp[3];

    FUNC_DEBUG();


	read_otp_data(OTP_THERMAL_L, &otp_temp[0]);
	read_otp_data(OTP_THERMAL_H, &otp_temp[1]);
    read_otp_data(OTP_THERMAL_FT, &otp_temp[2]);

	otp_thermal_t0 = otp_temp[0] | (otp_temp[1] << 8);
	otp_thermal_t0 = otp_thermal_t0 & TEMP_MASK ;
	DBG_INFO("otp_thermal_t0 %x  ",otp_thermal_t0 );

	otp_thermal_t1 = (otp_temp[1] >> 3) | (otp_temp[2] << 5);
	otp_thermal_t1 = otp_thermal_t1 & TEMP_MASK ;

	//DBG_INFO("otp_temp_0 %x , otp_temp_1 %x , otp_temp_2 %x ,otp_thermal_t1 %x  ",otp_temp[0],otp_temp[1],otp_temp[2],otp_thermal_t1);

    if(otp_thermal_t0 == 0)
		otp_thermal_t0 = 1488;
	//otp_thermal = 1550;


	temp_code = TEMP_MASK & readl(&thermal_reg_ptr->mo5_thermal_sts0);
    temp100 = ((otp_thermal_t0 - temp_code)*10000/TEMP_RATE)+4000;	
    //temp100 = ((temp_code*10000)/608)+2500;

    //temp100 = 25000-temp100;

	*temp =  temp100;

#if(0)  


    if((temp100 >= 4500) & (thermal_count < 5)){
		
	//writel(0x78F00010 , &sp_ctl_reg_ptr->mo_clk_sel0);	// enable thermal function
	writel(0x00F00010 , &sp_ctl_reg_ptr->mo_clk_sel0);	// enable thermal function
	thermal_count++ ;
    	}
	else{
	//writel(0x78F00000 , &sp_ctl_reg_ptr->mo_clk_sel0);	// enable thermal function
	writel(0x00F00000 , &sp_ctl_reg_ptr->mo_clk_sel0);	// enable thermal function
		thermal_count++ ;
	if(thermal_count >= 10)
		thermal_count = 0;
		}

#endif



	DBG_INFO("temp_code %d , temp100 %d",temp_code,temp100 );

	
	return 0;
}
EXPORT_SYMBOL(sp_thermal_get_temp);



static int sp_thermal_get_senser_temp(void *_data, int *temp)
{

#if(0)
	struct sp_thermal_data *data = _data;


    int temp_code,temp100;

    FUNC_DEBUG();

	mutex_lock(&data->thermal_lock);

	temp_code = TEMP_MASK & readl(&thermal_reg_ptr->mo5_thermal_sts0);
    temp100 = (1760 - temp_code)*10000/740;
    //temp100 = 25000-temp100;

	*temp =  temp100;

	mutex_unlock(&data->thermal_lock);


	DBG_INFO("temp_code %d , temp100 %d",temp_code,temp100 );


	
	return 0;

#endif

	struct sp_thermal_data *data = _data;
	struct sp_thermal_reg *thermal_reg = data->regs;


    int temp_code,temp100;
	int otp_thermal_t0,otp_thermal_t1;
    char otp_temp[3];

   // FUNC_DEBUG();

	mutex_lock(&data->thermal_lock);

	read_otp_data(OTP_THERMAL_L, &otp_temp[0]);
	read_otp_data(OTP_THERMAL_H, &otp_temp[1]);
    read_otp_data(OTP_THERMAL_FT, &otp_temp[2]);

	otp_thermal_t0 = otp_temp[0] | (otp_temp[1] << 8);
	otp_thermal_t0 = otp_thermal_t0 & TEMP_MASK ;
	//DBG_INFO("otp_thermal_t0 %x  ",otp_thermal_t0 );

	otp_thermal_t1 = (otp_temp[1] >> 3) | (otp_temp[2] << 5);
	otp_thermal_t1 = otp_thermal_t1 & TEMP_MASK ;


    //DBG_INFO("otp_temp_0 %x , otp_temp_1 %x , otp_temp_2 %x ,otp_thermal %x  ",otp_temp[0],otp_temp[1],otp_temp[2],otp_thermal	);

    if(otp_thermal_t0 == 0)
		otp_thermal_t0 = 1488;


	temp_code = TEMP_MASK & readl(&thermal_reg->mo5_thermal_sts0);
    temp100 = ((otp_thermal_t0 - temp_code)*10000/TEMP_RATE)+4000;	
    //temp100 = ((temp_code*10000)/608)+2500;

    //temp100 = 25000-temp100;

	*temp =  temp100;

#if(0)  

    if((temp100 >= 4500) & (thermal_count < 5)){
		
	//writel(0x78F00010 , &sp_ctl_reg_ptr->mo_clk_sel0);	// enable thermal function
	writel(0x00F00010 , &sp_ctl_reg_ptr->mo_clk_sel0);	// enable thermal function
	thermal_count++ ;
    	}
	else{
	//writel(0x78F00000 , &sp_ctl_reg_ptr->mo_clk_sel0);	// enable thermal function
	writel(0x00F00000 , &sp_ctl_reg_ptr->mo_clk_sel0);	// enable thermal function
		thermal_count++ ;
	if(thermal_count >= 10)
		thermal_count = 0;
		}

#endif

	mutex_unlock(&data->thermal_lock);


    DBG_INFO("temp_code %d , temp100 %d",temp_code,temp100 );


return 0;


	
}



static struct thermal_zone_of_device_ops sp_of_thermal_ops = {
	.get_temp = sp_thermal_get_senser_temp,
};




static int sp_thermal_register_sensor(struct platform_device *pdev,
					struct sp_thermal_data *data,
					int index)
{
	int ret, i;
	const struct thermal_trip *trip;

	data->id = index;

	data->pcb_tz = devm_thermal_zone_of_sensor_register(&pdev->dev,
				data->id, data, &sp_of_thermal_ops);
	if (IS_ERR(data->pcb_tz)) {
		ret = PTR_ERR(data->pcb_tz);
		data->pcb_tz = NULL;
		dev_err(&pdev->dev, "failed to register sensor id %d: %d\n",
			data->id, ret);
		return ret;
	}

	trip = of_thermal_get_trip_points(data->pcb_tz);

	for (i = 0; i < of_thermal_get_ntrips(data->pcb_tz); i++) {
		if (trip[i].type == THERMAL_TRIP_PASSIVE) {
			data->thres_temp = trip[i].temperature;
			break;
		}
	}

	return 0;
}



static int sp_thermal_probe(struct platform_device *plat_dev)
{

	struct sp_thermal_data *sp_data;

	int ret;
	int temp;
	//struct sp_thermal_reg *thermal;
	struct resource *res;
	void __iomem *reg_base;
	void __iomem *ctl_base;

	int ctl_code;

    FUNC_DEBUG();

	sp_data = devm_kzalloc(&plat_dev->dev, sizeof(*sp_data), GFP_KERNEL);
	if (!sp_data)
		return -ENOMEM;

	mutex_init(&sp_data->thermal_lock);


	//memset(sp_rtc, 0, sizeof(sp_rtc));
	memset(&sp_thermal, 0, sizeof(sp_thermal));

	res = platform_get_resource_byname(plat_dev, IORESOURCE_MEM, MOO5_REG_NAME);
	if (res) {
		reg_base = devm_ioremap(&plat_dev->dev, res->start, resource_size(res));
		if (IS_ERR(reg_base)) {
			dev_err(&plat_dev->dev,"%s devm_ioremap_resource fail\n",MOO5_REG_NAME);
		}
	}
	dev_dbg(&plat_dev->dev,"reg_base 0x%x\n",(unsigned int)reg_base);
	sp_data->regs = reg_base;


	DBG_INFO("reg_base 0x%x\n",(unsigned int)reg_base);

	res = platform_get_resource_byname(plat_dev, IORESOURCE_MEM, MOO4_REG_NAME);
	if (res) {
		ctl_base = devm_ioremap(&plat_dev->dev, res->start, resource_size(res));
		if (IS_ERR(reg_base)) {
			dev_err(&plat_dev->dev,"%s devm_ioremap_resource fail\n",MOO4_REG_NAME);
		}
	}
	dev_dbg(&plat_dev->dev,"reg_base 0x%x\n",(unsigned int)ctl_base);



	thermal_reg_ptr = (volatile struct sp_thermal_reg *)(reg_base);
	sp_ctl_reg_ptr = (volatile struct sp_ctl_reg *)(ctl_base);	
	
    writel(ENABLE_THREMAL , &thermal_reg_ptr->mo5_thermal_ctl0);  // enable thermal function
   // writel(THERMAL_M_MODE_EN , &thermal_reg_ptr->mo5_thermal_ctl0);  // enable thermal m mode
   // writel(0x00060002 , &thermal_reg_ptr->mo5_thermal_ctl0);  // enable thermal function


   //ctl_code = 0xFFFF & readl(&sp_ctl_reg_ptr->mo_clk_sel0);

  // writel(0x78F07810 , &sp_ctl_reg_ptr->mo_clk_sel0);  // enable thermal function

   ctl_code = 0xFFFF & readl(&sp_ctl_reg_ptr->mo_clk_sel0);


   DBG_INFO("ctl_code %x ",ctl_code );


	platform_set_drvdata(plat_dev, sp_data);


	ret = sp_thermal_register_sensor(plat_dev, sp_data, 0);

	DBG_INFO(" ret %d",ret );

    ret =  sp_thermal_get_temp(&temp);


	DBG_INFO(" temp %d",temp );
	
	return ret;	
	
}

static int sp_thermal_remove(struct platform_device *plat_dev)
{
	//struct sp_thermal_data *sp_data = platform_get_drvdata(plat_dev);

	//reset_control_assert(sp_rtc.rstc);
	
	//rtc_device_unregister(rtc);
	return 0;
}

static const struct of_device_id of_sp_thermal_match[] = {
	{ .compatible = "sunplus,sp7021-thermal" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_rtc_of_match);

static struct platform_driver sp_thermal_driver = {
	.probe		= sp_thermal_probe,
	.remove 	= sp_thermal_remove,
	.driver 	= {
		.name = "sp7021-thermal",
		.of_match_table = of_sp_thermal_match,
	},
};


module_platform_driver(sp_thermal_driver);



MODULE_AUTHOR("Sunplus");
MODULE_DESCRIPTION("Sunplus RTC driver");
MODULE_LICENSE("GPL");
