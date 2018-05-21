#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/types.h>
#include <sys/ioctl.h> 



int sp_gpio_fd = -1;

/*  GPIO  IOCTL code  */
#define GPIO_IOCTYPE                         'S'
#define GPIO_IOCNUM(n)                       (0x80 + n)

#define GPIO_IOC_FIRST_1    _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x10), unsigned long)
#define GPIO_IOC_FIRST_0    _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x11), unsigned long)
#define GPIO_IOC_MASTER_1   _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x12), unsigned long)
#define GPIO_IOC_MASTER_0   _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x13), unsigned long)
#define GPIO_IOC_SET_OE     _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x14), unsigned long)
#define GPIO_IOC_CLR_OE     _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x15), unsigned long)
#define GPIO_IOC_OUT_1      _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x16), unsigned long)
#define GPIO_IOC_OUT_0      _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x17), unsigned long)
#define GPIO_IOC_IN         _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x18), unsigned long)
#define GPIO_IOC_PIN_MUX_GET     _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x19), unsigned long)
#define GPIO_IOC_PIN_MUX_SEL     _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x20), unsigned long)
#define GPIO_IOC_INPUT_INVERT_1      _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x21), unsigned long)
#define GPIO_IOC_INPUT_INVERT_0      _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x22), unsigned long)
#define GPIO_IOC_OUTPUT_INVERT_1     _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x23), unsigned long)
#define GPIO_IOC_OUTPUT_INVERT_0     _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x24), unsigned long)
#define GPIO_IOC_OPEN_DRAIN_1        _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x25), unsigned long)
#define GPIO_IOC_OPEN_DRAIN_0        _IOWR(GPIO_IOCTYPE, GPIO_IOCNUM(0x26), unsigned long)

void gpio_input_get(unsigned int io_num)
{
	unsigned int level=io_num;
	ioctl(sp_gpio_fd, GPIO_IOC_FIRST_1,  &io_num);
	ioctl(sp_gpio_fd, GPIO_IOC_MASTER_1,  &io_num);
	ioctl(sp_gpio_fd, GPIO_IOC_CLR_OE,  &io_num);
	ioctl(sp_gpio_fd, GPIO_IOC_IN,  &level);

	printf("Get pin level : %d\n", level);
}

void gpio_output_set(unsigned int io_num, unsigned int level)
{
	printf("io_num : %d ; level : %d \n", io_num , level);
  
	ioctl(sp_gpio_fd, GPIO_IOC_FIRST_1,  &io_num);
	ioctl(sp_gpio_fd, GPIO_IOC_MASTER_1,  &io_num);
	if(level)
		ioctl(sp_gpio_fd, GPIO_IOC_OUT_1,  &io_num);
	else
		ioctl(sp_gpio_fd, GPIO_IOC_OUT_0,  &io_num);
}

void gpio_input_invert_set(unsigned int io_num, unsigned int level)
{
	printf("[II]io_num : %d ; level : %d \n", io_num , level);  
	ioctl(sp_gpio_fd, GPIO_IOC_FIRST_1,  &io_num);
	ioctl(sp_gpio_fd, GPIO_IOC_MASTER_1,  &io_num);
	if(level)
		ioctl(sp_gpio_fd, GPIO_IOC_INPUT_INVERT_1,  &io_num);
	else
		ioctl(sp_gpio_fd, GPIO_IOC_INPUT_INVERT_0,  &io_num);
}

void gpio_output_invert_set(unsigned int io_num, unsigned int level)
{
	printf("[OO]io_num : %d ; level : %d \n", io_num , level);  
	ioctl(sp_gpio_fd, GPIO_IOC_FIRST_1,  &io_num);
	ioctl(sp_gpio_fd, GPIO_IOC_MASTER_1,  &io_num);
	if(level)
		ioctl(sp_gpio_fd, GPIO_IOC_OUTPUT_INVERT_1,  &io_num);
	else
		ioctl(sp_gpio_fd, GPIO_IOC_OUTPUT_INVERT_0,  &io_num);
}

void gpio_open_drain_set(unsigned int io_num, unsigned int level)
{
	printf("[OO]io_num : %d ; level : %d \n", io_num , level);  
	ioctl(sp_gpio_fd, GPIO_IOC_FIRST_1,  &io_num);
	ioctl(sp_gpio_fd, GPIO_IOC_MASTER_1,  &io_num);
	if(level)
		ioctl(sp_gpio_fd, GPIO_IOC_OPEN_DRAIN_1,  &io_num);
	else
		ioctl(sp_gpio_fd, GPIO_IOC_OPEN_DRAIN_0,  &io_num);
}

static void print_usage(char *exec)
{
	printf("Usage : %s <ini file>\n", exec);
	printf("\n");
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, const char **argv)
{

	unsigned int  gpio_num, io_level;

	if(argc<3){
		printf("wrong parameter!\n");
		printf("how to use:\n");
		printf("gpio_number [o/i] [h/l]");
		printf("like: ./gpio_new 12 o l");
		return -1;
	}
	
	printf("argv[0]=%s\n", argv[0]);
	printf("argv[1]=%s\n", argv[1]);
	printf("argv[2]=%s\n", argv[2]);

	sscanf(argv[1], "%d", &gpio_num);
	printf("gpio_num=%d\n",gpio_num);
	sp_gpio_fd = open("/dev/gpio", O_RDWR); 
	printf("mhmh\n"); 
	if(sp_gpio_fd < 0)
	{ 
		printf("Error: opening /dev/sp_gpio fail!\n"); 
		return -1;
	}

	if ((strcmp(argv[2], "I") == 0)||(strcmp(argv[2], "i") == 0)){		//input
		gpio_input_get(gpio_num);
	}
	else if((strcmp(argv[2], "O") == 0)||(strcmp(argv[2], "o") == 0)){	//output
		sscanf(argv[3],"%d",&io_level);
		gpio_output_set(gpio_num, io_level);
	}
	else if((strcmp(argv[2], "a") == 0)||(strcmp(argv[2], "A") == 0)){	//input_invert
		sscanf(argv[3],"%d",&io_level);
		gpio_input_invert_set(gpio_num, io_level);
	}
	else if((strcmp(argv[2], "b") == 0)||(strcmp(argv[2], "B") == 0)){	//input_invert
		sscanf(argv[3],"%d",&io_level);
		gpio_output_invert_set(gpio_num, io_level);
	}
	else if((strcmp(argv[2], "c") == 0)||(strcmp(argv[2], "C") == 0)){	//input_invert
		sscanf(argv[3],"%d",&io_level);
		gpio_open_drain_set(gpio_num, io_level);
	}
	else{
		printf("gpio test fail -- paramater error.\n");
	}


	close(sp_gpio_fd);
	return 0; 
}
