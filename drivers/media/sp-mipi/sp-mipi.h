#include <media/v4l2-dev.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>


#define VOUT_NAME		        "sp_vout"
#define MIPICSI_REG_NAME        "mipicsi"
#define CSIIW_REG_NAME        	"csiiw"
#define VOUT_WIDTH		        1280
#define VOUT_HEIGHT		        800

#define norm_maxw() 	        1280
#define norm_maxh() 	        800

#define mEXTENDED_ALIGNED(w,n) \
    (w%n)?((w/n)*n+n):(w)


#define FUNC_DEBUG()    //printk(KERN_INFO "[MIPI] Debug: %s(%d)\n", __FUNCTION__, __LINE__)
#define DBG_INFO(fmt, args ...)    printk(KERN_INFO "[MIPI] Info: " fmt, ## args)
#define DBG_ERR(fmt, args ...)    printk(KERN_ERR "[MIPI] Error: " fmt, ## args)

//#define DBG_INFO_VB(fmt, args ...)    printk(KERN_INFO "[MIPI] Info: " fmt, ## args)
static void print_List(struct list_head *head){
    struct list_head *listptr;
    struct videobuf_buffer *entry; 
    printk("\n*********************************************************************************\n");
    printk("(HEAD addr =  %p, next = %p, prev = %p)\n",head, head->next, head->prev);
    list_for_each(listptr, head) {
        entry = list_entry(listptr, struct videobuf_buffer, stream);
        printk("list addr = %p | next = %p | prev = %p\n",
                &entry->stream, 
                entry->stream.next,  
                entry->stream.prev  );
    }
    printk("*********************************************************************************\n");
}

static unsigned int vid_limit = 16; //16M

typedef enum
{
	CAM_Capture_NONE = 0,
	CAM_Capture_BEGIN = 1,
	CAM_Capture_ACTIVE = 2,
	CAM_Capture_DONE = 3,
} DRV_CAMERA_Capture_Staute;

struct sp_vout_subdev_info {	
	char name[32];				/* Sub device name */	
	int grp_id;					/* Sub device group id */	
	struct i2c_board_info board_info;	/* i2c subdevice board info */
};

struct sp_vout_subdev_info sp_vout_sub_devs[] = {
	{
		.name = "ov9281",
		.grp_id = 0,
		.board_info = {
			I2C_BOARD_INFO("ov9281", 0x60),
		},
	}
};

struct sp_vout_config {
	
	int num_subdevs;					/* Number of sub devices connected to vpfe */	
	int i2c_adapter_id;					/* i2c bus adapter no */	
	struct sp_vout_subdev_info *sub_devs;	/* information about each subdev */	

};

struct sp_vout_config psp_vout_cfg = {
	.num_subdevs = ARRAY_SIZE(sp_vout_sub_devs),
	.i2c_adapter_id = 1,
	.sub_devs = sp_vout_sub_devs,
};

struct sp_vout_device {
	struct device 			*pdev;			/* parent device */
	struct video_device 	video_dev;
	struct videobuf_buffer  *cur_frm;		/* Pointer pointing to current v4l2_buffer */
	struct videobuf_buffer  *next_frm;		/* Pointer pointing to next v4l2_buffer */
	struct videobuf_queue   buffer_queue;	/* Buffer queue used in video-buf */
	struct list_head	    dma_queue;		/* Queue of filled frames */

	struct v4l2_device 		v4l2_dev;   
	struct v4l2_format 		fmt;            /* Used to store pixel format */		
	struct v4l2_rect 		crop;
	struct v4l2_rect 		win;
	struct v4l2_control 	ctrl;
	enum v4l2_buf_type 		type;
	enum v4l2_memory 		memory;
	struct v4l2_subdev 		**sd;
	struct sp_vout_subdev_info *current_subdev;	/* ptr to currently selected sub device */

	spinlock_t irqlock;						/* Used in video-buf */	
	spinlock_t dma_queue_lock;				/* IRQ lock for DMA queue */	
	struct mutex lock;						/* lock used to access this structure */	

	int 					baddr;
	int 					fs_irq;
	int 					fe_irq;
	u32                     io_usrs;		/* number of users performing IO */
	u8 						started;		/* Indicates whether streaming started */
	u8 						capture_status;
//	u32 usrs;								/* number of open instances of the channel */	
//	u8 initialized;							/* flag to indicate whether decoder is initialized */

};

/* File handle structure */
struct sp_vout_fh {
	struct v4l2_fh fh;
	struct sp_vout_device *vout;	
	u8 io_allowed;							/* Indicates whether this file handle is doing IO */
};

struct sp_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

typedef struct SP_MIPI_t_ {
//	struct i2c_msg *msgs;  /* messages currently handled */
//	struct i2c_adapter adap;
//	I2C_Cmd_t stCmdInfo;
	void __iomem *mipicsi_regs;
	void __iomem *csiiw_regs;
	struct clk *clk_mipicsi_0;
	struct clk *clk_csiiw_0;
//	struct reset_control *rstc;
} SP_MIPI_t;

typedef struct regs_mipicsi_t_ {
	volatile unsigned int mipicsi_ststus;      			/* 00 */
	volatile unsigned int mipi_debug0;      			/* 01 */
	volatile unsigned int mipi_wc_lpf;      			/* 02 */
	volatile unsigned int mipi_analog_cfg0;      		/* 03 */
	volatile unsigned int mipi_analog_cfg1;      		/* 04 */
	volatile unsigned int mipicsi_fsm_rst;      		/* 05 */
	volatile unsigned int mipi_analog_cfg2;  			/* 06 */
	volatile unsigned int mipicsi_enable;     			/* 07 */
	volatile unsigned int mipicsi_mix_cfg;       		/* 08 */
	volatile unsigned int mipicsi_delay_ctl;     		/* 09 */
	volatile unsigned int mipicsi_packet_size;  		/* 10 */
	volatile unsigned int mipicsi_sot_syncword;  		/* 11 */
	volatile unsigned int mipicsi_sof_sol_syncword;  	/* 12 */
	volatile unsigned int mipicsi_eof_eol_syncword;  	/* 13 */
	volatile unsigned int mipicsi_reserved_a14;      	/* 14 */
	volatile unsigned int mipicsi_reserved_a15;      	/* 15 */
	volatile unsigned int mipicsi_ecc_error;  			/* 16 */
	volatile unsigned int mipicsi_crc_error;  			/* 17 */
	volatile unsigned int mipicsi_ecc_cfg;       		/* 18 */
	volatile unsigned int mipi_analog_cfg3;      		/* 19 */
	volatile unsigned int mipi_analog_cfg4;      		/* 20 */
} regs_mipicsi_t;

typedef struct regs_csiiw_t_ {
	volatile unsigned int csiiw_latch_mode;				/* 00 */
	volatile unsigned int csiiw_config0;      			/* 01 */
	volatile unsigned int csiiw_base_addr;				/* 02 */
	volatile unsigned int csiiw_stride;      			/* 03 */
	volatile unsigned int csiiw_frame_size;      		/* 04 */
	volatile unsigned int csiiw_frame_buf;      		/* 05 */
	volatile unsigned int csiiw_config1;  				/* 06 */
	volatile unsigned int csiiw_frame_size_ro;     		/* 07 */
} regs_csiiw_t;



