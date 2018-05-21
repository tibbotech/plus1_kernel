#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <linux/types.h>
#include <sys/ioctl.h> 

#include "../../../include/mach/gpio_drv.h"

int sp_gpio_fd = -1;

void _pinmux_sel(PMXSEL_ID pinmux_id, unsigned int pinmux_num)
{
	PMXSEL_T pin_mux_sel;
	// config pin mux to connect real pins.
	pin_mux_sel.id = pinmux_id;
	pin_mux_sel.val= pinmux_num;
	printf("aa: %d pin_mux_sel.val:%d \n", pinmux_num , pin_mux_sel.val);
	ioctl(sp_gpio_fd, GPIO_IOC_PIN_MUX_SEL,  &pin_mux_sel);
}

void _pinmux_get(PMXSEL_ID pinmux_id, unsigned char pinmux_num)
{	
	PMXSEL_T pin_mux_sel;
	// config pin mux to connect real pins.
	pin_mux_sel.id = pinmux_id;
	pin_mux_sel.val= 100;	
	printf("Get pin_mux_g.val 1 : %d\n", pin_mux_sel.val);
	ioctl(sp_gpio_fd, GPIO_IOC_PIN_MUX_GET,  &pin_mux_sel.val);
	printf("Get pin_mux_g.val 2: %d\n", pin_mux_sel.val);
}


static int get_pmx_id(char* name, unsigned int *id)
{
	int ret =0;
	if(strcmp(name,"PMX_L2SW_CLK_OUT")==0) *id=PMX_L2SW_CLK_OUT;
	else if(strcmp(name,"PMX_L2SW_LED_FLASH0")==0) *id=PMX_L2SW_LED_FLASH0;
	else if(strcmp(name,"PMX_L2SW_LED_FLASH1")==0) *id=PMX_L2SW_LED_FLASH1;
	else if(strcmp(name,"PMX_L2SW_LED_FLASH1")==0) *id=PMX_L2SW_LED_FLASH1;
	else if(strcmp(name,"PMX_GPIO_INT0")==0) *id=PMX_GPIO_INT0;
	else if(strcmp(name,"PMX_GPIO_INT1")==0) *id=PMX_GPIO_INT1;
	else if(strcmp(name,"PMX_GPIO_INT2")==0) *id=PMX_GPIO_INT2;
	else if(strcmp(name,"PMX_GPIO_INT3")==0) *id=PMX_GPIO_INT3;
	else if(strcmp(name,"PMX_GPIO_INT4")==0) *id=PMX_GPIO_INT4;	
	else if(strcmp(name,"PMX_UA1_TX")==0) *id=PMX_UA1_TX;	
	else if(strcmp(name,"PMX_UA2_TX")==0) *id=PMX_UA2_TX;	
	else ret = -1;
	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, const char **argv)
{
	char cust_pmx_str[32];
	unsigned int pmx_id, pmx_sel;
	unsigned int ret;


	if (argc <= 1){
		printf("pinmux_test.x [module] [pinmux select]\n");
		printf("[module] : PMX_I2CM0; PMX_I2CDDC0_A ... etc\n");
		printf("[pinmux select] : 0; 1; 2 ... etc\n");
		printf("\n module name ref. gpio_drv.h\n");
		return 0;
	}
	else if(strcmp(argv[1], "-h")==0){
		printf("pinmux_test.x [module] [pinmux select]\n");
		printf("[module] : PMX_I2CM0; PMX_I2CDDC0_A ... etc\n");
		printf("[pinmux select] : 0; 1; 2 ... etc\n");
		printf("\n module name ref. gpio_drv.h\n");
		return 0;
	}

	sprintf(cust_pmx_str, argv[1]);

	ret = get_pmx_id(cust_pmx_str, &pmx_id);
	if (ret != 0){
		printf("pinmux_test fail -- unknow module.\n");
		return 0;	
	}

	sscanf(argv[2],"%d",&pmx_sel);

	sp_gpio_fd = open("/dev/gpio", O_RDWR); 
	if(sp_gpio_fd < 0){ 
		printf("Error: opening /dev/sp_gpio fail!\n"); 
		return -1;
	}
	
	//if ((strcmp(argv[3], "g") == 0)||(strcmp(argv[3], "g") == 0)){		//
	//	_pinmux_get(pmx_id, pmx_sel);
	//}
	//else
	//{
		_pinmux_sel(pmx_id, pmx_sel);
		printf("_pinmux_sel sel 0 done\n"); 
		
	//}
	

	close(sp_gpio_fd);
	return 0; 
}
