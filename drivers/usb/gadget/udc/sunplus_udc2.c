#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of_irq.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/usb/composite.h>
#include <linux/mod_devicetable.h>
#include <asm/cacheflush.h>
#include <asm/unaligned.h>
#include <linux/usb/otg.h>

#include "sunplus_udc2.h"
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
#include "../../phy/otg-sunplus.h"


extern void sp_accept_b_hnp_en_feature(struct usb_otg *);

extern u32 otg_id_pin;
static char *otg_status_buf;
static char *otg_status_buf_ptr_addr;

	#ifdef OTG_TEST
static u32 dev_otg_status = 1;
	#else
static u32 dev_otg_status;
	#endif
module_param(dev_otg_status, uint, 0644);
#endif

#define DRIVER_NAME "sp-udc"
static const char ep0name[] = "ep0";

#define EP_INFO(_name, _caps) \
	{ \
		.name = _name, \
		.caps = _caps, \
	}

static const struct {
	const char *name;
	const struct usb_ep_caps caps;
} ep_info_dft[] = {	/* Default endpoint configuration */
	EP_INFO(ep0name,
		USB_EP_CAPS(USB_EP_CAPS_TYPE_CONTROL, USB_EP_CAPS_DIR_ALL)),
	EP_INFO("ep1in",
		USB_EP_CAPS(USB_EP_CAPS_TYPE_ALL, USB_EP_CAPS_DIR_IN)),
	EP_INFO("ep2in",
		USB_EP_CAPS(USB_EP_CAPS_TYPE_ALL, USB_EP_CAPS_DIR_IN)),
	EP_INFO("ep3in",
		USB_EP_CAPS(USB_EP_CAPS_TYPE_ALL, USB_EP_CAPS_DIR_IN)),
	EP_INFO("ep4in",
		USB_EP_CAPS(USB_EP_CAPS_TYPE_ALL, USB_EP_CAPS_DIR_IN)),
	EP_INFO("ep5out",
		USB_EP_CAPS(USB_EP_CAPS_TYPE_ALL, USB_EP_CAPS_DIR_OUT)),
	EP_INFO("ep6out",
		USB_EP_CAPS(USB_EP_CAPS_TYPE_ALL, USB_EP_CAPS_DIR_OUT)),
	EP_INFO("ep7out",
		USB_EP_CAPS(USB_EP_CAPS_TYPE_ALL, USB_EP_CAPS_DIR_OUT)),
	EP_INFO("ep8out",
		USB_EP_CAPS(USB_EP_CAPS_TYPE_ALL, USB_EP_CAPS_DIR_OUT)),
};
#undef EP_INFO

static struct sp_udc *sp_udc_arry[2] = {NULL, NULL};

/* test mode, When test mode is enabled for USB IF Test, sof is not detected */
static uint test_mode;
module_param(test_mode, uint, 0644);

static uint dmsg = 0x3;
module_param(dmsg, uint, 0644);

#define UDC_LOGE(fmt, arg...)		pr_err(fmt, ##arg)
#define UDC_LOGW(fmt, arg...)		do { if (dmsg & BIT(0)) pr_warn(fmt, ##arg); } while (0)
#define UDC_LOGL(fmt, arg...)		do { if (dmsg & BIT(1)) pr_info(fmt, ##arg); } while (0)
#define UDC_LOGI(fmt, arg...)		do { if (dmsg & BIT(2)) pr_info(fmt, ##arg); } while (0)
#define UDC_LOGD(fmt, arg...)		do { if (dmsg & BIT(3)) pr_info(fmt, ##arg); } while (0)

/* Produces a mask of set bits covering a range of a 32-bit value */
static inline uint32_t bitfield_mask(uint32_t shift, uint32_t width)
{
	return ((1 << width) - 1) << shift;
}

/* Extract the value of a bitfield found within a given register value */
static inline uint32_t bitfield_extract(uint32_t reg_val, uint32_t shift, uint32_t width)
{
	return (reg_val & bitfield_mask(shift, width)) >> shift;
}

/* Replace the value of a bitfield found within a given register value */
static inline uint32_t bitfield_replace(uint32_t reg_val, uint32_t shift, uint32_t width, uint32_t val)
{
	uint32_t mask = bitfield_mask(shift, width);

	return (reg_val & ~mask) | (val << shift);
}

/* internal `bitfield_replace' version, for register value with mask bits */
#define __bitfield_replace(value, shift, width, new_value)	\
	(bitfield_replace(value, shift, width, new_value) | bitfield_mask(shift + 16, width))

static int sp_udc_get_frame(struct usb_gadget *gadget);
static int sp_udc_pullup(struct usb_gadget *gadget, int is_on);
static int sp_udc_vbus_session(struct usb_gadget *gadget, int is_active);
static int sp_udc_start(struct usb_gadget *gadget, struct usb_gadget_driver *driver);
static int sp_udc_stop(struct usb_gadget *gadget);
static struct usb_ep *sp_udc_match_ep(struct usb_gadget *_gadget,
					struct usb_endpoint_descriptor *desc,
						struct usb_ss_ep_comp_descriptor *ep_comp);
static int sp_udc_ep_enable(struct usb_ep *_ep, const struct usb_endpoint_descriptor *desc);
static int sp_udc_ep_disable(struct usb_ep *_ep);
static struct usb_request *sp_udc_alloc_request(struct usb_ep *_ep, gfp_t mem_flags);
static void sp_udc_free_request(struct usb_ep *_ep, struct usb_request *_req);
static int sp_udc_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags);
static int sp_udc_dequeue(struct usb_ep *_ep, struct usb_request *_req);
static int sp_udc_set_halt(struct usb_ep *_ep, int value);
static int sp_udc_probe(struct platform_device *pdev);
static int sp_udc_remove(struct platform_device *pdev);
static int hal_udc_setup(struct sp_udc *udc, const struct usb_ctrlrequest *ctrl);
#ifdef CONFIG_PM
static int udc_sunplus_drv_suspend(struct device *dev);
static int udc_sunplus_drv_resume(struct device *dev);
#endif

static const struct usb_gadget_ops sp_ops = {
	.get_frame			= sp_udc_get_frame,
	.wakeup				= NULL,
	.set_selfpowered		= NULL,
	.pullup				= sp_udc_pullup,
	.vbus_session			= sp_udc_vbus_session,
	.vbus_draw			= NULL,
	.udc_start			= sp_udc_start,
	.udc_stop			= sp_udc_stop,
	.match_ep			= sp_udc_match_ep,
};

static const struct usb_ep_ops sp_ep_ops = {
	.enable		= sp_udc_ep_enable,
	.disable	= sp_udc_ep_disable,

	.alloc_request	= sp_udc_alloc_request,
	.free_request	= sp_udc_free_request,

	.queue		= sp_udc_queue,
	.dequeue	= sp_udc_dequeue,

	.set_halt	= sp_udc_set_halt,
};

static const struct of_device_id sunplus_udc_ids[] = {
#if defined (CONFIG_SOC_Q645)
	{ .compatible = "sunplus,q645-usb-udc" },
#elif defined (CONFIG_SOC_SP7350)
	{ .compatible = "sunplus,sp7350-usb-udc" },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, sunplus_udc_ids);

#ifdef CONFIG_PM
struct dev_pm_ops const udc_sunplus_pm_ops = {
	.suspend = udc_sunplus_drv_suspend,
	.resume = udc_sunplus_drv_resume,
};
#endif

static struct platform_driver sp_udc_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table = sunplus_udc_ids,
#ifdef CONFIG_PM
		.pm = &udc_sunplus_pm_ops,
#endif
	},
	.probe		= sp_udc_probe,
	.remove		= sp_udc_remove,
};

void udc_print_block(u8 *pBuffStar, u32 uiBuffLen)
{
	print_hex_dump(KERN_NOTICE, "usb", DUMP_PREFIX_ADDRESS, 16, 4, pBuffStar,
									uiBuffLen, false);
}

static void pwr_uphy_pll(int enable)
{
	u32 temp;

	temp = readl(uphy0_regs + GLO_CTRL2_OFFSET);

	if (enable) {
		temp &= (~PLL_PD_SEL & ~PLL_PD);
		writel(temp, uphy0_regs + GLO_CTRL2_OFFSET);

		writel(readl(uphy0_regs + GLO_CTRL1_OFFSET) & ~CLK120_27_SEL,
							uphy0_regs + GLO_CTRL1_OFFSET);

		UDC_LOGD("uphy pll power-up\n");
	} else {
		temp |= (PLL_PD_SEL | PLL_PD);
		writel(temp, uphy0_regs + GLO_CTRL2_OFFSET);

		writel(readl(uphy0_regs + GLO_CTRL1_OFFSET) | CLK120_27_SEL,
							uphy0_regs + GLO_CTRL1_OFFSET);

		UDC_LOGD("uphy pll power-down\n");
	}
}

static void *udc_malloc_align(struct sp_udc *udc, dma_addr_t *pa,
				size_t size, size_t align, gfp_t flag)
{
	void *alloc_addr = NULL;
	struct device *dev = udc->dev;

	alloc_addr = dma_alloc_coherent(dev, size, pa, flag);
	if (!alloc_addr) {
		pr_warn("+%s.%d, fail\n", __func__, __LINE__);
		return NULL;
	}

	return alloc_addr;
}

static void udc_free_align(struct sp_udc *udc, void *vaddr, dma_addr_t pa, size_t size)
{
	void *alloc_addr = vaddr;
	struct device *dev = udc->dev;

	dma_free_coherent(dev, size, alloc_addr, pa);
}

static int udc_ring_malloc(struct sp_udc *udc, struct udc_ring *const ring, uint8_t num_mem)
{
	if (ring->trb_va) {
		pr_warn("ring already exists\n");
		return -1;
	}

	if (!num_mem) {
		pr_warn("ring members are %d\n", num_mem);
		return -2;
	}

	ring->num_mem = num_mem;
	ring->trb_va = (struct trb_data *)udc_malloc_align(udc,
		&ring->trb_pa, ring->num_mem * ENTRY_SIZE, ALIGN_64_BYTE, GFP_DMA);

	if (!ring->trb_va) {
		pr_warn("malloc %d memebrs ring fail\n", num_mem);
		return -3;
	}

	ring->end_trb_va = ring->trb_va + (ring->num_mem - 1);
	ring->end_trb_pa = ring->trb_pa + ((ring->num_mem - 1) * ENTRY_SIZE);
	UDC_LOGD("ring %px[%d], s:%px,%llx e:%px,%llx\n", ring, ring->num_mem,
			ring->trb_va, ring->trb_pa, ring->end_trb_va, ring->end_trb_pa);

	return 0;
}

static void udc_ring_free(struct sp_udc *udc, struct udc_ring *const ring)
{
	udc_free_align(udc, ring->trb_va, ring->trb_pa, ring->num_mem * ENTRY_SIZE);

	ring->trb_va = NULL;
	ring->end_trb_va = NULL;
	ring->num_mem = 0;
}

static inline struct udc_endpoint *to_sp_ep(struct usb_ep *ep)
{
	return container_of(ep, struct udc_endpoint, ep);
}

static inline struct sp_udc *to_sp_udc(struct usb_gadget *gadget)
{
	return container_of(gadget, struct sp_udc, gadget);
}

static inline struct sp_request *to_sp_req(struct usb_request *req)
{
	return container_of(req, struct sp_request, req);
}

#if 0
static ssize_t show_udc_ctrl(struct device *dev, struct device_attribute *attr, char *buffer)
{
	return 0;
}

static ssize_t store_udc_ctrl(struct device *dev, struct device_attribute *attr,
							const char *buffer, size_t count)
{
	UDC_LOGD("+%s.%d\n", __func__, __LINE__);
	return count;
}

static DEVICE_ATTR(udc_ctrl, S_IWUSR | S_IRUSR, show_udc_ctrl, store_udc_ctrl);

#ifndef CONFIG_SP_USB_PHY
static u8 sp_phy_disc_off[3] = {6, 6, 5};
static u8 sp_phy_disc_shift[3] = {0, 8, 24};
#define ORIG_UPHY_DISC      0x7
#define DEFAULT_UPHY_DISC   0x8

static void sp_usb_config_phy_otp(u32 usb_no)
{
	struct uphy_rn_regs *phy = UPHY_RN_REG(usb_no);
	u32 val;

	/* Select Clock Edge Control Signal */
	if (readl(&OTP_REG->hb_otp_data[2]) & BIT(usb_no)) {
		pr_info("uphy%d rx clk inv\n", usb_no);
		val = readl(&phy->gctrl[1]);
		val |= BIT(6);
		writel(val, &phy->gctrl[1]);
	}

	/* control disconnect voltage */
	val = readl(&OTP_REG->hb_otp_data[sp_phy_disc_off[usb_no]]);
	val = bitfield_extract(val, sp_phy_disc_shift[usb_no], 5);
	if (!val)
		val = DEFAULT_UPHY_DISC;
	else if (val <= ORIG_UPHY_DISC)
		val++;
	writel(bitfield_replace(readl(&phy->cfg[7]), 0, 5, val), &phy->cfg[7]);
}

static void usb_controller_phy_init(u32 usb_no)
{
	struct uphy_rn_regs *phy = UPHY_RN_REG(usb_no);
	u32 val;

	/* 1. reset UPHY */
	writel(RF_MASK_V_SET(BIT(usb_no + 1)), &MOON2_REG->reset[4]);
	mdelay(1);
	writel(RF_MASK_V_CLR(BIT(usb_no + 1)), &MOON2_REG->reset[4]);
	mdelay(1);

	/* 2. Default value modification */
	writel(0x08888100 | BIT(usb_no + 1), &phy->gctrl[0]);

	/* 3. PLL power off/on twice */
	writel(0x88, &phy->gctrl[2]);
	mdelay(1);
	writel(0x80, &phy->gctrl[2]);
	mdelay(1);
	writel(0x88, &phy->gctrl[2]);
	mdelay(1);
	writel(0x80, &phy->gctrl[2]);
	mdelay(1);
	writel(0x00, &phy->gctrl[2]);
	mdelay(20); // experience

	/* 4. UPHY 0&1 internal register modification */
	// Register default value: 0x87
	//writel(0x87, &phy->cfg[7]);

	/* 5. USB controller reset */
	writel(RF_MASK_V_SET(BIT(usb_no + 4)), &MOON2_REG->reset[4]);
	mdelay(1);
	writel(RF_MASK_V_CLR(BIT(usb_no + 4)), &MOON2_REG->reset[4]);
	mdelay(1);

	/* 6. fix rx-active question */
	val = readl(&phy->cfg[19]);
	val |= 0x0f;
	writel(val, &phy->cfg[19]);

	sp_usb_config_phy_otp(usb_no);
}


static void udc_usb_switch(struct sp_udc *udc, bool device)
{
	struct uphy_rn_regs *phy = UPHY_RN_REG(udc->port_num);

	if (device) {
		/* 2. Default value modification */
		writel(phy->gctrl[0] | BIT(8), &phy->gctrl[0]);

		if (udc->port_num == 0)
			MOON3_REG->sft_cfg[21] = RF_MASK_V(0x7 << 9, 0x01 << 9);
		else if (udc->port_num == 1)
			MOON3_REG->sft_cfg[22] = RF_MASK_V(0x7 << 0, 0x1 << 0);

		/* connect */
		udc->vbus_active = true;
		usb_udc_vbus_handler(&udc->gadget, udc->vbus_active);
		UDC_LOGL("host to device\n");
	} else {
		udc->vbus_active = false;
		usb_udc_vbus_handler(&udc->gadget, udc->vbus_active);
		if (udc->port_num == 0)
			MOON3_REG->sft_cfg[21] = RF_MASK_V(0x7 << 9, 0x07 << 9);
		else if (udc->port_num == 1)
			MOON3_REG->sft_cfg[22] = RF_MASK_V(0x7 << 0, 0x7 << 0);
		UDC_LOGL("device to host\n");
	}
}

int32_t usb_switch(uint8_t port, bool device)
{
	struct sp_udc *udc;

	if (port >= 2) {
		UDC_LOGE("usb switch has no port %d\n", port);
		return -EINVAL;
	}

	udc = sp_udc_arry[port];
	if (udc) {
		UDC_LOGL("USB port udc[%d]->%d switch\n", udc->port_num, port);
		udc_usb_switch(udc, device);
		return 0;
	}
	UDC_LOGW("USB port %d,UDC not ready\n", port);
	return -EINVAL;
}
EXPORT_SYMBOL(usb_switch);
#endif
#endif

static ssize_t show_udc_debug(struct device *dev, struct device_attribute *attr, char *buffer)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct sp_udc *udc = platform_get_drvdata(pdev);

	dev_info(dev, "%d\n", udc->port_num);
	dev_info(dev, "run speed %s,Max speed %s\n", udc->gadget.speed == USB_SPEED_FULL ? "Full" : "High",
						     udc->def_run_full_speed ? "Full" : "High");
	return 0;
}

static void check_event_ring(struct sp_udc *udc);
static void hal_dump_event_ring(struct sp_udc *udc);
static void hal_dump_ep_ring(struct sp_udc *udc, uint8_t ep_num);

/* Count the number of interruptions */
static u32 udc_irq_in_count;
static u32 udc_irq_out_count;

static ssize_t store_udc_debug(struct device *dev, struct device_attribute *attr,
							const char *buffer, size_t count)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct sp_udc *udc = platform_get_drvdata(pdev);
	unsigned long num;
	char *tmp;
	char *name, *arg, **s;

#if 0
#ifndef CONFIG_SP_USB_PHY
		if (*buffer == 'd')
			usb_switch(udc->port_num, 1);
		if (*buffer == 'h')
			usb_switch(udc->port_num, 0);

		if (*buffer == 'r') {
			UDC_LOGD("reset\n");
	//		usb_controller_phy_init(udc->port_num);
		}
#endif
#endif

	tmp = kmalloc(count, GFP_KERNEL);
	if (!tmp)
		return 0;

	memcpy(tmp, buffer, count);
	s = &tmp;
	name = strsep(s, " \n");
	arg = strsep(s, " \n");

	if (name && arg) {
		if (!strcasecmp("ep", name)) {
			if (!kstrtoul(arg, 10, &num))
				hal_dump_ep_ring(udc, num);
			kfree(tmp);
			return count;
		}

		/* set run max speed ,num > 0 max speed High*/
		if (!strcasecmp("MXS", name)) {
			if (!kstrtoul(arg, 10, &num)) {
				if (num)
					udc->def_run_full_speed = false;
				else
					udc->def_run_full_speed = true;
			}
		}
	}

	if (*buffer == 'c') {
		UDC_LOGD("Debug\n");
		check_event_ring(udc);
	}

	if (*buffer == 'D')
		hal_dump_event_ring(udc);

	if (*buffer == 'T')
		tasklet_schedule(&udc->event_task);

	if (*buffer == 'I')
		pr_emerg("UDC%d:irq[%d:%d]\n", udc->port_num, udc_irq_in_count, udc_irq_out_count);

	kfree(tmp);

	return count;
}

static DEVICE_ATTR(debug, S_IWUSR | S_IRUSR, show_udc_debug, store_udc_debug);

void init_ep_spin(struct sp_udc *udc)
{
	int i;

	for (i = 0; i < UDC_MAX_ENDPOINT_NUM; i++)
		spin_lock_init(&udc->ep_data[i].lock);
}

static void sp_udc_done(struct udc_endpoint *ep, struct sp_request *req, int status)
{
	if (likely (req->req.status == -EINPROGRESS))
		req->req.status = status;

	req->req.complete(&ep->ep, &req->req);
}

static void sp_udc_nuke(struct sp_udc *udc, struct udc_endpoint *ep, int status)
{
	unsigned long flags;
	struct sp_request *req;

	UDC_LOGD("+%s.%d\n", __func__, __LINE__);

	if (ep == NULL)
		return;

	spin_lock_irqsave(&ep->lock, flags);
	while (!list_empty (&ep->queue)) {
		req = list_entry (ep->queue.next, struct sp_request, queue);

#if (TRANS_MODE == DMA_MAP)
		usb_gadget_unmap_request(&udc->gadget, &req->req, EP_DIR(ep->bEndpointAddress));
#endif

		list_del(&req->queue);
		spin_unlock(&ep->lock);
		sp_udc_done(ep, req, status);
		spin_lock(&ep->lock);
	}
	spin_unlock_irqrestore(&ep->lock, flags);
}

static void udc_sof_polling(struct timer_list *t)
{
	uint32_t new_frame_num = 0;
	struct sp_udc *udc = from_timer(udc, t, sof_polling_timer);
	volatile struct udc_reg *USBx = udc->reg;

	new_frame_num = USBx->USBD_FRNUM_ADDR & FRNUM;
	if (udc->frame_num == new_frame_num) {
		UDC_LOGI("sof timer,device disconnect\n");
		udc->bus_reset_finish = false;
		usb_phy_notify_disconnect(udc->usb_phy, udc->gadget.speed);
		udc->frame_num = 0;
		return;
	}
	udc->frame_num = new_frame_num;
	mod_timer(&udc->sof_polling_timer, jiffies + HZ / 20);
}

static void udc_before_sof_polling(struct timer_list *t)
{
	uint32_t new_frame_num = 0;
	struct sp_udc *udc = from_timer(udc, t, before_sof_polling_timer);
	volatile struct udc_reg *USBx = udc->reg;

	if (udc->bus_reset_finish) {
		UDC_LOGI("before sof timer,bus reset finish\n");
		mod_timer(&udc->sof_polling_timer, jiffies + HZ / 20);
		usb_phy_notify_connect(udc->usb_phy, udc->gadget.speed);
		return;
	}

	new_frame_num = USBx->USBD_FRNUM_ADDR & FRNUM;
	if (udc->frame_num == new_frame_num) {
		UDC_LOGE("bus reset err\n");
		udc->bus_reset_finish = false;
		usb_phy_notify_disconnect(udc->usb_phy, udc->gadget.speed);
		udc->frame_num = 0;
		return;
	}

	udc->frame_num = new_frame_num;
	mod_timer(&udc->before_sof_polling_timer, jiffies + 1 * HZ);
}

static uint32_t check_trb_status(struct trb_data *t_trb)
{
	uint32_t ret = 0;
	uint32_t trb_status;

	trb_status = ETRB_CC(t_trb->entry2);
	switch (trb_status) {
	case INVALID_TRB:
		UDC_LOGE("invaild trb\n");
		break;
	case SUCCESS_TRB:
		ret = SUCCESS_TRB;
		break;
	case DATA_BUF_ERR:
		UDC_LOGE("data buf err\n");
		break;
	case BABBLE_ERROR:
		UDC_LOGE("babble error\n");
		ret = BABBLE_ERROR;
		break;
	case TRANS_ERR:
		UDC_LOGE("trans err,%p,%x,%x,%x,%x\n", t_trb, t_trb->entry0, t_trb->entry1,
								t_trb->entry2, t_trb->entry3);
		ret = TRANS_ERR;
		break;
	case TRB_ERR:
		UDC_LOGE("trb err\n");
		break;
	case RESOURCE_ERR:
		UDC_LOGE("resource err\n");
		break;
	case SHORT_PACKET:
		UDC_LOGD("short packet\n");
		ret = SHORT_PACKET;
		break;
	case EVENT_RING_FULL:
		UDC_LOGD("event ring full\n");
		ret = EVENT_RING_FULL;
		break;

	/* UDC evnet */
	case UDC_STOPED:
		ret = UDC_STOPED;
		break;
	case UDC_RESET:
		ret = UDC_RESET;
		break;
	case UDC_SUSPEND:
		ret = UDC_SUSPEND;
		break;
	case UDC_RESUME:
		ret = UDC_RESUME;
		break;
	default:
		UDC_LOGE("+%s.%d,trb_status:%d\n", __func__, __LINE__, trb_status);
		ret = -EPERM;
		break;
	}

	return ret;
}

static inline void hal_udc_sw_stop_handle(struct sp_udc *udc)
{
	volatile struct udc_reg *USBx = udc->reg;

	sp_udc_nuke(udc, &udc->ep_data[0], -ESHUTDOWN);
	hal_udc_endpoint_unconfigure(udc, 0x00);
	USBx->GL_CS |= 1 << 31;
}

static uint32_t hal_udc_check_trb_type(struct trb_data *t_trb)
{
	uint32_t index;
	uint32_t trb_type = -1;

	index = LTRB_TYPE((uint32_t)t_trb->entry3);
	switch (index) {
	case NORMAL_TRB:
		trb_type = NORMAL_TRB;
		UDC_LOGD("NORMAL_TRB WHY?\n");
		break;
	case SETUP_TRB:
		trb_type = SETUP_TRB;
		UDC_LOGD("SETUP_TRB\n");
		break;
	case LINK_TRB:
		trb_type = LINK_TRB;
		UDC_LOGD("LINK_TRB WHY?\n");
		break;
	case TRANS_EVENT_TRB:
		trb_type = TRANS_EVENT_TRB;
		UDC_LOGD("TRANS_EVENT_TRB\n");
		break;
	case DEV_EVENT_TRB:
		trb_type = DEV_EVENT_TRB;
		break;
	case SOF_EVENT_TRB:
		trb_type = SOF_EVENT_TRB;
		break;
	default:
		UDC_LOGD("+%s.%d,index:%d\n", __func__, __LINE__, index);
		break;
	}

	return trb_type;
}

static void hal_udc_transfer_event_handle(struct transfer_event_trb *transfer_evnet, struct sp_udc *udc)
{
	struct normal_trb *ep_trb = NULL;
	uint32_t len_zh, len_yu;
	struct sp_request *req;
	uint32_t trans_len = 0;
#if (TRANS_MODE == DMA_MODE)
	uint8_t *data_buf;
#elif (TRANS_MODE == DMA_MAP)
	uint32_t *data_buf;
#endif
	struct udc_endpoint *ep = NULL;
	uint8_t ep_num;
	int8_t count = TRANSFER_RING_COUNT;
	unsigned long flags;

	ep_num = transfer_evnet->eid;
	ep = &udc->ep_data[ep_num];

	spin_lock_irqsave(&ep->lock, flags);
	if (!ep->ep_trb_ring_dq) {
		spin_unlock_irqrestore(&ep->lock, flags);
		UDC_LOGW(" ep%d not configure\n", ep->num);

		return;
	}

	ep_trb = (struct normal_trb *)(ep->ep_transfer_ring.trb_va +
		((transfer_evnet->trbp - ep->ep_transfer_ring.trb_pa) / sizeof(struct trb_data)));

#if (TRANS_MODE == DMA_MODE)
	data_buf = (uint8_t *)phys_to_virt(ep_trb->ptr);
#elif (TRANS_MODE == DMA_MAP)
	data_buf = (uint32_t *)phys_to_virt(ep_trb->ptr);
#endif

	trans_len = ep_trb->tlen;

	UDC_LOGD("ep %x[%c],trb:%px,%x len %d - %d\n", ep_num, ep_trb->dir ? 'I' : 'O',
			ep_trb, transfer_evnet->trbp, trans_len, transfer_evnet->len);

	if (!ep->num && !trans_len && list_empty (&ep->queue)) {
		spin_unlock_irqrestore(&ep->lock, flags);
		UDC_LOGD("+%s.%d ep0 zero\n", __func__, __LINE__);

		return;
	}

	if (!ep->is_in && trans_len) {
		len_zh = trans_len / CACHE_LINE_SIZE;
		len_yu = trans_len % CACHE_LINE_SIZE;
		if (len_yu)
			len_zh = (len_zh + 1) * CACHE_LINE_SIZE;
		else
			len_zh = len_zh * CACHE_LINE_SIZE;

		dma_sync_single_for_cpu(udc->dev, ep_trb->ptr, len_zh, DMA_FROM_DEVICE);
	}

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	#if (TRANS_MODE == DMA_MODE)
	if (data_buf == (uint8_t *)otg_status_buf)
	#elif (TRANS_MODE == DMA_MAP)
	if (data_buf == (uint32_t *)otg_status_buf)
	#endif
	{
		struct usb_request *_req = container_of((void *)otg_status_buf_ptr_addr,
									struct usb_request, buf);
		struct sp_request *req = container_of(_req, struct sp_request, req);

		if (dev_otg_status == 1)
			dev_otg_status = 0;

		otg_status_buf = NULL;
		otg_status_buf_ptr_addr = NULL;

	#if (TRANS_MODE == DMA_MAP)
		usb_gadget_unmap_request(&udc->gadget, &req->req, EP_DIR(ep->bEndpointAddress));
	#endif

		kfree(req->req.buf);
		kfree(req);
		spin_unlock_irqrestore(&ep->lock, flags);

		return;
	}
#endif

	while (!list_empty (&ep->queue)) {
		req = list_entry (ep->queue.next, struct sp_request, queue);

		UDC_LOGD("find ep%x,req:%px,req_trb:%px->%px\n", ep_num, req,
						req->transfer_trb, ep_trb);

		if (req->transfer_trb == (struct trb_data *)ep_trb) {
			req->req.actual = trans_len - transfer_evnet->len;
			req->transfer_trb = NULL;

#if (TRANS_MODE == DMA_MAP)
			usb_gadget_unmap_request(&udc->gadget, &req->req, EP_DIR(ep->bEndpointAddress));
#endif

			list_del(&req->queue);

			spin_unlock_irqrestore(&ep->lock, flags);
			sp_udc_done(ep, req, 0);

			return ;
		}

		if (list_is_last(&req->queue, &ep->queue))
			count = -2;

		count--;
		if (count < 0)
			break;
	}

	if (count) {
		spin_unlock_irqrestore(&ep->lock, flags);
		UDC_LOGD("ep%x ep_queue not req %d\n", ep_num, count);
	}
}

static void hal_udc_analysis_event_trb(struct trb_data *event_trb, struct sp_udc *udc)
{
	volatile struct udc_reg *USBx = udc->reg;
	struct usb_ctrlrequest crq;
	uint32_t trb_type;
	uint32_t ret;

	/* handle trb err situation */
	trb_type = hal_udc_check_trb_type(event_trb);

	ret = check_trb_status(event_trb);
	if (!ret) {
		UDC_LOGE("error,check_trb_status fail,ret : %d\n", ret);
		return;
	}

	switch (trb_type) {
	case SETUP_TRB:
		if (!udc->bus_reset_finish) {
			switch (USBx->USBD_DEBUG_PP & 0xFF) {
			case PP_FULL_SPEED:
				udc->gadget.speed = USB_SPEED_FULL;
				UDC_LOGL("Full speed\n");
				break;
			case PP_HIGH_SPEED:
				udc->gadget.speed = USB_SPEED_HIGH;
				UDC_LOGL("High speed\n");
				break;
			default:
				UDC_LOGE("error, Unknown speed\n");
				udc->gadget.speed = udc->def_run_full_speed ? USB_SPEED_FULL : USB_SPEED_HIGH;
				break;
			}
		}

		/* enable device auto sof */
		if (!(readl(&USBx->GL_CS) & BIT(9)))
			writel(bitfield_replace(readl(&USBx->GL_CS), 9, 1, 1), &USBx->GL_CS);

		udc->bus_reset_finish = true;

		memcpy(&crq, event_trb, 8);
		UDC_LOGD("bRequestType=0x%02x,bRequest=0x%02x,wValue=0x%04x,wIndex=0x%04x,wLength=0x%04x\n",
						crq.bRequestType, crq.bRequest, crq.wValue, crq.wIndex, crq.wLength);

		hal_udc_setup(udc, &crq);
		break;
	case TRANS_EVENT_TRB:
		hal_udc_transfer_event_handle((struct transfer_event_trb *)event_trb, udc);
		break;
	case DEV_EVENT_TRB:
		/* disable device auto sof
		 * Nont:
		 * Prevent automatically generated SOF
		 * from affecting detection of device disconnection
		 */
		writel(bitfield_replace(readl(&USBx->GL_CS), 9, 1, 0), &USBx->GL_CS);

		switch (ret) {
		case UDC_RESET:
			UDC_LOGL("bus reset\n");

			pwr_uphy_pll(1);

			USBx->EP0_CS &= ~EP_EN;		/* dislabe ep0 */
			if (udc->driver->reset)
				udc->driver->reset(&udc->gadget);

			if (!test_mode)
				mod_timer(&udc->before_sof_polling_timer, jiffies + 1 * HZ);

			break;
		case UDC_SUSPEND:
			UDC_LOGL("udc suspend\n");

#ifndef CONFIG_USB_SUNPLUS_SP7350_OTG
			pwr_uphy_pll(0);
#endif

			break;
		case UDC_RESUME:
			UDC_LOGL("udc resume\n");

			pwr_uphy_pll(1);

			break;
		case UDC_STOPED:
			UDC_LOGL("udc stopped\n");
			if (udc->usb_test_mode)
				writel(bitfield_replace(readl(&USBx->GL_CS), 12, 4, 0), &USBx->GL_CS);

#if 0
			hal_udc_sw_stop_handle(udc);
#endif

			break;
		default:
			UDC_LOGE("not support:ret = %d\n", ret);
			return;
		}

		break;
	case SOF_EVENT_TRB:
		UDC_LOGL("sof event trb\n");

		break;
	}
}

static void check_event_ring(struct sp_udc *udc)
{
	uint32_t temp_event_sg_count = udc->current_event_ring_seg;
	uint8_t temp_ccs = udc->event_ccs;
	struct udc_ring *tmp_event_ring = NULL;
	struct trb_data *event_ring_dq = NULL;
	uint32_t valid_event_count = 0;
	struct trb_data *end_trb;
	bool found = false;
	uint32_t trb_cc = 0;

	if (!udc->event_ring || !udc->driver) {
		UDC_LOGD("handle_event return\n");

		return;
	}

	tmp_event_ring = &udc->event_ring[temp_event_sg_count];
	end_trb = tmp_event_ring->end_trb_va;

	/* event ring dq */
	event_ring_dq = udc->event_ring_dq;

	/* Count the valid events this time */
	while (1) {
		trb_cc = ETRB_C(event_ring_dq->entry3);
		if (trb_cc != temp_ccs) {	/*invaild event trb*/
			if (found)
				event_ring_dq--;

			break;
		} else {
			found = true;
		}

		if (hal_udc_check_trb_type(event_ring_dq) == DEV_EVENT_TRB) {
			if (UDC_SUSPEND == check_trb_status(event_ring_dq))
				UDC_LOGD("UDC SUSPEND\n");
		}

		/* switch event segment */
		if (end_trb == event_ring_dq) {
			if (temp_event_sg_count == (udc->event_ring_seg_total - 1)) {
				temp_event_sg_count = 0;
				/* The last segment,flip ccs */
				temp_ccs = (~temp_ccs) & 0x1;
			} else {
				temp_event_sg_count++;
			}

			event_ring_dq = udc->event_ring[temp_event_sg_count].trb_va;
			end_trb = udc->event_ring[temp_event_sg_count].end_trb_va;
		} else {
			event_ring_dq++;
		}

		valid_event_count++;
	}

	UDC_LOGD("find %d,%d\n", found, valid_event_count);
}

static void handle_event(struct sp_udc *udc)
{
	uint32_t temp_event_sg_count = udc->current_event_ring_seg;
	uint8_t temp_ccs = udc->event_ccs;
	struct udc_ring *tmp_event_ring = NULL;
	volatile struct udc_reg *USBx = udc->reg;
	struct trb_data *event_ring_dq = NULL;
	uint32_t valid_event_count = 0;
	struct trb_data *end_trb;
	bool found = false;
	uint32_t erdp_reg = 0;
	uint32_t trb_cc = 0;
	unsigned long flags;

	spin_lock_irqsave(&udc->lock, flags);
	if (!udc->event_ring || !udc->driver) {
		UDC_LOGD("handle_event return\n");
		spin_unlock_irqrestore(&udc->lock, flags);

		return;
	}

	tmp_event_ring = &udc->event_ring[temp_event_sg_count];
	end_trb = tmp_event_ring->end_trb_va;

	/* event ring dq */
	event_ring_dq = udc->event_ring_dq;

	/* Count the valid events this time */
	while (1) {
		trb_cc = ETRB_C(event_ring_dq->entry3);
		if (trb_cc != temp_ccs) {	/* invaild event trb */
			if (found)
				event_ring_dq--;

			break;
		} else {
			found = true;
		}

		/* switch event segment */
		if (end_trb == event_ring_dq) {
			if (temp_event_sg_count == (udc->event_ring_seg_total - 1)) {
				temp_event_sg_count = 0;
				/* The last segment, flip ccs */
				temp_ccs = (~temp_ccs) & 0x1;
			} else {
				temp_event_sg_count++;
			}

			event_ring_dq = udc->event_ring[temp_event_sg_count].trb_va;
			end_trb = udc->event_ring[temp_event_sg_count].end_trb_va;
		} else {
			event_ring_dq++;
		}

		valid_event_count++;
	}

	if (valid_event_count > 0) {
		UDC_LOGD("------ valid event %d -------\n", valid_event_count);

		/* reacquire ring dq */
		event_ring_dq = udc->event_ring_dq;
	} else {
		UDC_LOGD("------ no event %p -------\n", udc->event_ring_dq);
	}
	spin_unlock_irqrestore(&udc->lock, flags);

	while (valid_event_count > 0) {
		trb_cc = ETRB_C(event_ring_dq->entry3);

		/* trb_cc is used or not */
		if (trb_cc != udc->event_ccs) {
			break;
		} else {
			UDC_LOGD("------ event %d -------\n", valid_event_count);

			hal_udc_analysis_event_trb(event_ring_dq, udc);

			if (end_trb == event_ring_dq) {
				if (udc->current_event_ring_seg == (udc->event_ring_seg_total - 1)) {
					udc->current_event_ring_seg = 0;
					udc->event_ccs = (~udc->event_ccs) & 0x1;
				} else {
					udc->current_event_ring_seg++;
				}
				event_ring_dq = udc->event_ring[udc->current_event_ring_seg].trb_va;
				end_trb = udc->event_ring[temp_event_sg_count].end_trb_va;
			} else {
				event_ring_dq++;
			}

			valid_event_count--;
		}
	}

	spin_lock_irqsave(&udc->lock, flags);
	udc->event_ring_dq = event_ring_dq;
	erdp_reg = USBx->DEVC_ERDP & DESI;
	USBx->DEVC_ERDP = (uint32_t)(udc->event_ring[udc->current_event_ring_seg].trb_pa +
					((event_ring_dq - udc->event_ring[udc->current_event_ring_seg].trb_va) *
						sizeof(struct trb_data))) | erdp_reg;

	UDC_LOGD("ERDP_reg %x, %px\n", USBx->DEVC_ERDP, event_ring_dq);

	USBx->DEVC_ERDP |= EHB;

	spin_unlock_irqrestore(&udc->lock, flags);
}

static void hal_dump_event_ring(struct sp_udc *udc)
{
	struct udc_ring *tmp_event_ring = NULL;
	volatile struct udc_reg *USBx = udc->reg;
	uint32_t seg = 0;
	uint32_t trb_index = 0;
	struct trb_data *trb;

	pr_notice("udc event ccs:%d\n", udc->event_ccs);
	pr_notice("udc event seg table: %d\n", udc->event_ring_seg_total);
	pr_notice("udc event ring dq:%px,%x\n", udc->event_ring_dq, USBx->DEVC_ERDP);

	for (seg = 0; seg < udc->event_ring_seg_total; seg++) {
		tmp_event_ring = &udc->event_ring[seg];
		if (!tmp_event_ring) {
			pr_info("Not evnet ring %d\n", seg);

			continue;
		}

		pr_notice("event ring:%d\n", seg);
		pr_notice("len:%d\n", tmp_event_ring->num_mem);
		pr_notice("start:%px,%llx\n", tmp_event_ring->trb_va, tmp_event_ring->trb_pa);
		pr_notice("end:%px,%llx\n", tmp_event_ring->end_trb_va, tmp_event_ring->end_trb_pa);
		pr_notice("va[pa]:index | entry0,entry1,entry2,entry3 |t:type,ccs:ccs\n");

		for (trb_index = 0; trb_index < tmp_event_ring->num_mem; trb_index++) {
			trb = &tmp_event_ring->trb_va[trb_index];
			pr_notice("%px[%llx]:%2d | %8x,%8x,%8x,%8x |t:%4d,ccs:%d\n", trb,
			tmp_event_ring->trb_pa + (trb_index * ENTRY_SIZE), trb_index,
			trb->entry0, trb->entry1, trb->entry2, trb->entry3,
			LTRB_TYPE((uint32_t)trb->entry3),
			ETRB_C(trb->entry3));
		}
	}
}

static void hal_dump_ep_desc(struct sp_udc *udc)
{
	volatile struct udc_reg *USBx = udc->reg;
	uint32_t ep_index = 0;
	struct sp_desc *ep_desc;

	pr_notice("\n\nep count:%d\n", UDC_MAX_ENDPOINT_NUM);
	pr_notice("ep desc %px,%llx\n", udc->ep_desc, udc->ep_desc_pa);
	pr_notice("DEVC_ADDR:%x\n", USBx->DEVC_ADDR);
	pr_notice("va[pa]:index | entry0,entry1,entry2,entry3 |reg:\n");

	for (ep_index = 0; ep_index < UDC_MAX_ENDPOINT_NUM; ep_index++) {
		ep_desc = &udc->ep_desc[ep_index];

		pr_notice("%px[%llx]:%2d | %8x,%8x,%8x,%8x |reg:%x\n", ep_desc,
		udc->ep_desc_pa + (ep_index * ENTRY_SIZE), ep_index,
		ep_desc->entry0, ep_desc->entry1, ep_desc->entry2, ep_desc->entry3,
		ep_index ? USBx->EPN_CS[ep_index - 1] : USBx->EP0_CS);
	}
}

static void hal_dump_ep_ring(struct sp_udc *udc, uint8_t ep_num)
{
	struct udc_ring *tmp_trb_ring = NULL;
	uint32_t trb_index = 0;
	struct trb_data *trb;
	struct udc_endpoint *ep = NULL;
	unsigned long flags;

	hal_dump_ep_desc(udc);
	ep = &udc->ep_data[ep_num];

	spin_lock_irqsave(&ep->lock, flags);

	if (!ep->ep_trb_ring_dq) {
		pr_notice("EP%d not config\n", ep_num);
		spin_unlock_irqrestore(&ep->lock, flags);

		return;
	}
	tmp_trb_ring = &ep->ep_transfer_ring;

	pr_notice("\n\nEP%d[%c] t:%d MPS:%d dq_trb:%px\n", ep_num, ep->is_in ? 'I' : 'O',
				ep->type, ep->maxpacket, ep->ep_trb_ring_dq);
	pr_notice("start:%px,%llx\n", tmp_trb_ring->trb_va, tmp_trb_ring->trb_pa);
	pr_notice("end:%px,%llx\n", tmp_trb_ring->end_trb_va, tmp_trb_ring->end_trb_pa);
	pr_notice("len:%d", tmp_trb_ring->num_mem);
	pr_notice("va[pa]:index | entry0,entry1,entry2,entry3 |t:type\n");

	for (trb_index = 0; trb_index < tmp_trb_ring->num_mem; trb_index++) {
		trb = &tmp_trb_ring->trb_va[trb_index];
		pr_notice("%px[%llx]:%2d | %8x,%8x,%8x,%8x |t:%4d\n", trb,
		tmp_trb_ring->trb_pa + (trb_index * ENTRY_SIZE), trb_index,
		trb->entry0, trb->entry1, trb->entry2, trb->entry3,
		LTRB_TYPE((uint32_t)trb->entry3));
	}

	spin_unlock_irqrestore(&ep->lock, flags);
}

static irqreturn_t sp_udc_irq(int dummy, void *_dev)
{
	volatile struct udc_reg *USBx;
	struct sp_udc *udc = _dev;

	USBx = udc->reg;
	udc_irq_in_count++;

	spin_lock(&udc->lock);
	if ((USBx->DEVC_STS & EINT) && (USBx->DEVC_IMAN & DEVC_INTR_PENDING)) {
		USBx->DEVC_STS |= EINT;
		USBx->DEVC_IMAN |= DEVC_INTR_PENDING;
		tasklet_schedule(&udc->event_task);
	} else if (USBx->DEVC_STS & VBUS_CI) {
		USBx->DEVC_STS |= VBUS_CI;
	}
	spin_unlock(&udc->lock);

	udc_irq_out_count++;

	return IRQ_HANDLED;
}

static void fill_link_trb(struct trb_data *t_trb, dma_addr_t ring)
{
	t_trb->entry0 = ring;
	t_trb->entry3 |= (LINK_TRB<<10);
	if (t_trb->entry3 & TRB_C) {
		t_trb->entry3 &= ~TRB_C;
	} else {
		t_trb->entry3 |= TRB_C;		/* toggle cycle bit */
	}

	UDC_LOGD("--- fill link trb ---\n");
}

static void hal_udc_fill_transfer_trb(struct trb_data *t_trb, struct udc_endpoint *ep, uint32_t ioc)
{
	struct normal_trb *tmp_trb = (struct normal_trb *)t_trb;
	int len_zh = 0;
	int len_yu = 0;

	len_zh = ep->transfer_len / CACHE_LINE_SIZE;
	len_yu = ep->transfer_len % CACHE_LINE_SIZE;
	if (len_yu)
		len_zh = (len_zh + 1) * CACHE_LINE_SIZE;
	else
		len_zh = len_zh * CACHE_LINE_SIZE;

	tmp_trb->tlen = ep->transfer_len;
	tmp_trb->ptr = (uint32_t)ep->transfer_buff_pa;

	/* TRB function setting */
	if (!ep->is_in) {
		tmp_trb->dir = 0;	/* 0 is out */
		if (ep->transfer_len)
			dma_sync_single_for_cpu(ep->dev->dev, tmp_trb->ptr, len_zh, DMA_FROM_DEVICE);
	} else {
		tmp_trb->dir = 1;	/* 1 is IN */
		if (ep->transfer_len)
			dma_sync_single_for_device(ep->dev->dev, tmp_trb->ptr, len_zh, DMA_TO_DEVICE);
	}

	if (ep->type == UDC_EP_TYPE_ISOC)
		tmp_trb->sia = 1;	/* start ISO ASAP, modify */

	tmp_trb->isp = 1;

	if (ioc)
		tmp_trb->ioc = 1;	/* create event trb */
	else
		tmp_trb->ioc = 0;

	tmp_trb->type = 1;		/* trb type */

	/* valid trb */
	if (tmp_trb->cycbit == 1)
		tmp_trb->cycbit = 0;	/* set cycle bit 0 */
	else
		tmp_trb->cycbit = 1;
}

static void hal_udc_fill_ep_desc(struct sp_udc *udc, struct udc_endpoint *ep)
{
	volatile struct udc_reg *USBx = udc->reg;
	struct endpoint0_desc *tmp_ep0_desc = NULL;
	struct endpointn_desc *tmp_epn_desc = NULL;

	if (ep->num == 0) {
		tmp_ep0_desc = (struct endpoint0_desc *)udc->ep_desc;
		UDC_LOGD("%s.%d ep%d tmp_ep0_desc = %px\n", __func__, __LINE__, ep->num, tmp_ep0_desc);
		tmp_ep0_desc->cfgs = AUTO_RESPONSE;					/* auto response configure setting */
		tmp_ep0_desc->cfgm = AUTO_RESPONSE;					/* auto response configure setting */
		tmp_ep0_desc->speed = udc->def_run_full_speed ? UDC_FULL_SPEED : UDC_HIGH_SPEED; /* high speed */
#ifdef CONFIG_SOC_Q645
		tmp_ep0_desc->aset = AUTO_SET_CONF | AUTO_SET_INF;			/* auto setting config & interface */
#elif defined(CONFIG_SOC_SP7350)
		tmp_ep0_desc->aset = AUTO_SET_CONF | AUTO_SET_INF | AUTO_SET_ADDR;	/* auto setting config & interface & address */
#endif
		tmp_ep0_desc->dcs = udc->event_ccs;					/* set cycle bit 1 */
		tmp_ep0_desc->sofic = 0;
		tmp_ep0_desc->dptr = SHIFT_LEFT_BIT4(ep->ep_transfer_ring.trb_pa);

		UDC_LOGD("ep0, dptr:0x%x\n", tmp_ep0_desc->dptr);
	} else {
		tmp_epn_desc = (struct endpointn_desc *)(udc->ep_desc + ep->num);

		UDC_LOGD("%s.%d ep%d tmp_epn_desc = %px\n", __func__, __LINE__, ep->num, tmp_epn_desc);

		tmp_epn_desc->ifm = AUTO_RESPONSE;			/* auto response interface setting */
		tmp_epn_desc->altm = AUTO_RESPONSE;			/* auto response alternated setting */
		tmp_epn_desc->num = ep->num;
		tmp_epn_desc->type = ep->type;

		/* If endpoint type is ISO or INT */
		if ((ep->type == UDC_EP_TYPE_ISOC) || (ep->type == UDC_EP_TYPE_INTR))
			tmp_epn_desc->mult = FRAME_TRANSFER_NUM_3;

		tmp_epn_desc->dcs = 1;					/* set cycle bit 1 */
		tmp_epn_desc->mps = ep->maxpacket;
		tmp_epn_desc->dptr = SHIFT_LEFT_BIT4(ep->ep_transfer_ring.trb_pa);

		UDC_LOGD("%s.%d ep%d, dptr:0x%x\n", __func__, __LINE__,
					ep->num, tmp_epn_desc->dptr);
	}
	wmb();

	if (ep->num == 0)
		USBx->EP0_CS |= RDP_EN;					/* Endpoint register enable,RDP enable */
	else
		USBx->EPN_CS[ep->num - 1] |= RDP_EN;			/* Endpoint register enable,RDP enable */

}

static struct trb_data *hal_udc_fill_trb(struct sp_udc	*udc, struct udc_endpoint *ep, uint32_t zero)
{
	struct trb_data *end_trb;
	struct trb_data *fill_trb = NULL;

	end_trb = ep->ep_transfer_ring.end_trb_va;

	if (end_trb == ep->ep_trb_ring_dq) {
		fill_link_trb(ep->ep_trb_ring_dq, ep->ep_transfer_ring.trb_pa);
		ep->ep_trb_ring_dq->entry3 |= LTRB_TC;			/* toggle cycle bit */
		ep->ep_trb_ring_dq = ep->ep_transfer_ring.trb_va;
	}

	hal_udc_fill_transfer_trb(ep->ep_trb_ring_dq, ep, 1);
	fill_trb = ep->ep_trb_ring_dq;
	ep->ep_trb_ring_dq++;

	/* EP 0 send/receive zero packet */
	if (0 == ep->num && ep->transfer_len > 0) {
		if (end_trb == ep->ep_trb_ring_dq) {
			fill_link_trb(ep->ep_trb_ring_dq, ep->ep_transfer_ring.trb_pa);
			ep->ep_trb_ring_dq->entry3 |= LTRB_TC;		/* toggle cycle bit */
			ep->ep_trb_ring_dq = ep->ep_transfer_ring.trb_va;
		}

		/* len == MPS and len < setup len */
		if (zero && (ep->transfer_len == ep->maxpacket)) {
			ep->transfer_buff = NULL;
			ep->transfer_len = 0;
			hal_udc_fill_transfer_trb(ep->ep_trb_ring_dq, ep, 0);
			ep->ep_trb_ring_dq++;

			if (end_trb == ep->ep_trb_ring_dq) {
				fill_link_trb(ep->ep_trb_ring_dq, ep->ep_transfer_ring.trb_pa);
				ep->ep_trb_ring_dq->entry3 |= LTRB_TC;	/* toggle cycle bit */
				ep->ep_trb_ring_dq = ep->ep_transfer_ring.trb_va;
			}
		}

		/* ACK handshake */
		if (ep->is_in)
			ep->is_in = false;				/* ep0 rx zero packet */
		else
			ep->is_in = true;				/* ep0 tx zero packet */

		ep->transfer_buff = NULL;
		ep->transfer_len = 0;
		hal_udc_fill_transfer_trb(ep->ep_trb_ring_dq, ep, 0);
		ep->ep_trb_ring_dq++;
	}

	if (zero && ep->type == UDC_EP_TYPE_BULK && ep->is_in && ep->transfer_len > 0
						&& (ep->transfer_len % ep->maxpacket) == 0) {
		if (end_trb == ep->ep_trb_ring_dq) {
			fill_link_trb(ep->ep_trb_ring_dq, ep->ep_transfer_ring.trb_pa);
			ep->ep_trb_ring_dq->entry3 |= LTRB_TC;		/* toggle cycle bit */
			ep->ep_trb_ring_dq = ep->ep_transfer_ring.trb_va;
		}

		/* send/receive zero packet */
		ep->transfer_buff = NULL;
		ep->transfer_len = 0;
		hal_udc_fill_transfer_trb(ep->ep_trb_ring_dq, ep, 0);
		ep->ep_trb_ring_dq++;
	}

	wmb();
	if (ep->num == 0)
		udc->reg->EP0_CS |= EP_EN;
	else
		udc->reg->EPN_CS[ep->num-1] |= EP_EN;

	return fill_trb;
}

int32_t hal_udc_endpoint_transfer(struct sp_udc	*udc, struct sp_request *req, uint8_t ep_addr,
					uint8_t *data, dma_addr_t data_pa, uint32_t length, uint32_t zero)
{
	struct udc_endpoint *ep;
#if (TRANS_MODE == DMA_MODE)
	int len_zh = 0;
	int len_yu = 0;
#endif

	if (EP_NUM(ep_addr) > UDC_MAX_ENDPOINT_NUM) {
		UDC_LOGE("ep_addr parameter error, max endpoint num is %d.\n", UDC_MAX_ENDPOINT_NUM);
		return -EINVAL;
	}

	ep = &udc->ep_data[EP_NUM(ep_addr)];
	if (0 == ep->type && EP_NUM(ep_addr) != 0) {
		UDC_LOGE("ep%d not configure!\n", EP_NUM(ep_addr));
		return -EINVAL;
	}

	if (!ep->ep_trb_ring_dq) {
		UDC_LOGE("ep%d not configure2!\n", EP_NUM(ep_addr));
		return -EINVAL;
	}

	ep->is_in = EP_DIR(ep_addr);
	ep->transfer_len = length;

#if (TRANS_MODE == DMA_MODE)
	if (length != 0) {
		len_zh = length / CACHE_LINE_SIZE;
		len_yu = length % CACHE_LINE_SIZE;

		if (len_yu)
			len_zh = (len_zh + 1) * CACHE_LINE_SIZE;
		else
			len_zh = len_zh * CACHE_LINE_SIZE;

		if (ep->is_in)
			dma_sync_single_for_device(udc->dev, virt_to_phys(data), len_zh, DMA_TO_DEVICE);

		ep->transfer_buff = data;
		ep->transfer_buff_pa = virt_to_phys(data);
	} else {
		ep->transfer_buff = NULL;
		ep->transfer_buff_pa = 0;
	}
#elif (TRANS_MODE == DMA_MAP)
	if (length != 0) {
		ep->transfer_buff = data;
		ep->transfer_buff_pa = data_pa;
	} else {
		ep->transfer_buff = NULL;
		ep->transfer_buff_pa = 0;
	}
#endif

	/* Controller automatically responds to set config and set interface,
	   So there is no need to fill in trb.*/
	if ((0 == ep->num) && udc->aset_flag) {
		UDC_LOGD("auto set\n");
		udc->aset_flag = false;
		req->transfer_trb = NULL;

#if (TRANS_MODE == DMA_MAP)
		usb_gadget_unmap_request(&udc->gadget, &req->req, ep->is_in);
#endif

		list_del(&req->queue);

		return 0;
	}

	req->transfer_trb = hal_udc_fill_trb(udc, ep, zero);

	return 0;
}

int32_t hal_udc_endpoint_stall(struct sp_udc *udc, uint8_t ep_addr, bool stall)
{
	volatile struct udc_reg *USBx = udc->reg;
	struct udc_endpoint *ep = NULL;

	UDC_LOGD("+%s.%d\n", __func__, __LINE__);

	ep = &udc->ep_data[EP_NUM(ep_addr)];

	/* Set stall */
	#if 0
	if ((stall) && (ep->is_in == true) && (!list_empty (&ep->queue)) &&
					((USBx->EPN_CS[ep->num-1] & EP_EN) == EP_EN))
		return -EAGAIN;
	#else
	mdelay(1);
	#endif

	if (stall) {
		if (ep->num == 0)
			USBx->EP0_CS |= EP_STALL;
		else
			USBx->EPN_CS[ep->num-1] |= EP_STALL;

		ep->status = ENDPOINT_HALT;
	} else {
		/* Clear stall */
		if (ep->num == 0)
			USBx->EP0_CS &= ~EP_STALL;
		else
			USBx->EPN_CS[ep->num-1] &= ~EP_STALL;

		ep->status = ENDPOINT_READY;
	}

	return 0;
}

int32_t hal_udc_device_connect(struct sp_udc *udc)
{
	volatile struct udc_reg *USBx = udc->reg;
	struct udc_ring *tmp_ring = NULL;
	uint32_t seg_count = 0;

	hal_udc_power_control(udc, UDC_POWER_OFF);

	UDC_LOGI("+%s.%d %x\n", __func__, __LINE__, USBx->DEVC_CS);

	for (seg_count = 0; seg_count < udc->event_ring_seg_total; seg_count++) {
		tmp_ring = &udc->event_ring[seg_count];
		if (tmp_ring->trb_va)
			memset(tmp_ring->trb_va, 0, tmp_ring->num_mem * ENTRY_SIZE);
	}
	udc->event_ccs = 1;

	USBx->DEVC_STS = CLEAR_INT_VBUS;
	USBx->DEVC_CTRL = VBUS_DIS;

	/* fill register */
	/* event ring segnmet unmber */
	USBx->DEVC_ERSTSZ = udc->event_ring_seg_total;

	/* event ring dequeue pointers  */
	USBx->DEVC_ERDP = udc->event_ring[0].trb_pa;
	udc->event_ring_dq = udc->event_ring[0].trb_va;

	/* event ring segment table base address */
	USBx->DEVC_ERSTBA = udc->event_seg_table_pa;

	/* set interrupt moderation */
	USBx->DEVC_IMOD = 0;

	/* init step j */
	USBx->DEVC_ADDR = udc->ep_desc_pa;

	/* configure ep0 */
	hal_udc_endpoint_configure(udc, 0x0, UDC_EP_TYPE_CTRL, 0x40);

	/* enable interrupt */
	hal_udc_power_control(udc, UDC_POWER_FULL);

	/* run controller and reload ep0 */
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	if (otg_id_pin == 1)
		USBx->DEVC_CS = (UDC_RUN | EP_EN);
	else if (otg_id_pin == 0)
		USBx->DEVC_CS = EP_EN;
#else
	USBx->DEVC_CS = EP_EN;
#endif

	return 0;
}

int32_t hal_udc_device_disconnect(struct sp_udc *udc)
{
	volatile struct udc_reg *USBx = udc->reg;

	USBx->DEVC_CS = 0;
	UDC_LOGI("+%s.%d %x\n", __func__, __LINE__, USBx->DEVC_CS);

	return 0;
}

int32_t hal_udc_init(struct sp_udc *udc)
{
	struct trb_data *tmp_segment_table = NULL;
	struct udc_ring *tmp_ring = NULL;
	struct udc_endpoint *ep = NULL;
	uint8_t seg_count = 0;
	int32_t i;

	UDC_LOGI("version = 0x%x, offsets = 0x%x, parameter = 0x%x\n",
					udc->reg->DEVC_VER, udc->reg->DEVC_MMR, udc->reg->DEVC_PARAM);

	udc->event_ring_seg_total = EVENT_SEG_COUNT;
	udc->event_ccs = 1;
	udc->current_event_ring_seg = 0;

	/* malloc event ring segment teable */
	udc->event_seg_table = (struct segment_table *)udc_malloc_align(udc,
		&udc->event_seg_table_pa, udc->event_ring_seg_total * ENTRY_SIZE, ALIGN_64_BYTE, GFP_DMA);
	if (!udc->event_seg_table) {
		UDC_LOGE("mem_alloc event_ring_seg_table fail\n");
		goto event_seg_table_malloc_fail;
	}

	UDC_LOGD("event segment table %px,%llx,%d\n", udc->event_seg_table, udc->event_seg_table_pa,
										udc->event_ring_seg_total);

	/* malloc event ring pointer */
	udc->event_ring = (struct udc_ring *)udc_malloc_align(udc, &udc->event_ring_pa,
				udc->event_ring_seg_total * sizeof(struct udc_ring), ALIGN_64_BYTE, GFP_DMA);
	if (!udc->event_ring) {
		UDC_LOGE("mem_alloc evnet_ring fail\n");

		goto event_ring_malloc_fail;
	}

	UDC_LOGD("event ring %px,%llx\n", udc->event_ring, udc->event_ring_pa);

	/* malloc evnet ring and fill the event ring segment table */
	for (seg_count = 0; seg_count < udc->event_ring_seg_total; seg_count++) {
		tmp_segment_table = (struct trb_data *)((udc->event_seg_table) + seg_count);
		tmp_ring = &udc->event_ring[seg_count];
		udc_ring_malloc(udc, tmp_ring, EVENT_RING_COUNT);
		if (!tmp_ring->trb_va) {
			UDC_LOGE("mem_alloc event_ring %d fail\n", seg_count);

			goto event_ring_malloc_fail;
		}
		/* fill the event ring segment table */
		tmp_segment_table->entry0 = tmp_ring->trb_pa;
		tmp_segment_table->entry2 = tmp_ring->num_mem;

		UDC_LOGD("Event_ring[%d]:%px,%llx -> %px,%llx\n", seg_count, udc->event_ring[seg_count].trb_va,
						udc->event_ring[seg_count].trb_pa, tmp_ring->trb_va, tmp_ring->trb_pa);
	}

	/* malloc ep description */
	udc->ep_desc = (struct sp_desc *)udc_malloc_align(udc, &udc->ep_desc_pa,
				ENTRY_SIZE * UDC_MAX_ENDPOINT_NUM, ALIGN_32_BYTE, GFP_DMA);
	if (!udc->ep_desc) {
		UDC_LOGE("udc_malloc_align device desc fail\n");

		goto fail;
	}

	UDC_LOGD("device descriptor addr:%px,%llx\n", udc->ep_desc, udc->ep_desc_pa);


	/* endpoint init */
	for (i = 0U; i < UDC_MAX_ENDPOINT_NUM; i++) {
		ep = &udc->ep_data[i];
		ep->is_in = false;
		ep->num = i;

		/* Control until ep is activated */
		ep->type = 0;
		ep->maxpacket = 0U;
		ep->transfer_buff = NULL;
		ep->transfer_len = 0U;
		ep->ep_transfer_ring.trb_va = NULL;
		ep->ep_transfer_ring.num_mem = TRANSFER_RING_COUNT;
		ep->ep_trb_ring_dq = NULL;

#ifndef EP_DYNAMIC_ALLOC
		udc_ring_malloc(udc, &ep->ep_transfer_ring, TRANSFER_RING_COUNT);

		if (!ep->ep_transfer_ring.trb_va) {
			UDC_LOGE("ep%d malloc trb fail\n", i);

			ep->ep_transfer_ring.trb_va = NULL;
		}
#endif

		UDC_LOGD("ep[%d]:%px\n", i, ep);
	}

	return 0;

fail:
event_ring_malloc_fail:
	if (udc->event_ring) {
		for (i = 0; i < udc->event_ring_seg_total; i++) {
			if (udc->event_ring[i].trb_va) {
				udc_free_align(udc, udc->event_ring[i].trb_va,
					udc->event_ring[i].trb_pa, udc->event_ring[i].num_mem * ENTRY_SIZE);
			}
		}

		udc_free_align(udc, udc->event_ring,
			udc->event_ring_pa, udc->event_ring_seg_total * sizeof(struct udc_ring));
	}

event_seg_table_malloc_fail:
	if (udc->event_seg_table) {
		udc_free_align(udc, udc->event_seg_table,
			udc->event_seg_table_pa, udc->event_ring_seg_total * ENTRY_SIZE);
	}

	return -EPERM;
}

int32_t hal_udc_deinit(struct sp_udc *udc)
{
	struct udc_endpoint *ep;
	int32_t i;

	UDC_LOGI("udc %d deinit\n", udc->port_num);

	if (udc->ep_desc) {
		udc_free_align(udc, udc->ep_desc, udc->ep_desc_pa, ENTRY_SIZE * UDC_MAX_ENDPOINT_NUM);
		udc->ep_desc = NULL;
	}

	if (udc->event_ring) {
		for (i = 0; i < udc->event_ring_seg_total; i++) {
			if (udc->event_ring[i].trb_va)
				udc_ring_free(udc, &udc->event_ring[i]);
		}

		udc_free_align(udc, udc->event_ring, udc->event_ring_pa,
							udc->event_ring_seg_total * sizeof(struct udc_ring));
		udc->event_ring = NULL;
	}

	if (udc->event_seg_table) {
		udc_free_align(udc, udc->event_seg_table, udc->event_seg_table_pa,
							udc->event_ring_seg_total * ENTRY_SIZE);
		udc->event_seg_table = NULL;
	}

	/* endpoint init */
	for (i = 0U; i < UDC_MAX_ENDPOINT_NUM; i++) {
		ep = &udc->ep_data[i];
		ep->ep_trb_ring_dq = NULL;

		if (ep->ep_transfer_ring.trb_va)
			udc_ring_free(udc, &ep->ep_transfer_ring);
	}

	return 0;
}

int32_t hal_udc_endpoint_unconfigure(struct sp_udc *udc, uint8_t ep_addr)
{
	uint8_t ep_num;
	struct udc_endpoint *ep;
	volatile struct udc_reg *USBx = udc->reg;

	UDC_LOGI("unconfig EP %x\n", ep_addr);
	ep_num = EP_NUM(ep_addr);
	ep = &udc->ep_data[ep_num];

	spin_lock(&ep->lock);

	ep->maxpacket = 0U;
	ep->is_in = false;
	ep->num   = 0U;
	ep->type = 0U;
	ep->ep_trb_ring_dq = NULL;

	if (0 == ep_num)
		USBx->EP0_CS &= ~(RDP_EN | EP_EN); 	/* disable ep */
	else
		USBx->EPN_CS[ep_num - 1] &= ~(RDP_EN | EP_EN);

#ifdef	EP_DYNAMIC_ALLOC
	if (ep->ep_transfer_ring.trb_va != NULL)
		udc_ring_free(udc, &ep->ep_transfer_ring);
#endif

	if (!list_empty(&ep->queue))
		UDC_LOGW("ep%d list not empty\n", ep->num);

	spin_unlock(&ep->lock);

	return 0;
}

int32_t hal_udc_endpoint_configure(struct sp_udc *udc, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_max_packet_size)
{
	struct udc_endpoint *ep;
	uint8_t ep_num = EP_NUM(ep_addr);

	UDC_LOGI("ep%x,%x type:%x mps:%x config\n", ep_addr, ep_num, ep_type, ep_max_packet_size);

	if (ep_num >= UDC_MAX_ENDPOINT_NUM) {
		UDC_LOGE("ep_addr parameter error, max endpoint num is %d,%d\n", UDC_MAX_ENDPOINT_NUM, ep_num);

		return -EINVAL;
	}

	if (ep_type != UDC_EP_TYPE_CTRL && ep_type != UDC_EP_TYPE_ISOC
		&& ep_type != UDC_EP_TYPE_BULK && ep_type != UDC_EP_TYPE_INTR) {
		UDC_LOGE("ep_type parameter error, unsupported type!\n");

		return -EINVAL;
	}

	ep = &udc->ep_data[ep_num];
	spin_lock(&ep->lock);

	if (!list_empty(&ep->queue)) {
		UDC_LOGW("ep%d queue not empty\n", ep_num);

		spin_unlock(&ep->lock);

		return -EINVAL;
	}

	if (ep->ep_trb_ring_dq) {
		UDC_LOGE("ep%d repeated initialization fail\n", ep_num);

		spin_unlock(&ep->lock);

		return -EINVAL;
	}

	ep->maxpacket = ep_max_packet_size;
	ep->is_in = EP_DIR(ep_addr);
	ep->num = ep_num;
	ep->type = ep_type;

#ifdef EP_DYNAMIC_ALLOC
	/* init step h */
	udc_ring_malloc(udc, &ep->ep_transfer_ring, TRANSFER_RING_COUNT);
	UDC_LOGD("+%s.%d\n", __func__, __LINE__);
#endif

	if (!ep->ep_transfer_ring.trb_va) {
		UDC_LOGE("udc_malloc_align ep_transfer_ring %d fail\n", ep_num);
		spin_unlock(&ep->lock);

		return -ENOMEM;
	}

	memset(ep->ep_transfer_ring.trb_va, 0, ep->ep_transfer_ring.num_mem * ENTRY_SIZE);
	INIT_LIST_HEAD(&ep->queue);

	ep->ep_trb_ring_dq = ep->ep_transfer_ring.trb_va;
	UDC_LOGD("ep_transfer_ring[%d]:%px,%llx\n", ep_num, ep->ep_transfer_ring.trb_va,
									ep->ep_transfer_ring.trb_pa);
	hal_udc_fill_ep_desc(udc, ep);

	spin_unlock(&ep->lock);

	return 0;
}

/*
   enable udc interrupt
   UDC_POWER_OFF: disable interrupt
   UDC_POWER_FULL: enable interrupt
*/
int32_t hal_udc_power_control(struct sp_udc *udc, enum udc_power_state power_state)
{
	volatile struct udc_reg *usbx = udc->reg;

	UDC_LOGD("+%s.%d\n", __func__, __LINE__);

	if ((power_state != UDC_POWER_OFF) && (power_state != UDC_POWER_FULL)) {
		UDC_LOGE("power state error\n");
		return -EINVAL;
	}

	switch (power_state) {
	case UDC_POWER_OFF:
		usbx->DEVC_IMOD = IMOD;			/* interrupt interval */
		usbx->DEVC_IMAN &= ~DEVC_INTR_ENABLE;	/* Disable interrupt */
		break;
	case UDC_POWER_FULL:
		usbx->DEVC_IMAN |= DEVC_INTR_ENABLE;	/* Enable interrupt */
		break;
	case UDC_POWER_LOW:
		UDC_LOGE("no support\n");
		return -EINVAL;
	default:
		return -EINVAL;
	}

	return 0;
}

static void hal_udc_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct sp_udc *udc = NULL;
	volatile struct udc_reg *USBx;

	if (req->status || req->actual != req->length)
		UDC_LOGD("hal udc setup fail\n");

	if (!req->context)
		return;

	udc = req->context;
	USBx = udc->reg;

	if (udc->usb_test_mode) {
		switch (udc->usb_test_mode) {
		case USB_TEST_J:
			UDC_LOGL("TEST_J\n");
			writel(bitfield_replace(readl(&USBx->GL_CS), 12, 4, 0x1), &USBx->GL_CS);
			break;
		case USB_TEST_K:
			UDC_LOGL("TEST_K\n");
			writel(bitfield_replace(readl(&USBx->GL_CS), 12, 4, 0x2), &USBx->GL_CS);
			break;
		case USB_TEST_SE0_NAK:
			UDC_LOGL("TEST_SE0_NAK\n");
			writel(bitfield_replace(readl(&USBx->GL_CS), 12, 4, 0x3), &USBx->GL_CS);
			break;
		case USB_TEST_PACKET:
			UDC_LOGL("TEST_PACKET\n");
			writel(bitfield_replace(readl(&USBx->GL_CS), 12, 4, 0x4), &USBx->GL_CS);
			break;
		case USB_TEST_FORCE_ENABLE:
			UDC_LOGL("TEST_FORCE_ENABLE\n");
			writel(bitfield_replace(readl(&USBx->GL_CS), 12, 4, 0x5), &USBx->GL_CS);
			break;
		default:
			UDC_LOGL("Unsupport teset\n");
		}
	}
}

static int hal_udc_setup(struct sp_udc *udc, const struct usb_ctrlrequest *ctrl)
{
	struct udc_endpoint *ep_0 = &(udc->ep_data[0]);
	struct udc_endpoint *ep = NULL;
	struct usb_gadget *gadget = &udc->gadget;
	struct usb_composite_dev *cdev = get_gadget_data(gadget);
	struct usb_request *req = cdev->req;
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	struct usb_phy *otg_phy;
#endif
	int value = -EOPNOTSUPP;
	u16 w_index = le16_to_cpu(ctrl->wIndex);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 w_length = le16_to_cpu(ctrl->wLength);
	uint8_t ep_num = 0;

	udc->usb_test_mode = 0;

	if (ctrl->bRequestType & USB_DIR_IN) {
		ep_0->bEndpointAddress |= USB_DIR_IN;
	} else {
		if (ctrl->wLength)
			ep_0->bEndpointAddress = USB_DIR_OUT;
		else
			ep_0->bEndpointAddress |= USB_DIR_IN;
	}

	/* enable auto set flag */
	udc->aset_flag = false;

	if ((USB_REQ_SET_CONFIGURATION == ctrl->bRequest
		&& (USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE) == ctrl->bRequestType)
		|| (USB_REQ_SET_INTERFACE == ctrl->bRequest
			&& (USB_DIR_OUT | USB_RECIP_INTERFACE) == ctrl->bRequestType)
		|| (USB_REQ_SET_ADDRESS == ctrl->bRequest && (USB_DIR_OUT) == ctrl->bRequestType)) {
		udc->aset_flag = true;
	}

	value = udc->driver->setup(gadget, ctrl);
	if (value >= 0)
		goto done;

	req->zero = 0;
	req->context = udc;
	req->complete = hal_udc_setup_complete;
	req->length = 0;

	/* request get status */
	switch (ctrl->bRequest) {
	case USB_REQ_GET_STATUS:
		/* request device status */
		if (ctrl->bRequestType == (USB_DIR_IN | USB_RECIP_DEVICE)) {
			value = 2;
			put_unaligned_le16(0, req->buf);
		}

		/* interface status */
		if (ctrl->bRequestType == (USB_DIR_IN | USB_RECIP_INTERFACE)) {
			value = 2;
			put_unaligned_le16(0, req->buf);
		}

		/* endpoint status */
		if (ctrl->bRequestType == (USB_DIR_IN | USB_RECIP_ENDPOINT)) {
			ep_num = EP_NUM(w_index);
			if (ep_num < UDC_MAX_ENDPOINT_NUM) {
				ep = &(udc->ep_data[ep_num]);
				if (ep) {
					value = 2;
					UDC_LOGD("get EP%d status %d\n", ep_num, ep->status);
					put_unaligned_le16(ep->status, req->buf);
				}
			}
		}

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
		if ((ctrl->bRequestType == (USB_DIR_IN | USB_RECIP_DEVICE)) && (ctrl->wValue == 0) &&
						(ctrl->wIndex == OTG_STS_SELECTOR) && (ctrl->wLength == 4)) {
			struct sp_request *req;
			u32 status = 0;
			int32_t ret;

			req = kzalloc (sizeof(struct sp_request), GFP_KERNEL);
			if (!req) {
				UDC_LOGE("+%s.%d,otg req allocation fails\n", __func__, __LINE__);
				return 1;
			}

			req->req.buf = (char *)kmalloc(sizeof(u32), GFP_DMA);
			if (!req->req.buf) {
				UDC_LOGE("+%s.%d,otg req buffer allocation fails\n", __func__, __LINE__);
				return 1;
			}

			otg_status_buf = req->req.buf;
			otg_status_buf_ptr_addr = (char *)&req->req.buf;

			status = dev_otg_status;
			memcpy(req->req.buf, (char *)(&status), 4);
			req->req.length = sizeof(u32);
			req->req.zero = 0;
			req->req.num_sgs = 0;
			req->req.dma = DMA_ADDR_INVALID;

	#if (TRANS_MODE == DMA_MAP)
			ret = usb_gadget_map_request(&udc->gadget, &req->req, EP_DIR(ep_0->bEndpointAddress));
			if (ret) {
				kfree(req->req.buf);
				kfree(req);

				return ret;
			}
	#endif

			ret = hal_udc_endpoint_transfer(udc, req, ep_0->bEndpointAddress, req->req.buf,
								req->req.dma, req->req.length, req->req.zero);

			UDC_LOGD("%s: ep:%x len %d, req:%px,trb:%px\n", __func__, ep_0->bEndpointAddress,
								req->req.length, req, req->transfer_trb);

			if (ret) {
				UDC_LOGE("%s: transfer err\n", __func__);
	#if (TRANS_MODE == DMA_MAP)
				usb_gadget_unmap_request(&udc->gadget, &req->req, EP_DIR(ep_0->bEndpointAddress));
	#endif

				kfree(req->req.buf);
				kfree(req);

				return -EINVAL;
			}

			value = 0;
			goto done;;
		}
#endif

		break;

	/* request set feature */
	case USB_REQ_SET_FEATURE:
		/* request set feature */
		if (ctrl->bRequestType == (USB_DIR_OUT | USB_RECIP_DEVICE)) {
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
			if ((ctrl->bRequestType == 0) && (ctrl->wValue == USB_DEVICE_B_HNP_ENABLE) &&
								(ctrl->wIndex == 0) && (ctrl->wLength == 0)) {
				UDC_LOGD("set hnp feature");

	#ifdef CONFIG_USB_GADGET_PORT0_ENABLED
				otg_phy = usb_get_transceiver_sp(0);
	#else
				otg_phy = usb_get_transceiver_sp(1);
	#endif

				if (!otg_phy)
					UDC_LOGE("Get otg control fail");
				else
					sp_accept_b_hnp_en_feature(otg_phy->otg);

				value = 0;
				break;
			}
#endif

			if (w_value != USB_DEVICE_TEST_MODE && w_value != USB_DEVICE_REMOTE_WAKEUP)
				break;

			if (w_value == USB_DEVICE_TEST_MODE) {
				switch (w_index >> 8) {
				case USB_TEST_J:
					UDC_LOGL("TEST_J\n");
					udc->usb_test_mode = USB_TEST_J;
					value = 0;
					break;
				case USB_TEST_K:
					UDC_LOGL("TEST_K\n");
					udc->usb_test_mode = USB_TEST_K;
					value = 0;
					break;
				case USB_TEST_SE0_NAK:
					UDC_LOGL("TEST_SE0_NAK\n");
					udc->usb_test_mode = USB_TEST_SE0_NAK;
					value = 0;
					break;
				case USB_TEST_PACKET:
					UDC_LOGL("TEST_PACKET\n");
					udc->usb_test_mode = USB_TEST_PACKET;
					value = 0;
					break;
				case USB_TEST_FORCE_ENABLE:
					UDC_LOGL("TEST_FORCE_ENABLE\n");
					udc->usb_test_mode = USB_TEST_FORCE_ENABLE;
					break;
				default:
					UDC_LOGL("Unsupport teset\n");
				}

				if (value)
					break;
			}
		}

		/* interface status */
		if (ctrl->bRequestType == (USB_DIR_OUT | USB_RECIP_INTERFACE))
			break;

		/* endpoint status */
		if (ctrl->bRequestType == (USB_DIR_OUT | USB_RECIP_ENDPOINT)) {
			if (w_value != USB_ENDPOINT_HALT)
				break;

			ep_num = EP_NUM(w_index);
			if (ep_num < UDC_MAX_ENDPOINT_NUM) {
				ep = &(udc->ep_data[ep_num]);
				if (ep)
					sp_udc_set_halt(&ep->ep, 1);
			}
		}

		value = 0;
		break;

	/* request clear feature */
	case USB_REQ_CLEAR_FEATURE:
		/* request clear feature */
		if (ctrl->bRequestType == (USB_DIR_OUT | USB_RECIP_DEVICE)) {
			if (w_value != USB_DEVICE_TEST_MODE || w_value != USB_DEVICE_REMOTE_WAKEUP)
				break;
		}

		/* interface status */
		if (ctrl->bRequestType == (USB_DIR_OUT | USB_RECIP_INTERFACE))
			break;

		/* endpoint status */
		if (ctrl->bRequestType == (USB_DIR_OUT | USB_RECIP_ENDPOINT)) {
			if (w_value != USB_ENDPOINT_HALT)
				break;

			ep_num = EP_NUM(w_index);
			if (ep_num < UDC_MAX_ENDPOINT_NUM) {
				ep = &(udc->ep_data[ep_num]);
				if (ep)
					sp_udc_set_halt(&ep->ep, 0);
			}
		}

		value = 0;
		break;

	/* request set address */
	case USB_REQ_SET_ADDRESS:
		value = 0;
		break;
	}

	if (value >= 0) {
		req->length = value;
		req->context = udc;
		req->zero = value < w_length;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);

		if (value < 0) {
			DBG(cdev, "ep_queue --> %d\n", value);
			req->status = 0;
			hal_udc_setup_complete(gadget->ep0, req);
		}

		goto done;
	}

	UDC_LOGD("Unsupport request,return value %d\n", value);
	sp_udc_set_halt(&ep_0->ep, 1);

done:
	return value;
}

static void cfg_udc_ep(struct sp_udc *udc)
{
	struct udc_endpoint *udc_ep = NULL;
	int ep_id = 0;

	UDC_LOGD("+%s.%d\n", __func__, __LINE__);

	for (ep_id = 0; ep_id < UDC_MAX_ENDPOINT_NUM; ep_id++) {
		udc_ep = &udc->ep_data[ep_id];
		udc_ep->num = ep_id;
		udc_ep->ep.name = ep_info_dft[ep_id].name;
		udc_ep->ep.caps = ep_info_dft[ep_id].caps;
		udc_ep->ep.ops = &sp_ep_ops;
		udc_ep->dev = udc;
	}

	usb_ep_set_maxpacket_limit(&udc->ep_data[0].ep, EP_FIFO_SIZE);
	usb_ep_set_maxpacket_limit(&udc->ep_data[1].ep, ISO_MPS_SIZE);
	usb_ep_set_maxpacket_limit(&udc->ep_data[2].ep, ISO_MPS_SIZE);
	usb_ep_set_maxpacket_limit(&udc->ep_data[3].ep, ISO_MPS_SIZE);
	usb_ep_set_maxpacket_limit(&udc->ep_data[4].ep, ISO_MPS_SIZE);
	usb_ep_set_maxpacket_limit(&udc->ep_data[5].ep, ISO_MPS_SIZE);
	usb_ep_set_maxpacket_limit(&udc->ep_data[6].ep, ISO_MPS_SIZE);
	usb_ep_set_maxpacket_limit(&udc->ep_data[7].ep, ISO_MPS_SIZE);
	usb_ep_set_maxpacket_limit(&udc->ep_data[8].ep, ISO_MPS_SIZE);

	udc->ep_data[1].bEndpointAddress = USB_DIR_IN | EP1;
	udc->ep_data[1].bmAttributes = USB_ENDPOINT_XFER_BULK;

	udc->ep_data[2].bEndpointAddress = USB_DIR_IN | EP2;
	udc->ep_data[2].bmAttributes = USB_ENDPOINT_XFER_BULK;

	udc->ep_data[3].bEndpointAddress = USB_DIR_IN | EP3;
	udc->ep_data[3].bmAttributes = USB_ENDPOINT_XFER_INT;

	udc->ep_data[4].bEndpointAddress = USB_DIR_IN | EP4;
	udc->ep_data[4].bmAttributes = USB_ENDPOINT_XFER_ISOC;

	udc->ep_data[5].bEndpointAddress = USB_DIR_OUT | EP5;
	udc->ep_data[5].bmAttributes = USB_ENDPOINT_XFER_BULK;

	udc->ep_data[6].bEndpointAddress = USB_DIR_OUT | EP6;
	udc->ep_data[6].bmAttributes = USB_ENDPOINT_XFER_BULK;

	udc->ep_data[7].bEndpointAddress = USB_DIR_OUT | EP7;
	udc->ep_data[7].bmAttributes = USB_ENDPOINT_XFER_INT;

	udc->ep_data[8].bEndpointAddress = USB_DIR_OUT | EP8;
	udc->ep_data[8].bmAttributes = USB_ENDPOINT_XFER_ISOC;
}

static void sp_udc_ep_init(struct sp_udc *udc)
{
	struct udc_endpoint *ep;
	unsigned long flags;
	u32 i;

	UDC_LOGI("udc %d ep_init\n", udc->port_num);

	local_irq_save(flags);

	/* device/ep0 records init */
	INIT_LIST_HEAD(&udc->gadget.ep_list);
	INIT_LIST_HEAD(&udc->gadget.ep0->ep_list);

	for (i = 0; i < UDC_MAX_ENDPOINT_NUM; i++) {
		ep = &udc->ep_data[i];
		if (i != 0)
			list_add_tail (&ep->ep.ep_list, &udc->gadget.ep_list);

		ep->dev = udc;
		INIT_LIST_HEAD(&ep->queue);
	}
	local_irq_restore(flags);
}

static int sp_udc_ep_enable(struct usb_ep *_ep, const struct usb_endpoint_descriptor *desc)
{
	struct udc_endpoint *ep;
	struct sp_udc *udc;
	int max;

	ep = to_sp_ep(_ep);
	if (!_ep || !desc || desc->bDescriptorType != USB_DT_ENDPOINT) {
		UDC_LOGE("+%s.%d,EINVAL\n", __func__, __LINE__);
		return -EINVAL;
	}

	udc = ep->dev;
	if (!udc->driver) {
		UDC_LOGE("+%s.%d,%p,%x\n", __func__, __LINE__, udc->driver, udc->gadget.speed);
		return -ESHUTDOWN;
	}

	if (ep->ep_trb_ring_dq) {
		UDC_LOGE("ep%x already enable\n", ep->num);
		return -EINVAL;
	}

	max = usb_endpoint_maxp(desc) & 0x1fff;
	hal_udc_endpoint_configure(udc, desc->bEndpointAddress, desc->bmAttributes, max & 0x7ff);

	return 0;
}

static int sp_udc_ep_disable(struct usb_ep *_ep)
{
	struct udc_endpoint *ep = NULL;
	struct sp_udc *udc;

	UDC_LOGI("disable ep:%s\n", _ep->name);

	if (!_ep || !_ep->desc) {
		UDC_LOGE("+%s.%d,EINVAL\n", __func__, __LINE__);
		return -EINVAL;
	}

	ep = to_sp_ep(_ep);
	if (!ep) {
		UDC_LOGE("+%s.%d,EINVAL\n", __func__, __LINE__);
		return -EINVAL;
	}

	udc = ep->dev;
	sp_udc_nuke(udc, ep, -ESHUTDOWN);
	hal_udc_endpoint_unconfigure(udc, ep->bEndpointAddress);

	return 0;
}

static struct usb_request *sp_udc_alloc_request(struct usb_ep *_ep, gfp_t mem_flags)
{
	struct sp_request *req;

	if (!_ep) {
		UDC_LOGE("+%s.%d,_ep is null\n", __func__, __LINE__);
		return NULL;
	}

	req = kzalloc (sizeof(struct sp_request), mem_flags);
	if (!req) {
		UDC_LOGE("+%s.%d,req is null\n", __func__, __LINE__);
		return NULL;
	}

	req->req.dma = DMA_ADDR_INVALID;
	INIT_LIST_HEAD (&req->queue);

	return &req->req;
}

static void sp_udc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct sp_request *req = NULL;

	if (!_ep || !_req) {
		UDC_LOGE("+%s.%d,error\n", __func__, __LINE__);
		return;
	}

	req = to_sp_req(_req);
	kfree(req);
}

static int sp_udc_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags)
{
	struct sp_request *req = to_sp_req(_req);
	struct udc_endpoint *ep = to_sp_ep(_ep);
	struct sp_udc *udc;
	unsigned long flags;
	int32_t ret;

	if (unlikely (!_ep)) {
		UDC_LOGE("%s: invalid args\n", __func__);
		return -EINVAL;
	}

	if (likely (!req)) {
		UDC_LOGE("%s: req is null Why?\n", __func__);
		return -EINVAL;
	}

	udc = ep->dev;
	if (unlikely (!udc->driver)) {
		UDC_LOGE("+%s.%d,%p,%x\n", __func__, __LINE__, udc->driver, udc->gadget.speed);
		return -ESHUTDOWN;
	}

	if (unlikely(!_req || !_req->complete || !_req->buf)) {
		if (!_req)
			UDC_LOGE("%s: 1 X X X\n", __func__);
		else
			UDC_LOGE("%s: 0 %p %p\n", __func__, _req->complete, _req->buf);

		return -EINVAL;
	}

	_req->status = -EINPROGRESS;
	_req->actual = 0;

	if (_req->zero)
		UDC_LOGD("EP%d queue zero %d\n", ep->num, _req->zero);

	spin_lock_irqsave(&ep->lock, flags);
	if (!ep->ep_trb_ring_dq) {
		UDC_LOGE("ep%d is not configure why??\n", ep->num);

		spin_unlock_irqrestore(&ep->lock, flags);

		return -EINVAL;
	}

#if (TRANS_MODE == DMA_MAP)
	ret = usb_gadget_map_request(&udc->gadget, _req, EP_DIR(ep->bEndpointAddress));
	if (ret)
		return ret;
#endif

	list_add_tail(&req->queue, &ep->queue);

	ret = hal_udc_endpoint_transfer(udc, req, ep->bEndpointAddress, _req->buf, _req->dma,
										_req->length, _req->zero);
	UDC_LOGD("%s: ep:%x len %d, req:%px,trb:%px\n", __func__, ep->bEndpointAddress, _req->length,
										req, req->transfer_trb);
	if (ret) {
		UDC_LOGE("%s: transfer err\n", __func__);

#if (TRANS_MODE == DMA_MAP)
		usb_gadget_unmap_request(&udc->gadget, _req, EP_DIR(ep->bEndpointAddress));
#endif

		list_del(&req->queue);
		spin_unlock_irqrestore(&ep->lock, flags);

		return -EINVAL;
	}
	spin_unlock_irqrestore(&ep->lock, flags);

	return 0;
}

static int sp_udc_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct udc_endpoint *ep = to_sp_ep(_ep);
	struct sp_request *req = NULL;
	struct sp_udc *udc;
	int retval = -EINVAL;

	UDC_LOGD("%s(%px,%px)\n", __func__, _ep, _req);

	if (!_ep || !_req) {
		UDC_LOGE("+%s.%d,%px,%px\n", __func__, __LINE__, _ep, _req);
		return retval;
	}

	udc = ep->dev;
	if (!udc->driver) {
		UDC_LOGE("+%s.%d,dev->driver is null\n", __func__, __LINE__);
		return -ESHUTDOWN;
	}

	req = to_sp_req(_req);

	spin_lock(&ep->lock);
	if (!list_empty(&ep->queue)) {
		UDC_LOGD("dequeued req %px from %s, len %d buf %px,sp_req %px,%px\n",
					req, _ep->name, _req->length, _req->buf, req, req->transfer_trb);

#if (TRANS_MODE == DMA_MAP)
		usb_gadget_unmap_request(&udc->gadget, _req, ep->is_in);
#endif

		retval = 0;
		list_del(&req->queue);
		spin_unlock(&ep->lock);
		sp_udc_done(ep, req, -ECONNRESET);
	} else {
		spin_unlock(&ep->lock);
	}

	UDC_LOGD("dequeue done\n");

	return retval;
}

static int sp_udc_set_halt(struct usb_ep *_ep, int value)
{
	struct udc_endpoint *ep = to_sp_ep(_ep);
	struct sp_udc *udc;
	unsigned long flags;
	int retval;

	if (unlikely (!_ep || (!_ep->desc && _ep->name != ep0name))) {
		UDC_LOGE("%s: inval 2\n", __func__);
		return -EINVAL;
	}

	udc = ep->dev;
	UDC_LOGI("set EP%x halt value:%x\n", ep->num, value);
	local_irq_save (flags);
	retval = hal_udc_endpoint_stall(udc, ep->bEndpointAddress, value);
	local_irq_restore (flags);

	return retval;
}

/* gadget ops */
static struct usb_ep *sp_udc_match_ep(struct usb_gadget *_gadget,
					struct usb_endpoint_descriptor *desc,
						struct usb_ss_ep_comp_descriptor *ep_comp)
{
	struct usb_ep *ep = NULL;

	if (usb_endpoint_type(desc) == USB_ENDPOINT_XFER_INT) {
		gadget_for_each_ep(ep, _gadget) {
			usb_ep_set_maxpacket_limit(ep, INT_MPS_SIZE);

			if (ep->caps.type_int && usb_gadget_ep_match_desc(_gadget, ep, desc, ep_comp))
				return ep;

			usb_ep_set_maxpacket_limit(ep, ISO_MPS_SIZE);
		}
	}

	return NULL;
}

static int sp_udc_get_frame(struct usb_gadget *gadget)
{
	struct sp_udc *udc = to_sp_udc(gadget);
	volatile struct udc_reg *USBx = udc->reg;
	uint32_t frame_num;

	frame_num = USBx->USBD_FRNUM_ADDR & FRNUM;
	UDC_LOGD("+%s.%d,frame_num:%x\n", __func__, __LINE__, frame_num);

	return frame_num;
}

static int sp_udc_pullup(struct usb_gadget *gadget, int is_on)
{
	struct sp_udc *udc = to_sp_udc(gadget);

	UDC_LOGD("+%s.%d,is_on:%x\n", __func__, __LINE__, is_on);

	if (is_on) {
		if (udc->vbus_active && udc->driver)
			/* run controller pullup D+ */
			hal_udc_device_connect(udc);
		else
			return 1;
	} else {
		hal_udc_device_disconnect(udc);
	}

	return 0;
}

static int sp_udc_vbus_session(struct usb_gadget *gadget, int is_active)
{
	struct sp_udc *udc = to_sp_udc(gadget);

	udc->vbus_active = is_active;
	usb_udc_vbus_handler(gadget, udc->vbus_active);
	UDC_LOGI("sp_udc vbus SW %d\n", is_active);

	return 0;
}

static int sp_udc_start(struct usb_gadget *gadget, struct usb_gadget_driver *driver)
{
	struct sp_udc *udc = to_sp_udc(gadget);

	UDC_LOGI("%s start\n", __func__);

	/* Sanity checks */
	if (!udc) {
		UDC_LOGE("+%s.%d,dev is null\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (udc->driver) {
		UDC_LOGE("+%s.%d,busy\n", __func__, __LINE__);
		return -EBUSY;
	}

	/* Hook the driver */
	udc->driver = driver;

	/* initialize udc controller */
	hal_udc_init(udc);

	UDC_LOGI("+%s success\n", __func__);

	return 0;
}

static int sp_udc_stop(struct usb_gadget *gadget)
{
	struct sp_udc *udc = to_sp_udc(gadget);

	UDC_LOGI("+%s start\n", __func__);

	if (!udc) {
		UDC_LOGE("+%s.%d,dev is null\n", __func__, __LINE__);

		return -ENODEV;
	}

	udc->driver = NULL;

	/* disable interrupt */
	hal_udc_power_control(udc, UDC_POWER_OFF);

	/* free udc */
	hal_udc_deinit(udc);
	UDC_LOGI("+%s success\n", __func__);

	return 0;
}

void device_run_stop_ctrl(int enable)
{
	struct sp_udc *udc = sp_udc_arry[0];
	volatile struct udc_reg *USBx = udc->reg;

	/* driver unprobed or in suspend */
	if (!udc || (USBx->DEVC_ERSTSZ == 0x0))
		return;

	if (enable) {
		USBx->DEVC_ERSTSZ = udc->event_ring_seg_total;
		USBx->DEVC_CS |= UDC_RUN;
		USBx->EP0_CS |= EP_EN;
	} else {
		USBx->DEVC_CS &= ~UDC_RUN;
	}
}
EXPORT_SYMBOL(device_run_stop_ctrl);

void usb_switch(int device)
{
#if defined(CONFIG_USB_SUNPLUS_SP7350_OTG) && defined(CONFIG_SOC_SP7350)
	writel(MASK_MO1_USBC0_USB0_CTRL, moon4_reg + M4_SCFG_10);
#else
	if (device) {
	#ifdef CONFIG_SOC_Q645
		writel(MASK_MO1_USBC0_USB0_TYPE | MASK_MO1_USBC0_USB0_SEL |
					MASK_MO1_USBC0_USB0_CTRL | MO1_USBC0_USB0_CTRL,
							moon3_reg + M3_SCFG_22);
	#elif defined (CONFIG_SOC_SP7350)
		writel(MASK_MO1_USBC0_USB0_TYPE | MASK_MO1_USBC0_USB0_SEL |
					MASK_MO1_USBC0_USB0_CTRL | MO1_USBC0_USB0_CTRL,
							moon4_reg + M4_SCFG_10);
	#endif

		pwr_uphy_pll(0);
		device_run_stop_ctrl(1);
	} else {
	#ifdef CONFIG_SOC_Q645
		writel(MASK_MO1_USBC0_USB0_TYPE | MASK_MO1_USBC0_USB0_SEL | MASK_MO1_USBC0_USB0_CTRL |
					MO1_USBC0_USB0_TYPE | MO1_USBC0_USB0_SEL | MO1_USBC0_USB0_CTRL,
							moon3_reg + M3_SCFG_22);
	#elif defined(CONFIG_SOC_SP7350)
		writel(MASK_MO1_USBC0_USB0_TYPE | MASK_MO1_USBC0_USB0_SEL | MASK_MO1_USBC0_USB0_CTRL |
					MO1_USBC0_USB0_TYPE | MO1_USBC0_USB0_SEL | MO1_USBC0_USB0_CTRL,
							moon4_reg + M4_SCFG_10);
	#endif

		pwr_uphy_pll(1);
		device_run_stop_ctrl(0);
	}
#endif
}

static ssize_t udc_ctrl_show(struct device *dev, struct device_attribute *attr, char *buffer)
{
	return 0;
}

static ssize_t udc_ctrl_store(struct device *dev, struct device_attribute *attr,
							const char *buffer, size_t count)
{
	if (*buffer == 'd')		/* d:switch uphy to device */
		usb_switch(1);
	else if (*buffer == 'h')	/* h:switch uphy to host */
		usb_switch(0);

	return count;
}
static DEVICE_ATTR_RW(udc_ctrl);

static int sp_udc_probe(struct platform_device *pdev)
{
	struct resource *res_mem;
	struct sp_udc *udc = NULL;
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	struct usb_phy *otg_phy;
#endif
	resource_size_t rsrc_len;
	resource_size_t rsrc_start;
	int retval;
	typedef void (*pfunc)(unsigned long);

	udc = kzalloc(sizeof(*udc), GFP_KERNEL);
	if (!udc) {
		UDC_LOGE("%s.%d,malloc udc fail\n", __func__, __LINE__);
		return -ENOMEM;
	}

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	udc->otg_caps = kzalloc(sizeof(struct usb_otg_caps), GFP_KERNEL);
	if (!udc->otg_caps) {
		UDC_LOGE("%s.%d,malloc otg_caps fail\n", __func__, __LINE__);
		retval = -ENOMEM;
		goto err_free1;
	}
#endif

#if 0
	udc->irq_num = irq_of_parse_and_map(pdev->dev.of_node, 0);
#else
	udc->port_num = of_alias_get_id(pdev->dev.of_node, "usb-device");
	pr_info("%s start, port_num:%d, %px\n", __func__, udc->port_num, udc);

	/* phy */
	uphy[udc->port_num] = devm_phy_get(&pdev->dev, "uphy");
	if (IS_ERR(uphy[udc->port_num])) {
		dev_err(&pdev->dev, "no USB phy%d configured\n", udc->port_num);
		retval = PTR_ERR(uphy[udc->port_num]);
		goto err_free2;
	}

	/* reset */
	udc->rstc = devm_reset_control_get_shared(&pdev->dev, NULL);
	if (IS_ERR(udc->rstc)) {
		retval = PTR_ERR(udc->rstc);
		pr_err("UDC failed to retrieve reset controller: %d\n", retval);
		goto err_free2;
	}

	retval = reset_control_deassert(udc->rstc);
	if (retval)
		goto err_free2;

	/* clock */
	udc->clock = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(udc->clock)) {
		UDC_LOGE("not found clk source\n");
		retval = PTR_ERR(udc->clock);
		goto err_rst;
	}

	retval = phy_power_on(uphy[udc->port_num]);
	if (retval)
		goto err_rst;

	retval = phy_init(uphy[udc->port_num]);
	if (retval)
		goto err_rst;

	retval = clk_prepare_enable(udc->clock);
	if (retval)
		goto err_rst;

	udc->irq_num = platform_get_irq(pdev, 0);
#endif

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		UDC_LOGE("%s.%d,no memory recourse provided\n", __func__, __LINE__);
		if (udc)
			kfree(udc);

		retval = -ENXIO;
		goto err_clk;
	}

	rsrc_start = res_mem->start;
	rsrc_len = resource_size(res_mem);
	udc->reg = ioremap(rsrc_start, rsrc_len);
	if (!udc->reg) {
		UDC_LOGE("%s.%d,ioremap fail\n", __func__, __LINE__);
		if (udc)
			kfree(udc);

		retval = -ENOMEM;
		goto err_clk;
	}

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res_mem) {
#if defined (CONFIG_SOC_Q645)
		moon3_reg = devm_ioremap(&pdev->dev, res_mem->start, resource_size(res_mem));
		if (IS_ERR(moon3_reg)) {
#elif defined (CONFIG_SOC_SP7350)
		moon4_reg = devm_ioremap(&pdev->dev, res_mem->start, resource_size(res_mem));
		if (IS_ERR(moon4_reg)) {
#endif
			retval = -ENOMEM;
			goto err_map1;
		}
	}

	cfg_udc_ep(udc);
	udc->gadget.ops = &sp_ops;
	udc->gadget.ep0 = &(udc->ep_data[0].ep);
	udc->gadget.name = DRIVER_NAME;
	udc->gadget.max_speed = USB_SPEED_HIGH;
	udc->def_run_full_speed = false;	/* High Speed */


#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	udc->otg_caps->otg_rev = 0x0130;	/* OTG 1.3 */
	udc->otg_caps->hnp_support = true;
	udc->otg_caps->srp_support = true;
	udc->otg_caps->adp_support = false;
	udc->gadget.otg_caps = udc->otg_caps;

	#ifdef CONFIG_USB_OTG
	udc->gadget.is_otg = true;
	#endif
#endif

	sp_udc_ep_init(udc);

	retval = request_irq(udc->irq_num, sp_udc_irq,
					IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), udc);
	if (retval != 0) {
		UDC_LOGE("cannot request_irq %i, err %d\n", udc->irq_num, retval);
		retval = -EBUSY;
		goto err_map2;
	}

	spin_lock_init (&udc->lock);
	init_ep_spin(udc);
	platform_set_drvdata(pdev, udc);
	tasklet_init(&udc->event_task, (pfunc) handle_event, (unsigned long)udc);

	timer_setup(&udc->sof_polling_timer, udc_sof_polling, 0);
	udc->sof_polling_timer.expires = jiffies + HZ / 20;

	timer_setup(&udc->before_sof_polling_timer, udc_before_sof_polling, 0);
	udc->before_sof_polling_timer.expires = jiffies + 1 * HZ;

	udc->bus_reset_finish = false;
	udc->frame_num = 0;
	udc->vbus_active = true;
	udc->dev = &(pdev->dev);
	device_create_file(&pdev->dev, &dev_attr_udc_ctrl);
	device_create_file(&pdev->dev, &dev_attr_debug);

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	otg_phy = usb_get_transceiver_sp(udc->port_num);
	retval = otg_set_peripheral(otg_phy->otg, &udc->gadget);
	if (retval < 0)
		goto err_int;
#endif

	retval = usb_add_gadget_udc(&pdev->dev, &udc->gadget);
	if (retval) {
		UDC_LOGE("%s.%d,usb_add_gadget_udc fail\n", __func__, __LINE__);
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
		goto err_phy_init;
#else
		goto err_int;
#endif
	}

#if 0
	// "Null pointer" will happen due to lack of gadget->udc->driver in udc_gadget_disconnect()
	// There is still no gadget probed in this time
	// usb_udc_vbus_handler() : connect or disconnect gadget according to vbus status
	usb_udc_vbus_handler(&udc->gadget, udc->vbus_active);
#endif

#if 0
#ifndef CONFIG_SP_USB_PHY
	usb_controller_phy_init(udc->port_num); /* host do phy initialization */
#else
	phy = devm_usb_get_phy_by_phandle(&pdev->dev, "phys", 0);
	if (IS_ERR(phy)) {
		UDC_LOGE("get phy fail %ld\n", PTR_ERR(phy));
		goto err_phy_init;
	}

	if (usb_phy_init(phy)) {
		dev_err(&pdev->dev, "Failed to initialize phy\n");
		goto err_phy_init;
	}

	if (otg_set_peripheral(phy->otg, &udc->gadget)) {
		dev_err(&pdev->dev, "otg set peripheral err\n");
		usb_phy_shutdown(phy);
		goto err_phy_init;
	}
	udc->usb_phy = phy;
#endif
#endif

	sp_udc_arry[udc->port_num] = udc;

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	#ifdef CONFIG_USB_GADGET_PORT0_ENABLED
	if (sp_otg0_host) {
		sp_otg0_host->hnp_polling_timer = kthread_create(hnp_polling_watchdog,
								sp_otg0_host, "hnp_polling");
		wake_up_process(sp_otg0_host->hnp_polling_timer);
	}
	#else
	if (sp_otg1_host) {
		sp_otg1_host->hnp_polling_timer = kthread_create(hnp_polling_watchdog,
								sp_otg1_host, "hnp_polling");
		wake_up_process(sp_otg1_host->hnp_polling_timer);
	}
	#endif
#endif

	return 0;

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
err_phy_init:
	usb_put_phy(otg_phy);
#endif
err_int:
	free_irq(udc->irq_num, udc);
	udc->usb_phy = NULL;
err_map2:
#if defined (CONFIG_SOC_Q645)
	iounmap(moon3_reg);
#elif defined (CONFIG_SOC_SP7350)
	iounmap(moon4_reg);
#endif
err_map1:
	iounmap(udc->reg);
err_clk:
	clk_disable_unprepare(udc->clock);
err_rst:
	reset_control_assert(udc->rstc);
err_free2:
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	kfree(udc->otg_caps);
err_free1:
#endif
	kfree(udc);

	return retval;
}

static int sp_udc_remove(struct platform_device *pdev)
{
	struct sp_udc *udc = platform_get_drvdata(pdev);
#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	struct usb_phy *phy = NULL;
	int err;
#endif

	UDC_LOGD("%s start\n", __func__);

	device_remove_file(&pdev->dev, &dev_attr_udc_ctrl);
	device_remove_file(&pdev->dev, &dev_attr_debug);

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	#ifdef CONFIG_USB_GADGET_PORT0_ENABLED
	if (sp_otg0_host->hnp_polling_timer) {
		err = kthread_stop(sp_otg0_host->hnp_polling_timer);
		if (err)
			UDC_LOGW("kthread_stop failed: %d\n", err);
		else
			sp_otg0_host->hnp_polling_timer = NULL;
	}
	#else
	if (sp_otg1_host->hnp_polling_timer) {
		err = kthread_stop(sp_otg1_host->hnp_polling_timer);
		if (err)
			UDC_LOGW("kthread_stop failed: %d\n", err);
		else
			sp_otg1_host->hnp_polling_timer = NULL;
	}
	#endif
#endif

	if (udc->driver)
		device_run_stop_ctrl(0);

	clk_disable_unprepare(udc->clock);
	reset_control_assert(udc->rstc);

	phy_power_off(uphy[udc->port_num]);
	phy_exit(uphy[udc->port_num]);

	usb_del_gadget_udc(&udc->gadget);
	if (udc->driver) {
		UDC_LOGE("+%s.%d\n", __func__, __LINE__);
		return -EBUSY;
	}

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	if (udc->usb_phy) {
		phy = udc->usb_phy;
		otg_set_peripheral(phy->otg, NULL);
		usb_phy_shutdown(phy);
		usb_put_phy(phy);
		udc->usb_phy = NULL;
	}
#endif

	free_irq(udc->irq_num, udc);
	iounmap(udc->reg);
	platform_set_drvdata(pdev, NULL);
	sp_udc_arry[udc->port_num] = NULL;

	del_timer(&udc->before_sof_polling_timer);
	del_timer(&udc->sof_polling_timer);

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	kfree(udc->otg_caps);
#endif

	kfree(udc);

	UDC_LOGI("%s success\n", __func__);

	return 0;
}

#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
void detech_start(void)
{
	device_run_stop_ctrl(1);

	UDC_LOGD("%s...", __func__);
}
EXPORT_SYMBOL(detech_start);
#endif

#ifdef CONFIG_PM
static int udc_sunplus_drv_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sp_udc *udc = platform_get_drvdata(pdev);
	#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	int err = 0;

		#ifdef CONFIG_USB_GADGET_PORT0_ENABLED
	if (sp_otg0_host->hnp_polling_timer) {
		err = kthread_stop(sp_otg0_host->hnp_polling_timer);
		if (err)
			UDC_LOGW("kthread_stop failed: %d\n", err);
		else
			sp_otg0_host->hnp_polling_timer = NULL;
	}
		#else
	if (sp_otg1_host->hnp_polling_timer) {
		err = kthread_stop(sp_otg1_host->hnp_polling_timer);
		if (err)
			UDC_LOGW("kthread_stop failed: %d\n", err);
		else
			sp_otg1_host->hnp_polling_timer = NULL;
	}
		#endif
	#endif

	if (udc->driver) {
		hal_udc_sw_stop_handle(udc);
		device_run_stop_ctrl(0);
	}

	clk_disable_unprepare(udc->clock);
	reset_control_assert(udc->rstc);

	phy_power_off(uphy[udc->port_num]);
	phy_exit(uphy[udc->port_num]);

	return 0;
}

static int udc_sunplus_drv_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sp_udc *udc = platform_get_drvdata(pdev);
	int ret;

	ret = phy_power_on(uphy[udc->port_num]);
	if (ret)
		return ret;

	ret = phy_init(uphy[udc->port_num]);
	if (ret)
		return ret;

	ret = reset_control_deassert(udc->rstc);
	if (ret)
		return ret;

	ret = clk_prepare_enable(udc->clock);
	if (ret)
		return ret;

	if (udc->driver) {
		hal_udc_device_connect(udc);

	#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
		if (otg_id_pin == 1)
	#else
		#ifdef CONFIG_SOC_Q645
		if ((readl(moon3_reg + M3_SCFG_22) & (MO1_USBC0_USB0_SEL | MO1_USBC0_USB0_CTRL))
										== USB_DEVICE_MODE)
		#elif defined(CONFIG_SOC_SP7350)
		if ((readl(moon4_reg + M4_SCFG_10) & (MO1_USBC0_USB0_SEL | MO1_USBC0_USB0_CTRL))
										== USB_DEVICE_MODE)
		#endif
	#endif
		{
			device_run_stop_ctrl(1);
		}
	}

	#ifdef CONFIG_USB_SUNPLUS_SP7350_OTG
	sp_otg0_host->hnp_polling_timer = kthread_create(hnp_polling_watchdog,
								sp_otg0_host, "hnp_polling");
	wake_up_process(sp_otg0_host->hnp_polling_timer);
	#endif

	return 0;
}
#endif

static int __init udc_init(void)
{
	int retval;

	UDC_LOGI("+%s.%d\n", __func__, __LINE__);

	retval = platform_driver_register(&sp_udc_driver);
	if (retval)
		goto err;

	return 0;

err:
	UDC_LOGE("%s fail[%d]\n", __func__, retval);

	return retval;
}

static void __exit udc_exit(void)
{
	UDC_LOGI("+%s.%d\n", __func__, __LINE__);

	platform_driver_unregister(&sp_udc_driver);
}

module_init(udc_init);
module_exit(udc_exit);

MODULE_AUTHOR("Qiang.Deng");
MODULE_DESCRIPTION("Sunplus USB Device Controller Driver");
MODULE_VERSION("V1.0");
MODULE_LICENSE("GPL");
