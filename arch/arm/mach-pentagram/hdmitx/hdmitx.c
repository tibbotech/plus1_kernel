/*----------------------------------------------------------------------------*
 *					INCLUDE DECLARATIONS
 *---------------------------------------------------------------------------*/
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <mach/hdmitx.h>
#include "include/hal_hdmitx.h"

/*----------------------------------------------------------------------------*
 *					MACRO DECLARATIONS
 *---------------------------------------------------------------------------*/
/*about misc*/
// #define HPD_DETECTION
// #define EDID_READ
// #define HDCP_AUTH

/*about print msg*/
#ifdef _HDMITX_ERR_MSG_
#define HDMITX_ERR(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define HDMITX_ERR(fmt, args...)
#endif

#ifdef _HDMITX_WARNING_MSG_
#define HDMITX_WARNING(fmt, args...) printk(KERN_WARNING fmt, ##args)
#else
#define HDMITX_WARNING(fmt, args...)
#endif

#ifdef _HDMITX_INFO_MSG_
#define HDMITX_INFO(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define HDMITX_INFO(fmt, args...)
#endif

#ifdef _HDMITX_DBG_MSG_
#define HDMITX_DBG(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#else
#define HDMITX_DBG(fmt, args...)
#endif

/*----------------------------------------------------------------------------*
 *					DATA TYPES
 *---------------------------------------------------------------------------*/
enum hdmitx_fsm {
	FSM_INIT,
	FSM_HPD,
	FSM_RSEN,
	FSM_HDCP
};

struct hdmitx_video_attribute {
	enum hdmitx_timing timing;
	enum hdmitx_color_depth color_depth;
	enum hdmitx_color_space_conversion conversion;
};

struct hdmitx_audio_attribute {
	enum hdmitx_audio_channel chl;
	enum hdmitx_audio_type type;
	enum hdmitx_audio_sample_size sample_size;
	enum hdmitx_audio_layout layout;
	enum hdmitx_audio_sample_rate sample_rate;
};

struct hdmitx_config {
	enum hdmitx_mode mode;
	struct hdmitx_video_attribute video;
	struct hdmitx_audio_attribute audio;
};

/*----------------------------------------------------------------------------*
 *					GLOBAL VARIABLES
 *---------------------------------------------------------------------------*/
// device driver functions
static int hdmitx_fops_open(struct inode *inode, struct file *pfile);
static int hdmitx_fops_release(struct inode *inode, struct file *pfile);
static long hdmitx_fops_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg);

// platform driver functions
static int hdmitx_probe(struct platform_device *dev);
static int hdmitx_remove(struct platform_device *dev);

// device id
static struct of_device_id g_hdmitx_ids[] = {
	{.compatible = "sunplus,sp-hdmitx"},
};

// device driver operation functions
static const struct file_operations g_hdmitx_fops = {
	.owner          = THIS_MODULE,
	.open           = hdmitx_fops_open,
	.release        = hdmitx_fops_release,
	.unlocked_ioctl = hdmitx_fops_ioctl,
	// .mmap           = hdmitx_mmap,
};

static struct miscdevice g_hdmitx_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "hdmitx",
	.fops  = &g_hdmitx_fops,
};

// platform device
// static struct platform_device g_hdmitx_device = {
// 	.name = "hdmitx",
// 	.id   = -1,
// };

// platform driver
static struct platform_driver g_hdmitx_driver = {
	.probe    = hdmitx_probe,
	.remove   = hdmitx_remove,
	// .suspend  = hdmitx_suspend,
	// .resume   = hdmitx_resume,
	// .shutdown = hdmitx_shotdown,
	.driver   = {
		.name           = "hdmitx",
		.of_match_table = of_match_ptr(g_hdmitx_ids),
		.owner          = THIS_MODULE,
	},
};
module_platform_driver(g_hdmitx_driver);

static struct task_struct *g_hdmitx_task;
static struct mutex g_hdmitx_mutex;
static enum hdmitx_fsm g_hdmitx_state = FSM_INIT;
static unsigned char g_hpd_in = FALSE;
static unsigned char g_rx_ready = FALSE;
static struct hdmitx_config g_cur_hdmi_cfg;
static struct hdmitx_config g_new_hdmi_cfg;
/*----------------------------------------------------------------------------*
 *					EXTERNAL DECLARATIONS
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*
 *					FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
static unsigned char get_hpd_in(void)
{
#ifdef HPD_DETECTION
	return g_hpd_in;
#else
	return TRUE;
#endif
}

static unsigned char get_rx_ready(void)
{
	return g_rx_ready;
}

static void set_hdmi_mode(enum hdmitx_mode mode)
{
	unsigned char is_hdmi;

	is_hdmi = (mode == HDMITX_MODE_HDMI) ? TRUE : FALSE;

	hal_hdmitx_set_hdmi_mode(is_hdmi);
}

static void config_video(struct hdmitx_video_attribute *video)
{
	struct hal_hdmitx_video_attribute attr;

	if (!video) {
		return;
	}

	hal_hdmitx_get_video(&attr);

	attr.timing      = (enum hal_hdmitx_timing) video->timing;
	attr.color_depth = (enum hal_hdmitx_color_depth) video->color_depth;

	switch (video->conversion) {
		case HDMITX_COLOR_SPACE_CONV_LIMITED_RGB_TO_LIMITED_YUV444:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_RGB;
			attr.output_fmt   = PIXEL_FORMAT_YUV444;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_RGB_TO_LIMITED_YUV222:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_RGB;
			attr.output_fmt   = PIXEL_FORMAT_YUV422;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_YUV444_TO_LIMITED_RGB:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_YUV444_TO_FULL_RGB:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_YUV444_TO_LIMITED_YUV444:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_YUV444;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_YUV444_TO_LIMITED_YUV422:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_YUV422;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_YUV422_TO_LIMITED_RGB:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_YUV422;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_YUV422_TO_FULL_RGB:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_YUV422_TO_LIMITED_YUV444:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_YUV422;
			attr.output_fmt   = PIXEL_FORMAT_YUV444;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_YUV422_TO_LIMITED_YUV422:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_YUV422;
			attr.output_fmt   = PIXEL_FORMAT_YUV422;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_RGB_TO_FULL_RGB:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_RGB;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_RGB_TO_LIMITED_YUV444:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_RGB;
			attr.output_fmt   = PIXEL_FORMAT_YUV444;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_RGB_TO_LIMITED_YUV422:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_RGB;
			attr.output_fmt   = PIXEL_FORMAT_YUV422;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_RGB_TO_FULL_YUV444:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_RGB;
			attr.output_fmt   = PIXEL_FORMAT_YUV444;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_RGB_TO_FULL_YUV422:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_RGB;
			attr.output_fmt   = PIXEL_FORMAT_YUV422;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_YUV444_TO_LIMITED_RGB:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_YUV444_TO_FULL_RGB:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_YUV444_TO_FULL_YUV444:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_YUV444;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_YUV444_TO_FULL_YUV422:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_YUV444;
			attr.output_fmt   = PIXEL_FORMAT_YUV422;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_YUV422_TO_LIMITED_RGB:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_YUV422;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_YUV422_TO_FULL_RGB:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_YUV422;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_YUV422_TO_FULL_YUV444:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_YUV422;
			attr.output_fmt   = PIXEL_FORMAT_YUV444;
			break;
		case HDMITX_COLOR_SPACE_CONV_FULL_YUV422_TO_FULL_YUV422:
			attr.input_range  = QUANTIZATION_RANGE_FULL;
			attr.output_range = QUANTIZATION_RANGE_FULL;
			attr.input_fmt    = PIXEL_FORMAT_YUV422;
			attr.output_fmt   = PIXEL_FORMAT_YUV422;
			break;
		case HDMITX_COLOR_SPACE_CONV_LIMITED_RGB_TO_LIMITED_RGB:
		default:
			attr.input_range  = QUANTIZATION_RANGE_LIMITED;
			attr.output_range = QUANTIZATION_RANGE_LIMITED;
			attr.input_fmt    = PIXEL_FORMAT_RGB;
			attr.output_fmt   = PIXEL_FORMAT_RGB;
			break;
	}

	hal_hdmitx_config_video(&attr);
}

static void config_audio(struct hdmitx_audio_attribute *audio)
{
	struct hal_hdmitx_audio_attribute attr;

	if (!audio)	{
		return;
	}

	hal_hdmitx_get_audio(&attr);

	attr.chl         = (enum hal_hdmitx_audio_channel) audio->chl;
	attr.type        = (enum hal_hdmitx_audio_type) audio->type;
	attr.sample_size = (enum hal_hdmitx_audio_sample_size) audio->sample_size;
	attr.layout      = (enum hal_hdmitx_audio_layout) audio->layout;
	attr.sample_rate = (enum hal_hdmitx_audio_sample_rate) audio->sample_rate;

	hal_hdmitx_config_audio(&attr);
}

static void start(void)
{
	hal_hdmitx_start();
}

static void stop(void)
{
	hal_hdmitx_stop();
}

static void process_hdp_state(void)
{
	HDMITX_DBG("HDP State\n");

	if (get_hpd_in()) {
	#ifdef EDID_READ
		// send AV mute
		// read EDID
		// parser EDID
	#else
		// send AV mute
	#endif
		// update state
		mutex_lock(&g_hdmitx_mutex);
		g_hdmitx_state = FSM_RSEN;
		mutex_unlock(&g_hdmitx_mutex);
	}
}

static void process_rsen_state(void)
{
	HDMITX_DBG("RSEN State\n");

	mutex_lock(&g_hdmitx_mutex);
	if (get_hpd_in() && get_rx_ready()) {
		// apply A/V configuration
		stop();
		set_hdmi_mode(g_cur_hdmi_cfg.mode);
		config_video(&g_cur_hdmi_cfg.video);
		config_audio(&g_cur_hdmi_cfg.audio);
		start();
		// update state
		g_hdmitx_state = FSM_HDCP;
	} else {
		// update state
		if (!get_hpd_in()) {

			HDMITX_DBG("hpd out\n");
			g_hdmitx_state = FSM_HPD;
		} else {

			HDMITX_DBG("rsen out\n");
			g_hdmitx_state = FSM_RSEN;
		}
	}
	mutex_unlock(&g_hdmitx_mutex);
}

static void process_hdcp_state(void)
{
	HDMITX_DBG("HDCP State\n");

	if (get_hpd_in() && get_rx_ready()) {
	#ifdef HDCP_AUTH
		// auth.
		// clear AV mute
	#else
		// clear AV mute
	#endif
	} else {
		mutex_lock(&g_hdmitx_mutex);
		if (!get_hpd_in()) {

			HDMITX_DBG("hpd out\n");
			g_hdmitx_state = FSM_HPD;
		} else {

			HDMITX_DBG("rsen out\n");
			g_hdmitx_state = FSM_RSEN;
		}
		mutex_unlock(&g_hdmitx_mutex);
	}
}

static irqreturn_t hdmitx_irq_handler(int irq, void *data)
{
	if (hal_hdmitx_get_interrupt0_status(INTERRUPT0_HDP)) {

		if (hal_hdmitx_get_system_status(SYSTEM_STUS_HPD_IN)) {
			g_hpd_in = TRUE;
		} else {
			g_hpd_in = FALSE;
		}

		hal_hdmitx_clear_interrupt0_status(INTERRUPT0_HDP);
	}

	if (hal_hdmitx_get_interrupt0_status(INTERRUPT0_RSEN)) {

		if (hal_hdmitx_get_system_status(SYSTEM_STUS_RSEN_IN)) {
			g_rx_ready = TRUE;
		} else {
			g_rx_ready = FALSE;
		}

		hal_hdmitx_clear_interrupt0_status(INTERRUPT0_RSEN);
	}

	return IRQ_HANDLED;
}

static int hdmitx_state_handler(void *data)
{
	while (!kthread_should_stop()) {

		switch (g_hdmitx_state) {
			case FSM_HPD:
				process_hdp_state();
				break;
			case FSM_RSEN:
				process_rsen_state();
				break;
			case FSM_HDCP:
				process_hdcp_state();
				break;
			default:
				break;
		}

		msleep(1);
	}

	return 0;
}

void hdmitx_set_timming(enum hdmitx_timing timing)
{
	mutex_lock(&g_hdmitx_mutex);
	g_new_hdmi_cfg.video.timing = timing;
	mutex_unlock(&g_hdmitx_mutex);
}

void hdmitx_get_timming(enum hdmitx_timing *timing)
{
	mutex_lock(&g_hdmitx_mutex);
	*timing = g_cur_hdmi_cfg.video.timing;
	mutex_unlock(&g_hdmitx_mutex);
}

void hdmitx_set_color_depth(enum hdmitx_color_depth color_depth)
{
	mutex_lock(&g_hdmitx_mutex);
	g_new_hdmi_cfg.video.color_depth = color_depth;
	mutex_unlock(&g_hdmitx_mutex);
}

void hdmitx_get_color_depth(enum hdmitx_color_depth *color_depth)
{
	mutex_lock(&g_hdmitx_mutex);
	*color_depth = g_cur_hdmi_cfg.video.color_depth;
	mutex_unlock(&g_hdmitx_mutex);
}

unsigned char hdmitx_get_rx_ready(void)
{
	return get_rx_ready();
}

int hdmitx_enable_display(void)
{
	int err = 0;

	if (get_rx_ready()) {

		mutex_lock(&g_hdmitx_mutex);
		memcpy(&g_cur_hdmi_cfg, &g_new_hdmi_cfg, sizeof(struct hdmitx_config));
		g_hdmitx_state = FSM_RSEN;
		mutex_unlock(&g_hdmitx_mutex);
	} else {
		err = -1;
	}

	return err;
}

void hdmitx_disable_display(void)
{
	stop();
}

void hdmitx_enable_pattern(void)
{
	hal_hdmitx_enable_pattern();
}

void hdmitx_disable_pattern(void)
{
	hal_hdmitx_disable_pattern();
}

EXPORT_SYMBOL(hdmitx_enable_display);
EXPORT_SYMBOL(hdmitx_disable_display);
EXPORT_SYMBOL(hdmitx_set_timming);

static int hdmitx_fops_open(struct inode *inode, struct file *pfile)
{
	int minor, err = 0;

	minor = iminor(inode);

	if (minor != g_hdmitx_misc.minor) {
		HDMITX_ERR("invalid inode\n");
		pfile->private_data = NULL;
		err = -1;
	}

	return err;
}

static int hdmitx_fops_release(struct inode *inode, struct file *pfile)
{
	return 0;
}

static long hdmitx_fops_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	enum hdmitx_timing timing;
	enum hdmitx_color_depth color_depth;
	unsigned char rx_ready, enable;

	switch (cmd) {

		case HDMITXIO_SET_TIMING:
			if (copy_from_user((void*) &timing, (const void __user *) arg, sizeof(enum hdmitx_timing))) {
				err = -1;
			} else {
				hdmitx_set_timming(timing);
			}
			break;
		case HDMITXIO_GET_TIMING:
			hdmitx_get_timming(&timing);
			if (copy_to_user((void __user *) arg, (const void *) &timing, sizeof(enum hdmitx_timing))) {
				err = -1;
			}
			break;
		case HDMITXIO_SET_COLOR_DEPTH:
			if (copy_from_user((void*) &color_depth, (const void __user *) arg, sizeof(enum hdmitx_color_depth))) {
				err = -1;
			} else {
				hdmitx_set_color_depth(color_depth);
			}
			break;
		case HDMITXIO_GET_COLOR_DEPTH:
			hdmitx_get_color_depth(&color_depth);
			if (copy_to_user((void __user *) arg, (const void *) &color_depth, sizeof(enum hdmitx_color_depth))) {
				err = -1;
			}
			break;
		case HDMITXIO_GET_RX_READY:
			rx_ready = hdmitx_get_rx_ready();
			if (copy_to_user((void __user *) arg, (const void *) &rx_ready, sizeof(unsigned char))) {
				err = -1;
			}
			break;
		case HDMITXIO_DISPLAY:
			if (copy_from_user((void*) &enable, (const void __user *) arg, sizeof(unsigned char))) {
				err = -1;
			} else {
				if (enable) {
					hdmitx_enable_display();
				} else {
					hdmitx_disable_display();
				}
			}
			break;
		case HDMITXIO_PTG:
			if (copy_from_user((void*) &enable, (const void __user *) arg, sizeof(unsigned char))) {
				err = -1;
			} else {
				if (enable) {
					hdmitx_enable_pattern();
				} else {
					hdmitx_disable_pattern();
				}
			}
			break;
		default:
			HDMITX_ERR("Invalid hdmitx ioctl commnad\n");
			err = -1;
			break;
	}

	return -1;
}

static int hdmitx_probe(struct platform_device *pdev)
{
	int err, irq;
	printk("================== hdmitx_probe1\n");
	/*initialize hardware settings*/
	hal_hdmitx_init();

	/*initialize software settings*/
	// reset hdmi config
	g_cur_hdmi_cfg.mode              = HDMITX_MODE_HDMI;
	g_cur_hdmi_cfg.video.timing      = HDMITX_TIMING_480P;
	g_cur_hdmi_cfg.video.color_depth = HDMITX_COLOR_DEPTH_24BITS;
	g_cur_hdmi_cfg.video.conversion  = HDMITX_COLOR_SPACE_CONV_LIMITED_YUV444_TO_LIMITED_RGB;
	g_cur_hdmi_cfg.audio.chl         = HDMITX_AUDIO_CHL_I2S;
	g_cur_hdmi_cfg.audio.type        = HDMITX_AUDIO_TYPE_LPCM;
	g_cur_hdmi_cfg.audio.sample_size = HDMITX_AUDIO_SAMPLE_SIZE_16BITS;
	g_cur_hdmi_cfg.audio.layout      = HDMITX_AUDIO_LAYOUT_2CH;
	g_cur_hdmi_cfg.audio.sample_rate = HDMITX_AUDIO_SAMPLE_RATE_48000HZ;
	memcpy(&g_new_hdmi_cfg, &g_cur_hdmi_cfg, sizeof(struct hdmitx_config));

	// request irq
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		HDMITX_ERR("platform_get_irq failed\n");
		return -ENODEV;
	}

	err = devm_request_irq(&pdev->dev, irq, hdmitx_irq_handler, IRQF_TRIGGER_HIGH, "hdmitx irq", pdev);
	if (err) {
		HDMITX_ERR("devm_request_irq failed: %d\n", err);
		return err;
	}

	// create thread
	g_hdmitx_task = kthread_run(hdmitx_state_handler, NULL, "hdmitx_task");
	if (IS_ERR(g_hdmitx_task)) {
		err = PTR_ERR(g_hdmitx_task);
		HDMITX_ERR("kthread_run failed: %d\n", err);
		return err;
	}

	// registry device
	err = misc_register(&g_hdmitx_misc);
	if (err) {
		HDMITX_ERR("misc_register failed: %d\n", err);
		return err;
	}

	// init mutex
	mutex_init(&g_hdmitx_mutex);

	//
	wake_up_process(g_hdmitx_task);
	g_hdmitx_state = FSM_HPD;

	return 0;
}

static int hdmitx_remove(struct platform_device *pdev)
{
	int err = 0;

	/*deinitialize hardware settings*/
	hal_hdmitx_deinit();

	/*deinitialize software settings*/
	if (g_hdmitx_task) {
		err = kthread_stop(g_hdmitx_task);
		if (err) {
			HDMITX_ERR("kthread_stop failed: %d\n", err);
		} else {
			g_hdmitx_task = NULL;
		}
	}

	g_hdmitx_state = FSM_INIT;

	return err;
}

static int __init hdmitx_init(void)
{
	return 0;
}

static void __exit hdmitx_exit(void)
{

}

module_init(hdmitx_init);
module_exit(hdmitx_exit);
MODULE_DESCRIPTION("HDMITX driver");
MODULE_LICENSE("GPL");
