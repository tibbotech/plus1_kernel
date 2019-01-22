
/*
 * MIPI CSI driver
 *
 * Copyright (C) Sunplus Corporation 
 *
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <mach/gpio_drv.h>
#include <mach/io_map.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <mach/mipi_camera.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>
   

/* ------------------------------------------------------------------
	definition
   ------------------------------------------------------------------*/

 
#define BYTE unsigned char


// gpio number for I2C
#define I2C_SDA     0
#define I2C_SCL     1
#define I2C_RST     2


#define I2C_delayTime 50
#define I2C_SCL_SET(d)  GPIO_O_SET(I2C_SCL, d)
#define I2C_SDA_SET(d)  GPIO_O_SET(I2C_SDA, d)
#define I2C_SDA_GET()   GPIO_I_GET(I2C_SDA)
#define I2C_SCL_GET()   GPIO_I_GET(I2C_SCL)
#define I2C_SCL_IN()    GPIO_E_SET(I2C_SCL, 0)
#define I2C_SDA_IN()    GPIO_E_SET(I2C_SDA, 0)
#define I2C_SCL_OUT()   GPIO_E_SET(I2C_SCL, 1)
#define I2C_SDA_OUT()   GPIO_E_SET(I2C_SDA, 1)
#define I2C_SDA_H()     GPIO_E_SET(I2C_SDA, 0)
#define I2C_SDA_L()     {I2C_SDA_SET(0); GPIO_E_SET(I2C_SDA, 1);}
#define I2C_SCL_H()     GPIO_E_SET(I2C_SCL, 0)
#define I2C_SCL_L()     {I2C_SCL_SET(0); GPIO_E_SET(I2C_SCL, 1);}
#define I2C_RST_H()     GPIO_E_SET(I2C_RST, 0)
#define I2C_RST_L()     {I2C_SCL_SET(0); GPIO_E_SET(I2C_SCL, 1);}


// regs base
void __iomem *regs = (void __iomem *)B_SYSTEM_BASE;



// camera parameters
#define CAM_WIDHT 1280
#define CAM_HEIGHT 800
#define CAM_PIX_FMT V4L2_PIX_FMT_GREY


/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct vivi_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

//ar structure include v4l2_device
struct vivi 
{
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	unsigned int width, height;
	struct vivi_fmt fmt;  
};


//static ardev var
static struct vivi vividev;


struct CAM_PIC_info CPI;
struct CAM_PIC_info *pstMipi_cpi = NULL;


/* ------------------------------------------------------------------
	FUNCTION i2c for MIPI camear register r/w
   ------------------------------------------------------------------*/


/*******************************************************************************
 * Function:       i2c_init_io_risc
 * Description:    
 * Calls:          
 * Called By:      
 * Input:          
 * Output:         
 * Return:         
 * Others:         
 * History Information Description:         
 * Date                      Author                          Modification         
*******************************************************************************/
void i2c_init_io_risc(void)
{
	GPIO_F_SET(I2C_SDA, 1);
	GPIO_F_SET(I2C_SCL, 1);
	GPIO_F_SET(I2C_RST, 1);
	GPIO_M_SET(I2C_SDA, 1);
	GPIO_M_SET(I2C_SCL, 1);
	GPIO_M_SET(I2C_RST, 1);
}

/*******************************************************************************
 * Function:       delay_gpio
 * Description:    
 * Calls:          
 * Called By:      
 * Input:          
 * Output:         
 * Return:         
 * Others:         
 * History Information Description:         
 * Date                      Author                         Modification         
*******************************************************************************/
#define I2C_SPEED_DEFAULT 180
static int I2cSpeed = I2C_SPEED_DEFAULT;

void SetI2cSpeed(int speed)
{
	if (speed > 0)
		I2cSpeed = speed;
	else
		I2cSpeed = I2C_SPEED_DEFAULT;
}

static void delay_gpio(int i)
{
	do
	{
		int j = I2cSpeed;
		do
		{
			asm volatile ("nop");
		}
		while (--j >= 0);
	}
	while (--i >= 0);
}

#define delay_i2c(i)    delay_gpio(i)
#define i2c_delay()     delay_i2c(I2C_delayTime)

/*******************************************************************************
 * Function:       i2c_start_sig
 * Description:    SCL: --
 *                 SDA: -\_
 * Calls:          
 * Called By:      
 * Input:          
 * Output:         
 * Return:         
 * Others:         
 * History Information Description:         
 * Date                      Author                         Modification         
*******************************************************************************/
void i2c_start_sig(void)
{
    i2c_init_io_risc();
	I2C_SCL_H();
	I2C_SDA_H();
	delay_i2c(I2C_delayTime);
	I2C_SDA_L();
	delay_i2c(I2C_delayTime);

}

/*******************************************************************************
 * Function:       i2c_byte_w
 * Description:    SCL: _/-\_/-\_/-\_/-\_/-\_/-\_/-\_/-\_/-\_
 *                 SDA: _<D7><D6><D5><D4><D3><D2><D1><D0>ACK__
 
 * Calls:          
 * Called By:      
 * Input:          
 * Output:         
 * Return:         
 * Others:         
 * History Information Description:         
 * Date                      Author                         Modification         
*******************************************************************************/
int i2c_byte_w(int data)
{

	int i;
	
    i2c_init_io_risc();
	I2C_SCL_L()

	delay_i2c(I2C_delayTime);
	
	for (i = 8; i; i--)
	{
		if (data & 0x80)
		{
			I2C_SDA_H();
		}
		else
		{
			I2C_SDA_L();
		}
		delay_i2c(I2C_delayTime);     // scl -- low
		I2C_SCL_H();
		delay_i2c(I2C_delayTime); //hawk	// scl -> high
		data <<= 1;
		I2C_SCL_L();    // scl -> low
	}

	// Get ACK
	I2C_SDA_IN();
	delay_i2c(I2C_delayTime);           // scl -- low
	I2C_SCL_H();
	delay_i2c(I2C_delayTime);           // scl -> high
	i = I2C_SDA_GET(); 		            // 0 for no error
//	printk("==== state=%d  ",i);
	I2C_SCL_L();
	delay_i2c(I2C_delayTime);           // scl -> low
	
	return ((i) ? 1 : 0);  // i2c ack must zero from slave
}

/*******************************************************************************
 * Function:       i2c_stop_sig
 * Description:    SCL: _/-
 *                 SDA: __/
 * Calls:          
 * Called By:      
 * Input:          
 * Output:         
 * Return:         
 * Others:         
 * History Information Description:         
 * Date                      Author                         Modification         
*******************************************************************************/
void i2c_stop_sig(void)
{
    i2c_init_io_risc();
	I2C_SDA_L();
	I2C_SCL_L();
	delay_i2c(I2C_delayTime);
	I2C_SCL_H();
	delay_i2c(I2C_delayTime);
	I2C_SDA_H();

}

/*******************************************************************************
 * Function:       i2c_stop
 * Description:    
 * Calls:          
 * Called By:      
 * Input:          
 * Output:         
 * Return:         
 * Others:         
 * History Information Description:         
 * Date                      Author                         Modification         
*******************************************************************************/
void i2c_stop(void)
{
    i2c_init_io_risc();
	I2C_SCL_L();
	I2C_SDA_L();
	i2c_delay();
	I2C_SCL_H();
	i2c_delay();
	I2C_SDA_H();
	i2c_delay();
	I2C_SCL_L();
	i2c_delay();
	I2C_SCL_H();
	I2C_SDA_H();

}

/*******************************************************************************
 * Function:       init_i2c
 * Description:    
 * Calls:          
 * Called By:      
 * Input:          
 * Output:         
 * Return:         
 * Others:         
 * History Information Description:         
 * Date                      Author                         Modification         
*******************************************************************************/
void init_i2c(void)
{
    i2c_init_io_risc();
	I2C_SCL_H();
	I2C_SDA_H();
	i2c_delay();
	i2c_stop();

}

/*******************************************************************************
 * Function:       i2c_byte_r
 * Description:    SCL: _/-\_/-\_/-\_/-\_/-\_/-\_/-\_/-\_/-\_
 *                 SDA: _<D7><D6><D5><D4><D3><D2><D1><D0>ACK____
 * Calls:          
 * Called By:      
 * Input:          
 * Output:         
 * Return:         
 * Others:         
 * History Information Description:         
 * Date                      Author                         Modification         
*******************************************************************************/
unsigned char i2c_byte_r(BYTE ack)
{
	int i;
	unsigned char data = 0;

    i2c_init_io_risc();
	I2C_SCL_L();
	I2C_SDA_IN(); 

	for (i = 8; i; i--)
	{
		delay_i2c(I2C_delayTime);
		I2C_SCL_H();   // scl -> high
		delay_i2c(I2C_delayTime);
		data = (data << 1) | (I2C_SDA_GET() ? 1 : 0);
		I2C_SCL_L();    // scl -> low
	}


	delay_i2c(I2C_delayTime);

	// Set ACK !!
	if (ack == 1)                       // not ack
	{
		I2C_SDA_H();					// scl -- low
	}
	else
	{
		I2C_SDA_L();					// scl -- low
	}
	delay_i2c(I2C_delayTime);           // scl -- low
	I2C_SCL_H();
	delay_i2c(I2C_delayTime);           // scl -> high
	I2C_SCL_L();
	delay_i2c(I2C_delayTime);           // scl -> low

    
	return data;
}


int ReadFromI2c(int iDeviceAddr, int iAddr, BYTE *bBuffer)
{
    unsigned char data = 0;

	
    i2c_start_sig();
	
    if(0==i2c_byte_w(iDeviceAddr&0xfe))
    {
		if(0==i2c_byte_w((iAddr>>8)&0xff))		
		{
			if(0==i2c_byte_w(iAddr&0xff))
			{
			
					i2c_start_sig();
				   
					if(0==i2c_byte_w(iDeviceAddr|1))
                    {
                        data = i2c_byte_r(1);
                        i2c_stop_sig();
                    }
			}
		}
	}

	i2c_stop_sig();

//	printk("========ReadFromI2c data2: %x %x\n",iAddr,data);

   *bBuffer = data;

    return 1;
}



int WriteToI2c(int iDeviceAddr, int iAddr, BYTE *bData)
{
    u32 loop;


//	printk("========WriteToI2c data:%x %x\n",iAddr,*bData);
	
    i2c_start_sig();

	if(0==i2c_byte_w(iDeviceAddr&0xfe))
	{
		if(0==i2c_byte_w((iAddr>>8)&0xff))		
		{
			if(0==i2c_byte_w(iAddr&0xff))
			{

				if(0==i2c_byte_w(*bData))
	            {
            
					i2c_stop_sig();
            
	            }
			}

        } 
	}


	i2c_stop_sig();

    return 1;
}


//======================================================


u32 buffer_anchor[5];
u32 buffer_status[5];
u32 buffer_count;
u32 buffer_size;
u32 buffer_streaming = 0;


void MIPICAM_IMG_Capture(struct CAM_PIC_info *pstCPI)
{

	pstMipi_cpi = pstCPI;

	writel(mEXTENDED_ALIGNED(pstMipi_cpi->width,16), regs + (4*32*(166) + 4*(3)));  // line pitch
	writel(pstMipi_cpi->height<<16|pstMipi_cpi->width, regs + (4*32*(166) + 4*(4))); // hv size
	writel(pstMipi_cpi->buffer_anchor, regs + (4*32*(166) + 4*(2)));  				// base address

	writel(0x2701, regs + (4*32*(166) + 4*(1)));
	pstMipi_cpi->capture_status  = CAM_Capture_BEGIN;

	//printk("==========begin\n"); 
	
}

irqreturn_t mipicsiiw_field_start(int irq, void *dev_instance) 
{ 

//	printk("%s: irq %d dev_instance %p\n", __func__, irq, dev_instance); 

	if(buffer_streaming)
	{
	
		if(buffer_count)
		{
		
			if(buffer_status[0]==0)
			{

				//printk("==========CAM_Capture_status:%d\n",CPI.capture_status); 

				if((CPI.capture_status  == CAM_Capture_NONE) || (CPI.capture_status  == CAM_Capture_DONE))
				{		
					CPI.buffer_anchor = buffer_anchor[0];
					MIPICAM_IMG_Capture(&CPI);				
				}
			}						
		}
	}

	return IRQ_NONE; 
	
} 

irqreturn_t mipicsiiw_field_end(int irq, void *dev_instance) 
{ 

//	printk("%s: irq %d dev_instance %p\n", __func__, irq, dev_instance); 

	if(pstMipi_cpi!=NULL)
	{

			// when csiiw enable , it apply at field start to wrtie data and field end interrupt
			// will occurs when data write finish				
			
			if(pstMipi_cpi->capture_status == CAM_Capture_BEGIN)	
			{			
				writel(0x2700, regs + (4*32*(166) + 4*(1)));
				pstMipi_cpi->capture_status  = CAM_Capture_DONE;
				pstMipi_cpi = NULL;			
				buffer_status[0] = 1;

				//printk("==========done\n"); 			
			}
	}

	return IRQ_NONE; 
} 


u32 irq_fs_num;
u32 irq_fe_num;
const int IRQ_NUM = 50; 
void *irq_dev_id = (void *)&IRQ_NUM; 


static int MIPI_CSIIW_intr_init(void)
{ 

	u32 result;
	int ret; 
	struct device_node *np = NULL; 

	np = of_find_compatible_node(NULL, NULL, "sunplus,mipicsi"); 

	irq_fs_num = irq_of_parse_and_map(np, 0);  
			
	ret = request_irq(irq_fs_num, mipicsiiw_field_start, IRQF_SHARED, "MIPICSIIW0_FS", irq_dev_id); 
	
	if (ret) { 
	printk("request_irq() failed (%d)\n", ret); 
	return (ret); 
	} 

	irq_fe_num = irq_of_parse_and_map(np, 1);  

	ret = request_irq(irq_fe_num, mipicsiiw_field_end, IRQF_SHARED, "MIPICSIIW0_FE", irq_dev_id); 
	
	if (ret) { 
	printk("request_irq() failed (%d)\n", ret); 
	return (ret); 
	} 


	printk("MIPICSIIW interrupt installed.\n"); 
	
	return 0; 

}

void MIPI_camera_init(void)
{

	int i, num_regs;
	BYTE data;


	// camera regsiter setup
	
	num_regs = sizeof(ov9281_regs_default)/sizeof(ov9281_regs_default[0]);

	for (i = 0; i < num_regs; i++) 
	{
	
		WriteToI2c(0xc0,ov9281_regs_default[i].reg,&ov9281_regs_default[i].val);
	
	}

	for (i = 0; i < num_regs; i++) 
	{
	
		ReadFromI2c(0xc0,ov9281_regs_default[i].reg,&data);
	
	}


	// global camera parameter setup	
	CPI.output_format = CAM_PIX_FMT;
	CPI.width = CAM_WIDHT;
	CPI.height = CAM_HEIGHT;
	CPI.line_pitch = mEXTENDED_ALIGNED(CPI.width,16);

}


// MIPI CSI analog/logic circuit init.
void MIPI_CSI_init(void)
{

	writel(0x1f, regs + (4*32*(165) + 4*(6))); 
	//raw10
	writel(0x118104, regs + (4*32*(165) + 4*(8))); 
	//raw8
	//writel(0x128104, regs + (4*32*(165) + 4*(8))); 

	writel(0X1000, regs + (4*32*(165) + 4*(4))); 
	writel(0X1001, regs + (4*32*(165) + 4*(4))); 
	writel(0X1000, regs + (4*32*(165) + 4*(4))); 

	//raw10
	writel(0X2B, regs + (4*32*(165) + 4*(12))); 
	//raw8
	//writel(0X2A, regs + (4*32*(165) + 4*(12))); 


}


// MIPI CSIIW init.
void MIPI_CSIIW_init(void)
{

	writel(0x1, regs + (4*32*(166) + 4*(0)));   // latch mode should be enable before base address setup

	//writel(handle, regs + (4*32*(166) + 4*(2)));  // base address
	writel(0x500, regs + (4*32*(166) + 4*(3))); 
	writel(0x3200500, regs + (4*32*(166) + 4*(4))); 

	// 3 buffer
	//writel(0x021f4000, regs + (4*32*(166) + 4*(5))); 
	// 2 buffer
	//writel(0x011f4000, regs + (4*32*(166) + 4*(5))); 
	// 1 buffer
	writel(0x00000100, regs + (4*32*(166) + 4*(5)));  // set offset to trigger dram writer

	//raw10
	// 10bit two byte space
	//writel(0x2731, regs + (4*32*(166) + 4*(1))); 
	// 8bit one byte space
	//writel(0x2701, regs + (4*32*(166) + 4*(1))); 

	//raw8
	//writel(0x2701, regs + (4*32*(166) + 4*(1))); 

	//CPI.buffer_anchor = handle;


}


static int __init MIPICSI_vivi_init(void);
static void __exit MIPICSI_vivi_exit(void);


static int mipi_init(void)
{


	// i2c init
	i2c_init_io_risc();

	// mipicsi init
	MIPI_camera_init();
	MIPI_CSI_init();
	MIPI_CSIIW_init();
	MIPI_CSIIW_intr_init();

	// v4L2 init
	MIPICSI_vivi_init();

}

static void mipi_exit(void)
{

	free_irq(irq_fs_num, irq_dev_id); 
	free_irq(irq_fe_num, irq_dev_id); 

	MIPICSI_vivi_exit();

}


module_init(mipi_init);
module_exit(mipi_exit);




//===================================================================================   
/* ------------------------------------------------------------------
	IOCTL vidioc handling
   ------------------------------------------------------------------*/
   
static int vidioc_querycap(struct file *file,void  *priv,struct v4l2_capability *vcap)
{

	struct vivi *ar = video_drvdata(file); 
	strlcpy(vcap->driver, ar->vdev.name, sizeof(vcap->driver));

	vcap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
	   
	strlcpy(vcap->card, "MIPI Camera Card", sizeof(vcap->card));
	strlcpy(vcap->bus_info, "MIPI Camera BUS", sizeof(vcap->bus_info));
	vcap->capabilities = vcap->device_caps| V4L2_CAP_DEVICE_CAPS ; // report capabilities
   
   return 0;
}




static int vidioc_g_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
	
	struct vivi *dev = video_drvdata(file);

	f->fmt.pix.width        = dev->width;
	f->fmt.pix.height       = dev->height;
	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat  = dev->fmt.fourcc;
	f->fmt.pix.bytesperline = mEXTENDED_ALIGNED(f->fmt.pix.width,16) * dev->fmt.depth;
	f->fmt.pix.sizeimage =		f->fmt.pix.height * f->fmt.pix.bytesperline;
	
	if (dev->fmt.fourcc == V4L2_PIX_FMT_YUYV ||
	    dev->fmt.fourcc == V4L2_PIX_FMT_UYVY)
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	
	return 0;
}




static int MIPICSI_reqbufs(struct v4l2_requestbuffers *req )
{

	u8 *buf_va=NULL;
	u32 handle=NULL;
	u32 i;
	
	struct device *dev;

	if(req->count>1)
	return -1;

	buffer_count = req->count;
	buffer_size = CAM_WIDHT*CAM_HEIGHT;

	for(i=0;i<req->count;i++)
	{

		buf_va = dma_alloc_coherent(dev, buffer_size , &buffer_anchor[i], GFP_KERNEL);

		printk("========MIPICSI_reqbufs : %x VA:%x PA:%x\n",i,buf_va,buffer_anchor[i]);

		if(buf_va==NULL)
		return -1;

	}

	return 1;

}

static int MIPICSI_querybuf(struct v4l2_buffer *b)
{

	struct device *dev;

	if(b->index > buffer_count)
	return -1;

	b->length = buffer_size;
	b->m.offset = buffer_anchor[b->index];

	return 1;

}

static int MIPICSI_qbuf(struct v4l2_buffer *b)
{
	
	struct device *dev;

	if(b->index > buffer_count)
	return -1;

	buffer_status[b->index] = 0;

	return 1;
}

int MIPICSI_streamon(enum v4l2_buf_type type)
{


	if (buffer_streaming) {
		printk("already streaming\n");
		return 0;
	}

	if (!buffer_count) {
		printk("no buffers have been allocated\n");
		return -EINVAL;
	}

	buffer_streaming = 1;

	return 1;

}

int MIPICSI_streamoff(enum v4l2_buf_type type)
{

	buffer_streaming = 0;

	return 1;

}



int MIPICSI_dqbuf(struct v4l2_buffer *b)
{


	if(buffer_streaming==0)
	return -1;
	
	if(b->index > buffer_count)
	return -1;


	for(;;)
	{
		// wait for buffer filling 
		if(buffer_status[0]==1)
		break;

	}


	b->index = 0;

	return 1;

}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{

	struct vivi *dev = video_drvdata(file);
	return MIPICSI_reqbufs(p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{

	struct vivi *dev = video_drvdata(file);
	return MIPICSI_querybuf(p);
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{

	struct ar *dev = video_drvdata(file);
	return MIPICSI_qbuf(p);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{

	struct ar *dev = video_drvdata(file);
	return MIPICSI_dqbuf(p);
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{

	struct ar *dev = video_drvdata(file);
	return MIPICSI_streamon(i);
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{

	struct ar *dev = video_drvdata(file);
	return MIPICSI_streamoff(i);
}


/****************************************************************************
*
* Video4Linux Module functions
*
****************************************************************************/
   
static const struct v4l2_file_operations vivi_fops = 
{
	.owner			= THIS_MODULE,
	.open			= v4l2_fh_open,   // open /dev/video0 will enter here
	.release		= v4l2_fh_release,	 // close fd will enter here	
	.unlocked_ioctl	= video_ioctl2,	// need for ioctl
	
};
   
static const struct v4l2_ioctl_ops vivi_ioctl_ops = 
{
	.vidioc_querycap		= vidioc_querycap,  // ioctl VIDIOC_QUERYCAP will enter here
	.vidioc_g_fmt_vid_cap	= vidioc_g_fmt_vid_cap,
	.vidioc_reqbufs			= vidioc_reqbufs,
	.vidioc_querybuf		= vidioc_querybuf,
	.vidioc_qbuf			= vidioc_qbuf,
	.vidioc_dqbuf			= vidioc_dqbuf,
	.vidioc_streamon		= vidioc_streamon,
	.vidioc_streamoff		= vidioc_streamoff,
};

static int __init MIPICSI_vivi_init(void)
{	

	struct vivi *ar;
	struct v4l2_device *v4l2_dev;
	int ret;  

	ar = &vividev;
	v4l2_dev = &ar->v4l2_dev;
	
	//init v4l2 name , version	
	strlcpy(v4l2_dev->name, "MIPI V4L2 Camera driver", sizeof(v4l2_dev->name));
	ret = v4l2_device_register(NULL, v4l2_dev);
	
	if (ret < 0) 
	{
		printk(KERN_INFO "Could not register v4l2_device\n");
		return ret;
	}
	   
	//setup video
	strlcpy(ar->vdev.name, "vivi Driver", sizeof(ar->vdev.name));
	ar->vdev.v4l2_dev = v4l2_dev;   // set V4l2_device address to video_device 
	ar->vdev.fops = &vivi_fops;   //v4l2_file_operations
	ar->vdev.ioctl_ops = &vivi_ioctl_ops;	 //v4l2_ioctl_ops
	ar->vdev.release = video_device_release_empty;
	ar->width = CPI.width;
	ar->height = CPI.height;
	ar->fmt.fourcc = V4L2_PIX_FMT_GREY;
	ar->fmt.depth = 1;
	
	video_set_drvdata(&ar->vdev, ar);
   
	if (video_register_device(&ar->vdev, VFL_TYPE_GRABBER, -1) != 0) {
	   /* return -1, -ENFILE(full) or others */
	   printk(KERN_INFO "[mark]%s, %d, video_register_device FAIL \n", __func__,__LINE__);
	   ret = -ENODEV;
	   goto out_dev;
	}	
	
	printk(KERN_INFO "==================%s, %d, module inserted\n", __func__,__LINE__);
	 
	return 0;
   
out_dev:
	v4l2_device_unregister(&ar->v4l2_dev);
	video_unregister_device(&ar->vdev);
	   
	return ret;
	   
	 
}
   
static void __exit MIPICSI_vivi_exit(void)
{

	struct vivi *ar;
	ar = &vividev;
	   
	printk(KERN_INFO "%s, %d, module remove\n", __func__,__LINE__);
	video_unregister_device(&ar->vdev);
	v4l2_device_unregister(&ar->v4l2_dev);
	   
}
   

