#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include "hal_mipi_i2c.h"

/* Message Definition */
//#define HAL_I2C_FUNC_DEBUG
//#define HAL_I2C_FUNC_INFO
//#define HAL_I2C_FUNC_ERR

#define I2C_TIMEOUT_TH  0x10000

#ifdef HAL_I2C_FUNC_DEBUG
	#define HAL_I2C_DEBUG()    printk(KERN_INFO "[HAL I2C] DBG: %s(%d)\n", __FUNCTION__, __LINE__)
#else
	#define HAL_I2C_DEBUG()
#endif
#ifdef HAL_I2C_FUNC_INFO
	#define HAL_I2C_INFO(fmt, args ...)    printk(KERN_INFO "[HAL I2C] INFO: " fmt, ## args)
#else
	#define HAL_I2C_INFO(fmt, args ...)
#endif
#ifdef HAL_I2C_FUNC_ERR
	#define HAL_I2C_ERR(fmt, args ...)    printk(KERN_ERR "[HAL I2C] ERR: " fmt, ## args)
#else
	#define HAL_I2C_ERR(fmt, args ...)
#endif

static mipi_isp_reg_t *pMipiIspReg[MIPI_I2C_NUM];

/*
void sleep(unsigned long sec)
{
	int i;

	HAL_I2C_INFO("%s, total time: %ld seconds\n", __FUNCTION__, sec);
	for (i = 0; i < sec; i++)
	{
		mdelay(1000);
		HAL_I2C_INFO("%s, %d sec\n", __FUNCTION__, i+1);
	}
}
*/

/*
	@ispSleep_i2c this function depends on O.S.
*/
void ispSleep_i2c(int delay)
{
	#define ISP_DELAY_TIMEBASE  21  // 20.83 ns
	u64 time;

	// Calculate how many time to delay in ns
	time = delay * ISP_DELAY_TIMEBASE;
	HAL_I2C_INFO("%s, delay %lld ns\n", __FUNCTION__, time);
	ndelay(time);
}

/*
	@hal_mipi_i2c_isp_reset
*/
void hal_mipi_i2c_isp_reset(unsigned int device_id)
{
	mipi_isp_reg_t *regs = pMipiIspReg[device_id];

	HAL_I2C_INFO("%s, %d\n", __FUNCTION__, __LINE__);

	writeb(0x13, &(regs->reg[0x2000])); //reset all module include ispck
	writeb(0x1c, &(regs->reg[0x2003])); //enable phase clocks
	writeb(0x07, &(regs->reg[0x2005])); //enbale p1xck
	writeb(0x05, &(regs->reg[0x2008])); //switch b1xck/bpclk_nx to normal clocks
	writeb(0x03, &(regs->reg[0x2000])); //release ispck reset
	ispSleep_i2c(20);                     //#(`TIMEBASE*20;
	//
	writeb(0x00, &(regs->reg[0x2000])); //release all module reset
	//
	writeb(0x01, &(regs->reg[0x276c])); //reset front
	writeb(0x00, &(regs->reg[0x276c])); //
	//	
	writeb(0x03, &(regs->reg[0x2000]));
	writeb(0x00, &(regs->reg[0x2000]));
	//
	writeb(0x00, &(regs->reg[0x2010])); //cclk: 48MHz
}
EXPORT_SYMBOL(hal_mipi_i2c_isp_reset);

void hal_mipi_i2c_power_on(unsigned int device_id)
{
	mipi_isp_reg_t *regs = pMipiIspReg[device_id];
	u8 data;

	HAL_I2C_INFO("%s start\n", __FUNCTION__);

	/* enable sensor mclk and i2c sck */
	writeb(0x00, &(regs->reg[0x2781]));
	writeb(0x08, &(regs->reg[0x2785]));

	data = readb(&(regs->reg[0x2042])) | 0x03;
	writeb(data, &(regs->reg[0x2042]));     /* xgpio[8,9] output enable */
	data = readb(&(regs->reg[0x2044])) | 0x03;
	writeb(data, &(regs->reg[0x2044]));     /* xgpio[8,9] output high - power up */

	mdelay(1);

	//writeb(0xFD, &(regs->reg[0x2042]));
	//writeb(0xFD, &(regs->reg[0x2044]));
	data = readb(&(regs->reg[0x2044])) & (~0x1);
	writeb(data, &(regs->reg[0x2044]));     /* xgpio[8] output low - reset */

	mdelay(1);

	data = readb(&(regs->reg[0x2044])) | 0x03;
	writeb(data, &(regs->reg[0x2044]));     /* xgpio[8,9] output high - power up */

	HAL_I2C_INFO("%s end\n", __FUNCTION__);
}
EXPORT_SYMBOL(hal_mipi_i2c_power_on);

void hal_mipi_i2c_power_down(unsigned int device_id)
{
	mipi_isp_reg_t *regs = pMipiIspReg[device_id];
	u8 data;

	HAL_I2C_INFO("%s start\n", __FUNCTION__);

	/* disable sensor mclk and i2c sck */
	//writeb(0x48, &(regs->reg[0x2781]));
	writeb(0x00, &(regs->reg[0x2785]));
	udelay(6);                              /* delay 128 extclk = 6 us */

	/* xgpio[8) - 0: reset, 1: normal */
	data = readb(&(regs->reg[0x2042])) | 0x01;
	writeb(data, &(regs->reg[0x2042]));     /* xgpio[8] output enable */
	data = readb(&(regs->reg[0x2044])) & (~0x1);
	writeb(data, &(regs->reg[0x2044]));     /* xgpio[8] output low - reset */

	/* xgpio[9) - 0: power down, 1: power up */
	data = readb(&(regs->reg[0x2042])) | 0x02;
	writeb(data, &(regs->reg[0x2042]));     /* xgpio[9] output enable */
	data = readb(&(regs->reg[0x2044])) & 0xFD;
	writeb(data, &(regs->reg[0x2044]));     /* xgpio[9] output low - power down */

	HAL_I2C_INFO("%s end\n", __FUNCTION__);
}
EXPORT_SYMBOL(hal_mipi_i2c_power_down);
/*
void Reset_i2c(mipi_isp_reg_t *regs)
{
	HAL_I2C_INFO("%s, start\n", __FUNCTION__);

	writeb(0x01, &(regs->reg[0x2660]));
	writeb(0x00, &(regs->reg[0x2660]));
}
*/
//void hal_mipi_i2c_init(u8 DevSlaveAddr, u8 speed_div, unsigned int device_id)
void hal_mipi_i2c_init(unsigned int device_id)
{
	mipi_isp_reg_t *regs = pMipiIspReg[device_id];
	u8 data;

	HAL_I2C_INFO("%s, start\n", __FUNCTION__);

	data = readb(&(regs->reg[0x2790])) & 0xBF;
	writeb(data, &(regs->reg[0x2790]));				//clear bit6:sdi gate0
	//writeb(0x01, &(regs->reg[0x27F6]));			//enable // for other ISP
	//writeb(DevSlaveAddr, &(regs->reg[0x2600]));
	//writeb(speed_div, &(regs->reg[0x2604]));		/* speed selection */
													/*
														0: 48MHz div 2048
														1: 48MHz div 1024
														2: 48MHz div 512
														3: 48MHz div 256
														4: 48MHz div 128
														5: 48MHz div 64
														6: 48MHz div 32
														7: 48MHz div 16
													*/
}

// Write data to 16-bit address sensor
void setSensor16_i2c(unsigned int device_id, u16 addr16, u16 value16, u8 data_count)
{
	mipi_isp_reg_t *regs = pMipiIspReg[device_id];
	u8 i;
	//u8 addr_count = 0x02, data_count = 0x02;
	u8 addr_count = 0x02;
	u8 regData[2];
	u32 SensorI2cTimeOut;

	HAL_I2C_INFO("%s, start\n", __FUNCTION__);

	if ((addr_count+data_count) > 32) return;
	if (data_count == 1)
	{
		regData[0] = (u8)(value16 & 0xFF);
	}
	else if (data_count == 2)
	{
		regData[0] = (u8)(value16 >> 8);
		regData[1] = (u8)(value16 & 0xFF);
	}
	else
	{
		//not support
		HAL_I2C_ERR("%s, no support! (data_count=%d)\n", __FUNCTION__, data_count);
		return;
	}
		
	//writeb(SensorSlaveAddr, &(regs->reg[0x2600]));
	//writeb(0x00, &(regs->reg[0x2604]));					/* speed selection */
													/*
														0: 48MHz div 2048
														1: 48MHz div 1024
														2: 48MHz div 512
														3: 48MHz div 256
														4: 48MHz div 128
														5: 48MHz div 64
														6: 48MHz div 32
														7: 48MHz div 16
													*/
	writeb(0x04, &(regs->reg[0x2601]));						/* set new transaction write mode */
	writeb(0x00, &(regs->reg[0x2603]));                  			/* set synchronization with vd */
	if ((addr_count+data_count) == 32)
		writeb(0x00, &(regs->reg[0x2608]));						/* total 32 bytes to write */
	else
		writeb((addr_count+data_count), &(regs->reg[0x2608])); /* total count bytes to write */
   
	/* data write */
	writeb((u8)(addr16 >> 8), &(regs->reg[0x2610]));			/* set the high byte of subaddress */
	writeb((u8)(addr16 & 0x00FF), &(regs->reg[0x2611]));		/* set the low byte of subaddress */
	for (i = 0; i < data_count; i++) {
		writeb(regData[i], &(regs->reg[0x2612+i]));	/* set data */
	}
		
	//writeb(addr16_H, &(regs->reg[0x2610]));   				/* set subaddress */
	//writeb(addr16_L, &(regs->reg[0x2611]));   				/* set subaddress */
	//for (i = 0; i < data_count; i++)
	//	writeb(regData[i], &(regs->reg[0x2612+i]));			/* set data */	

	SensorI2cTimeOut = I2C_TIMEOUT_TH;					//ULONG SensorI2cTimeOut defined in ROM
	while (readb(&(regs->reg[0x2650])) == 0x01)				/* busy waiting until serial interface not busy */  
	{
		SensorI2cTimeOut--;
		if (SensorI2cTimeOut == 0)
		{
			HAL_I2C_ERR("%s, I2C Timeout!\n", __FUNCTION__);

			/* reset front i2c interface */                            
			writeb(0x01, &(regs->reg[0x2660]));
			writeb(0x00, &(regs->reg[0x2660]));
			return;
		}
  	}	
	HAL_I2C_INFO("%s, I2C time: %d\n", __FUNCTION__, I2C_TIMEOUT_TH-SensorI2cTimeOut);

	HAL_I2C_INFO("%s, Done\n", __FUNCTION__);
}
EXPORT_SYMBOL(setSensor16_i2c);

// Read data from 16-bit address sensor
void getSensor16_i2c(unsigned int device_id, u16 addr16, u16 *value16, u8 data_count)
{
	mipi_isp_reg_t *regs = pMipiIspReg[device_id];
	u8 i;
	//u8 addr_count = 0x02, data_count = 0x02;
	u8 addr_count = 0x02;
	u8 regData[2];
	u32 SensorI2cTimeOut;

	HAL_I2C_INFO("%s, start\n", __FUNCTION__);

	HAL_I2C_INFO("%s, addr=0x%04x, count=%d\n", __FUNCTION__, addr16, data_count);

	if (addr_count>32 || data_count>32)return;
	//writeb(, &(regs->reg[0x2600) = SensorSlaveAddr;
	//writeb(, &(regs->reg[0x2604) = 0x00;			/* speed selection */
											/*
												0: 48MHz div 2048
												1: 48MHz div 1024
												2: 48MHz div 512
												3: 48MHz div 256
												4: 48MHz div 128
												5: 48MHz div 64
												6: 48MHz div 32
												7: 48MHz div 16
											*/
	writeb(0x06, &(regs->reg[0x2601]));                	/* set new transaction READ mode */
	writeb(0x00, &(regs->reg[0x2603]));               		/* set synchronization with vd */
	//
	if (addr_count == 32)
		writeb(0x00, &(regs->reg[0x2608]));					/* total 32 bytes to write */
	else
		writeb(addr_count, &(regs->reg[0x2608])); 		/* total count bytes to write */
	//
	if (data_count == 32)
		writeb(0x00, &(regs->reg[0x2609]));					/* total 32 bytes to write */
	else
		writeb(data_count, &(regs->reg[0x2609])); 		/* total count bytes to write */
	//			
	writeb((u8)(addr16 >> 8), &(regs->reg[0x2610]));		/* set the high byte of subaddress */
	writeb((u8)(addr16 & 0x00FF), &(regs->reg[0x2611]));	/* set the low byte of subaddress */
	//writeb(addr16_H, &(regs->reg[0x2610]));   			/* set subaddress */
	//writeb(addr16_L, &(regs->reg[0x2611]));   			/* set subaddress */
	writeb(0x13, &(regs->reg[0x2602]));           			/* set restart enable, subaddress enable , subaddress 16-bit mode and prefetech */ 

	SensorI2cTimeOut = I2C_TIMEOUT_TH;				//ULONG SensorI2cTimeOut defined in ROM
	while (readb(&(regs->reg[0x2650])) == 0x01)			/* busy waiting until serial interface not busy */  
	{
		SensorI2cTimeOut--;
		if (SensorI2cTimeOut == 0)
		{
			HAL_I2C_ERR("%s, I2C Timeout!\n", __FUNCTION__);

			/* reset front i2c interface */                            
			writeb(0x01, &(regs->reg[0x2660]));
			writeb(0x00, &(regs->reg[0x2660]));
			return;
		}
  	}
	HAL_I2C_INFO("%s, I2C time: %d\n", __FUNCTION__, I2C_TIMEOUT_TH-SensorI2cTimeOut);

	/* data read */
	for (i = 0; i < data_count; i++)
	{   
		regData[i] = readb(&(regs->reg[0x2610 + i]));	/* get data */

		HAL_I2C_INFO("%s, i=%d, regData[i]=0x%02x\n", __FUNCTION__, i, regData[i]);
	}

	SensorI2cTimeOut = I2C_TIMEOUT_TH;
	while (readb(&(regs->reg[0x2650])) == 0x01)				/* busy waiting until serial interface not busy */  
	{
		SensorI2cTimeOut--;
		if (SensorI2cTimeOut == 0)
		{
			/* reset front i2c interface */                            
			writeb(0x01, &(regs->reg[0x2660]));
			writeb(0x00, &(regs->reg[0x2660]));
			return;
		}
  	}
	if (data_count == 1)
		*value16 = *((u8 *)regData);
	else if (data_count == 2)
		*value16 = *((u16 *)regData);
	else
		*value16 = 0;

	HAL_I2C_INFO("%s, I2C time: %d\n", __FUNCTION__, I2C_TIMEOUT_TH-SensorI2cTimeOut);
	HAL_I2C_INFO("%s, Done\n", __FUNCTION__);
}
EXPORT_SYMBOL(getSensor16_i2c);

void hal_mipi_i2c_status_get(unsigned int device_id, unsigned char *status)
{
	*status = readb(&(pMipiIspReg[device_id]->reg[0x2650]));
	HAL_I2C_INFO("%s, 2650=0x%02x\n", __FUNCTION__, *status);

	//while (readb(&(regs->reg[0x2650])) == 0x01)			/* busy waiting until serial interface not busy */	
}
EXPORT_SYMBOL(hal_mipi_i2c_status_get);

void hal_mipi_i2c_data_get(unsigned int device_id, unsigned char *rdata, unsigned int read_cnt)
{
	unsigned int i = 0;

	if (device_id < MIPI_I2C_NUM) {
		for (i = 0; i < read_cnt; i++) {
			rdata[i] = readb(&(pMipiIspReg[device_id]->reg[0x2610+i]));

			HAL_I2C_INFO("%s, i=%d, rdata[i]=0x%02x\n", __FUNCTION__, i, rdata[i]); // CCHo add for debugging
		}
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_data_get);

void hal_mipi_i2c_data_set(unsigned int device_id, unsigned char *wdata, unsigned int write_cnt)
{
	unsigned int i = 0;

	if (device_id < MIPI_I2C_NUM) {
		for (i = 0; i < write_cnt; i++) {
			writeb(wdata[i], &(pMipiIspReg[device_id]->reg[0x2610+i]));
		}

		//writeb(addr16_H, &(regs->reg[0x2610]));					/* set subaddress */
		//writeb(addr16_L, &(regs->reg[0x2611]));					/* set subaddress */
		//for (i = 0; i < data_count; i++)
		//	writeb(regData[i], &(regs->reg[0x2612+i])); 		/* set data */	
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_data_set);

void hal_mipi_i2c_read_trigger(unsigned int device_id, mipi_i2c_start_mode start_mode)
{
	unsigned char reg_2602 = 0;

	if (device_id < MIPI_I2C_NUM) {
		switch(start_mode) {
			default:
			case I2C_START:
				reg_2602 = MIPI_I2C_2602_SUBADDR_EN | MIPI_I2C_2602_PREFETCH;
				break;

			case I2C_RESTART:
				reg_2602 = MIPI_I2C_2602_SUBADDR_EN | MIPI_I2C_2602_RESTART_EN | MIPI_I2C_2602_PREFETCH;
				break;
		}

		writeb(reg_2602, &(pMipiIspReg[device_id]->reg[0x2602]));

		//writeb(0x13, &(regs->reg[0x2602]));           			/* set restart enable, subaddress enable , subaddress 16-bit mode and prefetech */ 

	}
}
EXPORT_SYMBOL(hal_mipi_i2c_read_trigger);


void hal_mipi_i2c_rw_mode_set(unsigned int device_id, mipi_i2c_action rw_mode)
{
	unsigned char reg_2601 = 0;

	if (device_id < MIPI_I2C_NUM) {
		switch(rw_mode) {
			default:
			case I2C_NOR_NEW_WRITE:
				reg_2601 = MIPI_I2C_2601_NEW_TRANS_EN;
				break;

			case I2C_NOR_NEW_READ:
				reg_2601 = MIPI_I2C_2601_NEW_TRANS_EN | MIPI_I2C_2601_NEW_TRANS_MODE;
				break;
		}
		writeb(reg_2601, &(pMipiIspReg[device_id]->reg[0x2601]));	

		//writeb(0x04, &(regs->reg[0x2601])); 					/* set new transaction write mode */
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_rw_mode_set);

void hal_mipi_i2c_active_mode_set(unsigned int device_id, unsigned char mode)
{
	unsigned char reg_2603 = mode;

	if (device_id < MIPI_I2C_NUM) {
		writeb(reg_2603, &(pMipiIspReg[device_id]->reg[0x2603]));

		//writeb(0x00, &(regs->reg[0x2603])); 					/* set synchronization with vd */
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_active_mode_set);

void hal_mipi_i2c_trans_cnt_set(unsigned int device_id, unsigned int write_cnt, unsigned int read_cnt)
{
	unsigned char reg_2608 = write_cnt;
	unsigned char reg_2609 = read_cnt;

	if (device_id < MIPI_I2C_NUM) {
		if (reg_2608 > 31) reg_2608 = 0;    // 0 for 32 bytes to write
		if (reg_2608 != 0xff) 
			writeb(reg_2608, &(pMipiIspReg[device_id]->reg[0x2608]));

		if (reg_2609 > 31) reg_2609 = 0;    // 0 for 32 bytes to read
		if (reg_2609 != 0xff) 
			writeb(reg_2609, &(pMipiIspReg[device_id]->reg[0x2609]));

		//if ((addr_count+data_count) == 32)
		//	writeb(0x00, &(regs->reg[0x2608])); 					/* total 32 bytes to write */
		//else
		//	writeb((addr_count+data_count), &(regs->reg[0x2608])); /* total count bytes to write */
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_trans_cnt_set);

void hal_mipi_i2c_slave_addr_set(unsigned int device_id, unsigned int addr)
{
	unsigned int reg_2600 = 0;

	if (device_id < MIPI_I2C_NUM) {
		if (addr > 0x7F) {
			HAL_I2C_ERR("wrong slave address! addr:0x%04x", addr);
			return;
		}

		reg_2600 = addr << 1;   // Need to shift slave address by software 
		writeb((u8)reg_2600, &(pMipiIspReg[device_id]->reg[0x2600]));

		reg_2600 = 0;
		reg_2600 = readb(&(pMipiIspReg[device_id]->reg[0x2600]));
		HAL_I2C_INFO("%s, reg_2600=0x%02x\n", __FUNCTION__, reg_2600);
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_slave_addr_set);

void hal_mipi_i2c_clock_freq_set(unsigned int device_id, unsigned int freq)
{

	HAL_I2C_DEBUG();

	if (device_id < MIPI_I2C_NUM) {
		writeb((u8)freq, &(pMipiIspReg[device_id]->reg[0x2604]));
		//writeb(0x00, &(regs->reg[0x2604]));			/* speed selection */
														/*
															0: 48MHz div 2048
															1: 48MHz div 1024
															2: 48MHz div 512
															3: 48MHz div 256
															4: 48MHz div 128
															5: 48MHz div 64
															6: 48MHz div 32
															7: 48MHz div 16
														*/
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_clock_freq_set);

void hal_mipi_i2c_reset(unsigned int device_id)
{
	HAL_I2C_DEBUG();

	if (device_id < MIPI_I2C_NUM) {
		HAL_I2C_INFO("device_id:%01d, reg base:0x%px", device_id, pMipiIspReg[device_id]);

		//Reset_i2c(pMipiIspReg[device_id]);
		writeb(0x01, &(pMipiIspReg[device_id]->reg[0x2660]));
		writeb(0x00, &(pMipiIspReg[device_id]->reg[0x2660]));
	} else {
		HAL_I2C_ERR("wrong device id! device_id:%01d", device_id);
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_reset);

void hal_mipi_i2c_base_set(unsigned int device_id, void __iomem *membase)
{
	HAL_I2C_DEBUG();

	if (device_id < MIPI_I2C_NUM) {
		HAL_I2C_INFO("device_id:%01d, reg base:0x%px", device_id, membase);

		pMipiIspReg[device_id] = (mipi_isp_reg_t *)(membase - ISP_BASE_ADDRESS);
	} else {
		HAL_I2C_ERR("wrong device id! device_id:%01d", device_id);
	}
}
EXPORT_SYMBOL(hal_mipi_i2c_base_set);
