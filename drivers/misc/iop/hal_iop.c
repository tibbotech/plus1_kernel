#include <linux/delay.h>
#include <linux/io.h>
#include "hal_iop.h"
//#include <mach/io_map.h> 
#define IOP_CODE_SIZE	4096

extern const unsigned char IopNormalCode[];
void hal_iop_init(void __iomem *iopbase)
{
	regs_iop_t *pIopReg = (regs_iop_t *)iopbase;

	volatile unsigned int*   IOP_base =(volatile unsigned int*)0x100000;
	unsigned char * IOP_kernel_base;
	unsigned int reg;
	
	int wLen = IOP_CODE_SIZE;	
	//clock enable
	//reg = readl((void __iomem *)(B_SYSTEM_BASE + 32*4*0+ 4*1));
	//reg|=0x00100010;
	IOP_kernel_base = (unsigned char *)ioremap((unsigned long)IOP_base, IOP_CODE_SIZE);
	memset((unsigned char *)IOP_kernel_base,0, IOP_CODE_SIZE);
	memcpy((unsigned char *)IOP_kernel_base, IopNormalCode, IOP_CODE_SIZE);

	writel(0x00100010, (void __iomem *)(B_SYSTEM_BASE + 32*4*0+ 4*1));
	
	pIopReg->iop_control|=0x01;

	//printk("%s(%d) iop_data0=%x  iop_data1=%x iop_data2=%x iop_data3=%x iop_data4=%x iop_data5=%x\n", __FUNCTION__, __LINE__, 
	//	pIopReg->iop_data0,pIopReg->iop_data1,pIopReg->iop_data2,pIopReg->iop_data3,pIopReg->iop_data4,pIopReg->iop_data5);
	
	//printk("%s(%d) iop_data6=%x  iop_data7=%x iop_data8=%x iop_data9=%x iop_data10=%x iop_data11=%x\n", __FUNCTION__, __LINE__, 
	//	pIopReg->iop_data6,pIopReg->iop_data7,pIopReg->iop_data8,pIopReg->iop_data9,pIopReg->iop_data10,pIopReg->iop_data11);


	
	pIopReg->iop_control&=~(0x8000);
	//pIopReg->iop_control&=~(0x200);
	pIopReg->iop_control|=0x0200;//disable watchdog event reset IOP


	pIopReg->iop_base_adr_l = (unsigned int) ((u32)(IOP_base) & 0xFFFF);
	pIopReg->iop_base_adr_h  =(unsigned int) ((u32)(IOP_base) >> 16);
	
	pIopReg->iop_control &=~(0x01);

	
	
}
EXPORT_SYMBOL(hal_iop_init);

#define IOP_READY	0x4
#define RISC_READY	0x8

void hal_iop_suspend(void __iomem *iopbase, void __iomem *ioppmcbase ,void __iomem *ioprtcbase)
{
	regs_iop_t *pIopReg = (regs_iop_t *)iopbase;
	regs_iop_pmc_t *pIopPmcReg = (regs_iop_pmc_t *)ioppmcbase;
	regs_iop_rtc_t *pIopRtcReg = (regs_iop_rtc_t *)ioprtcbase;
	unsigned int reg;


	volatile unsigned int*   IOP_base =(volatile unsigned int*)0x110000;

	//clock enable
	//reg = readl((void __iomem *)(B_SYSTEM_BASE + 32*4*0+ 4*1));
	//reg|=0x00100010;
	writel(0x00100010, (void __iomem *)(B_SYSTEM_BASE + 32*4*0+ 4*1));

	early_printk("Leon  hal_iop_suspend\n");
	//early_printk("[IOP iopbase: 0x%x  ioppmcbase: 0x%x   ioprtcbase: 0x%x!!\n", (unsigned int)(iopbase)
	//	,(unsigned int)(ioppmcbase),(unsigned int)(ioprtcbase));

	pIopReg->iop_control&=~(0x8000);	
	pIopReg->iop_control|=0x1;

	
		//RTC set

	early_printk("Leon  hal_iop_suspend rtc_timer_out=0x%x\n",pIopRtcReg->rtc_timer_out);
	
	//RTC set
	//*RTC_TIMER_SET = *RTC_TIMER_OUT;
	//*RTC_ALARM_SET = (unsigned int)(*RTC_TIMER_SET +1);


	pIopRtcReg->rtc_timer_set= 0x0;
	
	early_printk("Leon  hal_iop_suspend rtc_timer_set=0x%x\n",pIopRtcReg->rtc_timer_set);
	early_printk("Leon  hal_iop_suspend rtc_timer_set=0x%x\n",pIopRtcReg->rtc_timer_set);
	early_printk("Leon  hal_iop_suspend rtc_timer_set=0x%x\n",pIopRtcReg->rtc_timer_set);

	pIopRtcReg->rtc_alarm_set =0x100;

	early_printk("Leon  hal_iop_suspend rtc_alarm_set=0x%x\n",pIopRtcReg->rtc_alarm_set);
	early_printk("Leon  hal_iop_suspend rtc_alarm_set=0x%x\n",pIopRtcReg->rtc_alarm_set);
	early_printk("Leon  hal_iop_suspend rtc_alarm_set=0x%x\n",pIopRtcReg->rtc_alarm_set);

	pIopReg->iop_data5=0x00;
	pIopReg->iop_data6=0x60;


	//*RTC_CTRL|=0xffff0012;//disable system reset RTC, enable Alarm for PMC

	pIopRtcReg->rtc_ctrl|=0xffff0012;//disable system reset RTC, enable Alarm for PMC

	early_printk("Leon  hal_iop_suspend rtc_ctrl=0x%x\n",pIopRtcReg->rtc_ctrl);

	//PMC set
	//*PMC_TIMER=0x00010001; //0x0a0a


	//*PMC_TIMER2=0x1;
	//*PMC_TIMER2=0x10;

	//*PMC_CTRL|=0x23; // disable system reset PMC, enalbe power down 27M, enable gating 27M
	//*PMC_IOPDM_PWD_I=0x55aa00ff;
	//*PMC_IOPDM_PWD_II=0x00ff55aa;
	//*PMC_XTAL32K_PWD_I=0xaa00ff55;
	//*PMC_XTAL32K_PWD_II=0xff55aa00;
	//*PMC_CLK27M_PWD_I=0xaaff0055;
	//*PMC_CLK27M_PWD_II=0x5500aaff;
	pIopPmcReg->PMC_TIMER=0x00010001; //0x0a0a
	pIopPmcReg->PMC_CTRL|=0x23; // disable system reset PMC, enalbe power down 27M, enable gating 27M

	pIopPmcReg->XTAL27M_PASSWORD_I=0x55aa00ff;
	pIopPmcReg->XTAL27M_PASSWORD_II=0x00ff55aa;
	pIopPmcReg->XTAL32K_PASSWORD_I=0xaa00ff55;
	pIopPmcReg->XTAL32K_PASSWORD_II=0xff55aa00;
	pIopPmcReg->CLK27M_PASSWORD_I=0xaaff0055;
	pIopPmcReg->CLK27M_PASSWORD_II=0x5500aaff;
	pIopPmcReg->PMC_TIMER2=0x01000100;

	early_printk("Leon  hal_iop_suspend PMC_TIMER=0x%x\n",pIopPmcReg->PMC_TIMER);
	early_printk("Leon  hal_iop_suspend PMC_CTRL=0x%x\n",pIopPmcReg->PMC_CTRL);
	/*
	early_printk("Leon  hal_iop_suspend XTAL27M_PASSWORD_I=0x%x\n",pIopPmcReg->XTAL27M_PASSWORD_I);
	early_printk("Leon  hal_iop_suspend XTAL27M_PASSWORD_II=0x%x\n",pIopPmcReg->XTAL27M_PASSWORD_II);
	early_printk("Leon  hal_iop_suspend XTAL32K_PASSWORD_I=0x%x\n",pIopPmcReg->XTAL32K_PASSWORD_I);
	early_printk("Leon  hal_iop_suspend XTAL32K_PASSWORD_II=0x%x\n",pIopPmcReg->XTAL32K_PASSWORD_II);
	early_printk("Leon  hal_iop_suspend CLK27M_PASSWORD_I=0x%x\n",pIopPmcReg->CLK27M_PASSWORD_I);
	early_printk("Leon  hal_iop_suspend CLK27M_PASSWORD_II=0x%x\n",pIopPmcReg->CLK27M_PASSWORD_II);
	early_printk("Leon  hal_iop_suspend PMC_TIMER2=0x%x\n",pIopPmcReg->PMC_TIMER2);
	*/


	//writel(0x25, (void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20));

	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));
	//early_printk("Leon  hal_iop_suspend alarm_set=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE +32*4*116 + 4 * 20)));

	//IOP Hardware IP reset
	//*mo_reset0|=0x10;
	//*mo_reset0&=~(0x10);

	//pIopMoon0Reg->reset[0]|=0x10;
	//pIopMoon0Reg->reset[0]&=~(0x10);
	reg = readl((void __iomem *)(B_SYSTEM_BASE + 4 * 21));
	reg|=0x10;
	writel(reg, (void __iomem *)(B_SYSTEM_BASE + 4 * 21));
	reg&=~(0x10);
	writel(reg, (void __iomem *)(B_SYSTEM_BASE + 4 * 21));


	writel(0x00ff0085, (void __iomem *)(B_SYSTEM_BASE + 32*4*1+ 4*1));
	
	reg = readl((void __iomem *)(B_SYSTEM_BASE + 32*4*1+ 4*2));
	reg|=0x08000800;
	writel(reg, (void __iomem *)(B_SYSTEM_BASE + 32*4*1+ 4*2));

	/*	
	early_printk("Leon  g1.0=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*1+ 4*0)));
	early_printk("Leon  g1.1=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*1+ 4*1)));
	early_printk("Leon  g1.2=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*1+ 4*2)));
	early_printk("Leon  g1.3=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*1+ 4*3)));
	early_printk("Leon  g1.4=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*1+ 4*4)));
	*/
	
	//regs0->iop_control&=~(0x0200);//disable watchdog event reset IOP
	//*iop_control|=0x0200;//disable watchdog event reset IOP

	pIopReg->iop_control|=0x0200;//disable watchdog event reset IOP

	pIopReg->iop_base_adr_l = (unsigned int) ((u32)(IOP_base) & 0xFFFF);
	pIopReg->iop_base_adr_h=(unsigned int) ((u32)(IOP_base) >> 16);

	
	//*iop_control &=~(0x01);
	pIopReg->iop_control&=~(0x01);

	//early_printk("%s(%d) iop_data0=%x  iop_data1=%x iop_data2=%x iop_data3=%x iop_data4=%x iop_data5=%x\n", __FUNCTION__, __LINE__, 
	//	pIopReg->iop_data0,pIopReg->iop_data1,pIopReg->iop_data2,pIopReg->iop_data3,pIopReg->iop_data4,pIopReg->iop_data5);
	
	//early_printk("%s(%d) iop_data6=%x  iop_data7=%x iop_data8=%x iop_data9=%x iop_data10=%x iop_data11=%x\n", __FUNCTION__, __LINE__, 
	//	pIopReg->iop_data6,pIopReg->iop_data7,pIopReg->iop_data8,pIopReg->iop_data9,pIopReg->iop_data10,pIopReg->iop_data11);

	while((pIopReg->iop_data2&IOP_READY)!=IOP_READY);
		pIopReg->iop_data2|=RISC_READY;
	/*
	early_printk("%s(%d) iop_data0=%x  iop_data1=%x iop_data2=%x iop_data3=%x iop_data4=%x iop_data5=%x\n", __FUNCTION__, __LINE__, 
		pIopReg->iop_data0,pIopReg->iop_data1,pIopReg->iop_data2,pIopReg->iop_data3,pIopReg->iop_data4,pIopReg->iop_data5);
	
	early_printk("%s(%d) iop_data6=%x  iop_data7=%x iop_data8=%x iop_data9=%x iop_data10=%x iop_data11=%x\n", __FUNCTION__, __LINE__, 
		pIopReg->iop_data6,pIopReg->iop_data7,pIopReg->iop_data8,pIopReg->iop_data9,pIopReg->iop_data10,pIopReg->iop_data11);
	*/
	while(1)
	{
		if(pIopReg->iop_data7==0xaaaa)
		{
			break;
		}
		/*
		early_printk("%s(%d) iop_data0=%x  iop_data1=%x iop_data2=%x iop_data3=%x iop_data4=%x iop_data5=%x\n", __FUNCTION__, __LINE__, 
			pIopReg->iop_data0,pIopReg->iop_data1,pIopReg->iop_data2,pIopReg->iop_data3,pIopReg->iop_data4,pIopReg->iop_data5);
		
		early_printk("%s(%d) iop_data6=%x  iop_data7=%x iop_data8=%x iop_data9=%x iop_data10=%x iop_data11=%x\n", __FUNCTION__, __LINE__, 
			pIopReg->iop_data6,pIopReg->iop_data7,pIopReg->iop_data8,pIopReg->iop_data9,pIopReg->iop_data10,pIopReg->iop_data11);
		*/
	}
	/*
	early_printk("%s(%d) iop_data0=%x  iop_data1=%x iop_data2=%x iop_data3=%x iop_data4=%x iop_data5=%x\n", __FUNCTION__, __LINE__, 
		pIopReg->iop_data0,pIopReg->iop_data1,pIopReg->iop_data2,pIopReg->iop_data3,pIopReg->iop_data4,pIopReg->iop_data5);
	
	early_printk("%s(%d) iop_data6=%x  iop_data7=%x iop_data8=%x iop_data9=%x iop_data10=%x iop_data11=%x\n", __FUNCTION__, __LINE__, 
		pIopReg->iop_data6,pIopReg->iop_data7,pIopReg->iop_data8,pIopReg->iop_data9,pIopReg->iop_data10,pIopReg->iop_data11);
	*/
	/*
	early_printk("Leon  g30.0=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*0)));
	early_printk("Leon  g30.1=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*1)));
	early_printk("Leon  g30.2=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*2)));
	early_printk("Leon  g30.3=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*3)));
	early_printk("Leon  g30.4=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*4)));
	early_printk("Leon  g30.5=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*5)));
	early_printk("Leon  g30.6=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*6)));
	early_printk("Leon  g30.7=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*7)));
	early_printk("Leon  g30.8=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*8)));
	early_printk("Leon  g30.9=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*9)));
	early_printk("Leon  g30.10=0x%x\n",readl((void __iomem *)(B_SYSTEM_BASE + 32*4*30+ 4*10)));
	*/



	
	//pIopReg->iop_data1=0xdd;
	/*
	early_printk("%s(%d) iop_data0=%x  iop_data1=%x iop_data2=%x iop_data3=%x iop_data4=%x iop_data5=%x\n", __FUNCTION__, __LINE__, 
		pIopReg->iop_data0,pIopReg->iop_data1,pIopReg->iop_data2,pIopReg->iop_data3,pIopReg->iop_data4,pIopReg->iop_data5);
	
	early_printk("%s(%d) iop_data6=%x  iop_data7=%x iop_data8=%x iop_data9=%x iop_data10=%x iop_data11=%x\n", __FUNCTION__, __LINE__, 
		pIopReg->iop_data6,pIopReg->iop_data7,pIopReg->iop_data8,pIopReg->iop_data9,pIopReg->iop_data10,pIopReg->iop_data11);
	*/
	early_printk("Leon  hal_iop_suspend end\n");
	printk("Leon  hal_iop_suspend end\n");

	
}
EXPORT_SYMBOL(hal_iop_suspend);

