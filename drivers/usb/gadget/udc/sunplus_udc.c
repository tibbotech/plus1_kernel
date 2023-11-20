// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/init.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/usb/gadget.h>
#include <mach/io_map.h>
#include <linux/usb/composite.h>
#include <uapi/linux/usb/cdc.h>
#include <linux/usb/otg.h>
#include <linux/usb/sp_usb.h>
#include <asm/cacheflush.h>
#include "../../../../arch/arm/mm/dma.h"
#ifdef CONFIG_USB_SUNPLUS_OTG
#include "../../phy/otg-sunplus.h"
#endif

#ifdef CONFIG_FIQ_GLUE
#include <asm/fiq.h>
#include <asm/fiq_glue.h>
#include <linux/pgtable.h>
#include <asm/hardware/gic.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#endif

#include <linux/clk.h>
#include <linux/vmalloc.h>

#include "sunplus_udc.h"
#include "sunplus_udc_regs.h"

#define TRANS_MODE				PIO_MODE
#define PIO_MODE				0
#define DMA_MODE_FOR_NCMH			1

//#define BULKIN_ENHANCED

#define IRQ_USB_DEV_PORT0			45
#define IRQ_USB_DEV_PORT1			48

#define TO_HOST					0
#define TO_DEVICE				1

/* ctrl rx singal keep the strongest time */
#define BUS_RESET_FOR_CHIRP_DELAY		2000
#define DMA_FLUSH_TIMEOUT_DELAY			300000

#define FULL_SPEED_DMA_SIZE			64
#define HIGH_SPEED_DMA_SIZE			512
#define UDC_FLASH_BUFFER_SIZE			(1024*64)
#define	DMA_ADDR_INVALID			(~(dma_addr_t)0)

#define ISO_DEBUG_INFO
#ifdef CONFIG_USB_GADGET_PORT0_ENABLED
#define USB0
#endif

void usb_switch(int device);
extern void sp_accept_b_hnp_en_feature(struct usb_otg *otg);

static u32 bulkep_dma_block_size;
static int gadget_num;

struct sp_ep_iso {
	struct list_head queue;
	int act;
};

static u32 dmsg = 1;
module_param(dmsg, uint, 0644);

/* coverity[declared_but_not_referenced] */
#ifdef CONFIG_USB_SUNPLUS_OTG
	#ifdef OTG_TEST
static u32 dev_otg_status = 1;
	#else
static u32 dev_otg_status;
	#endif
module_param(dev_otg_status, uint, 0644);
#endif

static void __iomem *base_addr[2];

static int dmesg_buf_size = 3000;
module_param(dmesg_buf_size, uint, 0644);
static int dqueue = 1;
module_param(dqueue, uint, 0644);
static char *sp_kbuf = NULL;

static spinlock_t plock;

static void dprintf(const char *fmt, ...)
{
	char buf[200];
	int len;
	int x;
	char *pBuf;
	char *pos;
	char org;
	static int i;
	static int bfirst = 1;
	va_list args;

	spin_lock(&plock);

	if (!sp_kbuf)
		sp_kbuf = kzalloc(dmesg_buf_size, GFP_KERNEL);

	va_start(args, fmt);
	vsnprintf(buf, 200, fmt, args);

	len = strlen(buf);
	if (len) {
		if (i+len <= dmesg_buf_size)
			memcpy(sp_kbuf+i, buf, len);
	}

	i += len;
	va_end(args);

	if (dqueue) {
		pr_info("%s\n", buf);
	} else if (i >= dmesg_buf_size && bfirst == 1) {
		bfirst = 0;
		i = 0;
		dmsg = 32;
		x = 0;

		pr_info(">>> start dump\n");
		pr_info("sp_kbuf: %d, i = %d\n", strlen(sp_kbuf), i);

		pBuf = sp_kbuf;
		while (1) {
			pos = strstr(pBuf, "\n");
			if (pos == NULL)
				break;

			org = *pos;
			*pos = 0;
			pr_info("%s", pBuf);
			pBuf = pos + 1;
		}

		pr_info(">>> end dump\n");
	}

	spin_unlock(&plock);
}

#define DEBUG_INFO(fmt, arg...)		do { if (dmsg&(1<<0)) dprintf(fmt, ##arg); } while (0)
#define DEBUG_NOTICE(fmt, arg...)	do { if (dmsg&(1<<1)) dprintf(fmt, ##arg); } while (0)
#define DEBUG_DBG(fmt, arg...)		do { if (dmsg&(1<<2)) dprintf(fmt, ##arg); } while (0)
#define DEBUG_DUMP(fmt, arg...)		do { if (dmsg&(1<<3)) dprintf(fmt, ##arg); } while (0)
#define DEBUG_PRT(fmt, arg...)		do { if (dmsg&(1<<4)) dprintf(fmt, ##arg); } while (0)

static inline u32 udc_read(u32 reg, void __iomem *base_addr);
static inline void udc_write(u32 value, u32 reg, void __iomem *base_addr);
static inline u32 udc0_read(u32 reg);
static inline void udc0_write(u32 value, u32 reg);
static inline u32 udc1_read(u32 reg);
static inline void udc1_write(u32 value, u32 reg);
static int udc_init_c(void __iomem *baseaddr);

static void sp_print_hex_dump_bytes(const char *prefix_str, int prefix_type,
							const void *buf, size_t len)
{
	if (dmsg & (1 << 3))
		print_hex_dump(KERN_NOTICE, prefix_str, prefix_type, 16, 1,
				buf, len, true);
}

static ssize_t udc_ctrl_show(struct device *dev, struct device_attribute *attr, char *buffer)
{
	return true;
}

static ssize_t udc_ctrl_store(struct device *dev, struct device_attribute *attr,
							const char *buffer, size_t count)
{
	struct platform_device *pdev;
	struct sp_udc *udc = NULL;
	void __iomem *baddr = NULL;
	u32 ret = 0;

	pdev = container_of(dev, struct platform_device, dev);
	udc = platform_get_drvdata(pdev);
	baddr = udc->base_addr;
		
	ret = udc_read(UDLCSET, baddr) & SIM_MODE;
	if (*buffer == 'd') {			/* d:switch uphy to device */
		pr_info("user switch to device");
		usb_switch(TO_DEVICE);
		msleep(2);
		udc_init_c(baddr);
		return count;
	} else if (*buffer == 'h') {		/* h:switch uphy to host */
		pr_info("user switch to host");
		usb_switch(TO_HOST);
		return count;
	} else if (*buffer == '1') {		/* support SET_DESC COMMND */
		ret |= SUPP_SETDESC;
	} else if (*buffer == 'n') {		/* ping pong buffer not auto switch */
		udc_write(udc_read(UDEPBPPC, baddr) | SOFT_DISC, UDEPBPPC, baddr);
	} else if (*buffer == 'o') {		/* ping pong buffer auto switch */
		udc_write(udc_read(UDNBIE, baddr) | EP11O_IF, UDNBIE, baddr);
		udc_write(udc_read(UDEPBPPC, baddr) & 0XFE, UDEPBPPC, baddr);
	} else {
		ret &= ~SUPP_SETDESC;
	}

	udc_write(ret, UDLCSET, baddr);

	return count;
}

static inline struct sp_ep *to_sp_ep(struct usb_ep *ep)
{
	return container_of(ep, struct sp_ep, ep);
}

static inline struct sp_udc *to_sp_udc(struct usb_gadget *gadget)
{
	return container_of(gadget, struct sp_udc, gadget);
}

static inline struct sp_request *to_sp_req(struct usb_request *req)
{
	return container_of(req, struct sp_request, req);
}

static inline u32 udc_read(u32 reg, void __iomem *base_addr)
{
	return readl(base_addr + reg);
}

static inline void udc_write(u32 value, u32 reg, void __iomem *base_addr)
{
	writel(value, base_addr + reg);
}

static inline u32 udc0_read(u32 reg)
{
	return readl(base_addr[0] + reg);
}

static inline void udc0_write(u32 value, u32 reg)
{
	writel(value, base_addr[0] + reg);
}

static inline u32 udc1_read(u32 reg)
{
	return readl(base_addr[1] + reg);
}

static inline void udc1_write(u32 value, u32 reg)
{
	writel(value, base_addr[1] + reg);
}

void init_ep_spin(struct sp_udc *udc)
{
	int i;

	for (i = 0; i < SP_MAXENDPOINTS; i++)
		spin_lock_init(&udc->ep[i].lock);
}

static inline int sp_udc_get_ep_fifo_count(struct sp_udc *udc,  int is_pingbuf, u32 ping_c)
{
	int tmp = 0;

	if (is_pingbuf)
		tmp = udc->reg_read(ping_c);
	else
		tmp = udc->reg_read(ping_c + 0x4);

	return (tmp & 0x3ff);
}

static void sp_udc_done(struct sp_ep *ep, struct sp_request *req, int status)
{
	unsigned int halted = ep->halted;

	DEBUG_DBG(">>> %s...", __func__);

	list_del_init(&req->queue);
	if (likely(req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	ep->halted = 1;
	usb_gadget_giveback_request(&ep->ep, &req->req);
	ep->halted = halted;

	DEBUG_DBG("<<< %s...", __func__);
}

static void sp_udc_nuke(struct sp_ep *ep, int status)
{
	/* Sanity check */
	DEBUG_DBG(">>> %s...", __func__);
	if (&ep->queue == NULL)
		return;

	while (!list_empty(&ep->queue)) {
		struct sp_request *req;

		req = list_entry(ep->queue.next, struct sp_request, queue);
		sp_udc_done(ep, req, status);
	}

	DEBUG_DBG("<<< %s...", __func__);
}

static int sp_udc_set_halt(struct usb_ep *_ep, int value)
{
	struct sp_ep *ep = to_sp_ep(_ep);
	struct sp_udc *udc = ep->udc;
	unsigned long flags;
	u32 idx;
	u32 v = 0;

	if (unlikely(!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		pr_err("%s: inval 2\n", __func__);

		return -EINVAL;
	}

	DEBUG_DBG(">>> %s...", __func__);

	local_irq_save(flags);

	idx = ep->bEndpointAddress & 0x7f;
	DEBUG_DBG("udc set halt ep=%x val=%x", idx, value);

	switch (idx) {
	case EP1:
		v = SETEP1STL;
		break;

	case EP2:
		v = SETEP2STL;
		break;

	case EP3:
		v = SETEP3STL;
		break;

	case EP4:
		v = SETEP4STL;
		break;

	case EP6:
		v = SETEP6STL;
		break;

	case EP8:
		v = SETEP8STL;
		break;

	case EP9:
		v = SETEP8STL;
		break;

	case EP10:
		v = SETEPASTL;
		break;

	case EP11:
		v = SETEPBSTL;
		break;

	default:
		return -EINVAL;
	}

	if ((!value) && v)
		v = v << 16;

	udc->reg_write((udc->reg_read(UDLCSTL) | v), UDLCSTL);
	DEBUG_DBG("udc set halt v=%xh, UDLCSTL=%x", v, udc->reg_read(UDLCSTL));
	ep->halted = value ? 1 : 0;

	local_irq_restore(flags);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_ep_enable(struct usb_ep *_ep, const struct usb_endpoint_descriptor *desc)
{
	struct sp_udc *udc;
	struct sp_ep *ep;
	unsigned long flags;
	u32 linker_int_en;
	u32 bulk_int_en;
	u32 max;

	DEBUG_DBG(">>> %s", __func__);

	ep = to_sp_ep(_ep);

	if (!_ep || !desc || _ep->name == ep0name || desc->bDescriptorType != USB_DT_ENDPOINT) {
		pr_err("%s.%d,,%p,%p,%p,%s,%d\n", __func__, __LINE__,
				_ep, desc, ep->desc, _ep->name, desc->bDescriptorType);

		return -EINVAL;
	}

	udc = ep->udc;
	DEBUG_DBG("udc->driver = %xh, udc->gadget.speed = %d", udc->driver, udc->gadget.speed);
	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN)
		return -ESHUTDOWN;

	max = le16_to_cpu(desc->wMaxPacketSize) & 0x1fff;

	local_irq_save(flags);

	_ep->maxpacket = max & 0x7ff;
	ep->desc = desc;
	ep->halted = 0;

	ep->bEndpointAddress = desc->bEndpointAddress;

	linker_int_en   = udc->reg_read(UDLIE);
	bulk_int_en     = udc->reg_read(UDNBIE);

	DEBUG_DBG("ep->num = %d", ep->num);

	switch (ep->num) {
	case EP0:
		linker_int_en |= EP0S_IF;
		break;

	case EP1:
		linker_int_en |= EP1I_IF;
		udc->reg_write(EP_DIR | EP_ENA | RESET_PIPO_FIFO, UDEP12C);
		break;

	case EP2:
		bulk_int_en |= EP2O_IF;
		udc->reg_write(EP_ENA | RESET_PIPO_FIFO, UDEP12C);
		break;

	case EP3:
		linker_int_en |= EP3I_IF;
		break;
		
	case EP4:
		linker_int_en |= EP4I_IF;
		break;
		
	case EP6:
		linker_int_en |= EP6I_IF;
		break;
		
	case EP8:
		linker_int_en |= EP8I_IF;
		udc->reg_write(EP_DIR | EP_ENA | RESET_PIPO_FIFO, UDEP89C);
		break;

	case EP11:
		bulk_int_en |= EP11O_IF;
		udc->reg_write(EP_ENA | RESET_PIPO_FIFO, UDEPABC);
		break;

	default:
		return -EINVAL;
	}

	udc->reg_write(linker_int_en, UDLIE);
	udc->reg_write(bulk_int_en, UDNBIE);

	/* print some debug message */
	DEBUG_DBG("enable %s(%d) ep%x%s-blk max %02x", _ep->name, ep->num, desc->bEndpointAddress,
					desc->bEndpointAddress & USB_DIR_IN ? "in" : "out", max);

	local_irq_restore(flags);
	sp_udc_set_halt(_ep, 0);
	DEBUG_DBG("<<< %s", __func__);

	return 0;
}

static int sp_udc_ep_disable(struct usb_ep *_ep)
{
	struct sp_ep *ep = to_sp_ep(_ep);
	struct sp_udc *udc = ep->udc;
	unsigned long flags;
	u32 int_udll_ie;
	u32 int_udn_ie;

	DEBUG_DBG(">>> %s...", __func__);

	if (!_ep || !ep->desc) {
		DEBUG_DBG("%s not enabled", _ep ? ep->ep.name : NULL);
		return -EINVAL;
	}

	DEBUG_DBG("ep_disable: %s", _ep->name);

	local_irq_save(flags);

	ep->desc = NULL;
	ep->halted = 1;

	sp_udc_nuke(ep, -ESHUTDOWN);

	/* disable irqs */
	int_udll_ie = udc->reg_read(UDLIE);
	int_udn_ie = udc->reg_read(UDNBIE);

	switch (ep->num) {
	case EP0:
		int_udll_ie &= ~(EP0I_IF | EP0O_IF | EP0S_IF);
		break;

	case EP1:
		int_udll_ie &= ~(EP1I_IF);

		if (udc->reg_read(UEP12DMACS) & DMA_EN) {
			udc->dma_len_ep1 = 0;
			udc->reg_write(udc->reg_read(UEP12DMACS) | DMA_FLUSH, UEP12DMACS);

			while (!(udc->reg_read(UEP12DMACS) & DMA_FLUSHEND))
				DEBUG_DBG("wait dma 1 flush");
		}

		udc->dma_len_ep1 = 0;
		break;

	case EP8:
		int_udll_ie &= ~(EP8I_IF);
		break;

	case EP3:
		int_udll_ie &= ~(EP3I_IF);
		break;

	case EP4:
		int_udll_ie &= ~(EP4I_IF);
		break;

	case EP2:
		int_udn_ie &= ~(EP2O_IF);
		break;

	case EP11:
		int_udn_ie &= ~(EP11O_IF);

		if (udc->reg_read(UDEPBDMACS) & DMA_EN) {
			udc->reg_write(udc->reg_read(UDEPBDMACS) | DMA_FLUSH, UDEPBDMACS);

			while (!(udc->reg_read(UDEPBDMACS) & DMA_FLUSHEND))
				DEBUG_DBG("wait dma 11 flush");
		}

		udc->dma_xferlen_ep11 = 0;
		break;

	default:
		return -EINVAL;
	}

	udc->reg_write(int_udll_ie, UDLIE);
	udc->reg_write(int_udn_ie, UDNBIE);

	local_irq_restore(flags);

	DEBUG_DBG("%s disabled", _ep->name);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static struct usb_request *sp_udc_alloc_request(struct usb_ep *_ep, gfp_t mem_flags)
{
	struct sp_request *req;

	DEBUG_DBG(">>> %s...", __func__);

	if (!_ep)
		return NULL;

	req = kzalloc(sizeof(struct sp_request), mem_flags);
	if (!req)
		return NULL;

	req->req.dma = DMA_ADDR_INVALID;
	INIT_LIST_HEAD(&req->queue);
	DEBUG_DBG("<<< %s...", __func__);

	return &req->req;
}

static void sp_udc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct sp_request *req = to_sp_req(_req);
	struct sp_ep *ep = to_sp_ep(_ep);

	DEBUG_DBG("%s free rquest", _ep->name);

	if (!ep || !_req || (!ep->desc && _ep->name != ep0name))
		return;

	WARN_ON(!list_empty(&req->queue));
	kfree(req);
}

static void clearHwState_UDC(struct sp_udc *udc)
{
	/* INFO: we don't disable udc interrupt when we are clear udc hw state,			*/
	/* 1.since when we are clearing, we are in ISR , will not the same interrupt reentry	*/
	/*   problem.										*/
	/* 2.after we finish clearing , we will go into udc ISR again, if there are interrupts	*/
	/*   occur while we are clearing ,we want to catch them immediately			*/
	/*											*/

	/* ===== check udc DMA state, and flush it ===== */
	int tmp = 0;

	if (udc->reg_read(UDEP2DMACS) & DMA_EN) {
		udc->reg_write(udc->reg_read(UDEP2DMACS) | DMA_FLUSH, UDEP2DMACS);
		while (!(udc->reg_read(UDEP2DMACS) & DMA_FLUSHEND)) {
			tmp++;
			if (tmp > DMA_FLUSH_TIMEOUT_DELAY) {
				DEBUG_DBG("##");
				tmp = 0;
			}
		}
	}

	/* Disable Interrupt */
	/* Clear linker layer Interrupt source */
	udc->reg_write(0xefffffff, UDLIF);
	/* EP0 control status */
	/* clear ep0 out vld = 1, clear set ep0 in vld = 0, set ctl dir to OUT direction = 0 */
	udc->reg_write(CLR_EP0_OUT_VLD, UDEP0CS);
	udc->reg_write(0x0, UDEP0CS);

	udc->reg_write(CLR_EP_OVLD | RESET_PIPO_FIFO, UDEP12C);

	/* Clear Stall Status */
	udc->reg_write((CLREPBSTL | CLREPASTL | CLREP9STL | CLREP8STL | CLREP3STL | CLREP2STL
							| CLREP1STL | CLREP0STL), UDLCSTL);
}

static int vbusInt_handle(struct sp_udc *udc)
{
	DEBUG_DBG(">>> %s... UDCCS=%xh, UDLCSET=%xh",
				__func__, udc->reg_read(UDCCS), udc->reg_read(UDLCSET));

	/* host present */
	if (udc->reg_read(UDCCS) & VBUS) {
		/* if soft discconect ->force connect */
		if (udc->reg_read(UDLCSET) & SOFT_DISC)
			udc->reg_write(udc->reg_read(UDLCSET) & 0xfe, UDLCSET);
	} else {/* host absent */
		/* soft connect ->force disconnect */
		if (!(udc->reg_read(UDLCSET) & SOFT_DISC))
			udc->reg_write(udc->reg_read(UDLCSET) | SOFT_DISC, UDLCSET);
	}

	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_readep0_fifo_crq(struct sp_udc *udc, struct usb_ctrlrequest *crq)
{
	unsigned char *outbuf = (unsigned char *)crq;

	DEBUG_DBG("read ep0 fifi crq ,len= %d", udc->reg_read(UDEP0DC));
	memcpy((unsigned char *)outbuf, (char *)(UDEP0SDP + udc->base_addr), 4);
	mb();		// make sure settings are effective.
	memcpy((unsigned char *)(outbuf + 4), (char *)(UDEP0SDP + udc->base_addr), 4);

	return 8;
}

static int sp_udc_get_status(struct sp_udc *udc, struct usb_ctrlrequest *crq)
{
	u8 ep_num = crq->wIndex & 0x7f;
	struct sp_ep *ep = &udc->ep[ep_num];
	u32 status = 0;

	switch (crq->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_INTERFACE:
		break;

	case USB_RECIP_DEVICE:
		status = udc->devstatus;
		break;

	case USB_RECIP_ENDPOINT:
		if (ep_num > 14 || crq->wLength > 2)
			return 1;

		status = ep->halted;
		break;

	default:
		return 1;
	}

	udc->reg_write(EP0_DIR | CLR_EP0_OUT_VLD, UDEP0CS);
	udc->reg_write(((1 << 2) - 1), UDEP0VB);
	memcpy((char *)(udc->base_addr + UDEP0DP), (char *)(&status), 4);
	udc->reg_write(udc->reg_read(UDLIE) | EP0I_IF, UDLIE);
	udc->reg_write(EP0_DIR | SET_EP0_IN_VLD, UDEP0CS);

	return 0;
}

static void sp_udc_handle_ep0_idle(struct sp_udc *udc, struct sp_ep *ep,
						struct usb_ctrlrequest *crq, u32 ep0csr, int cf)
{
#ifdef CONFIG_USB_SUNPLUS_OTG
	struct usb_phy *otg_phy;
#endif
	int ret, state;
	int len;

	DEBUG_DBG(">>> %s...", __func__);

	/* start control request? */
	sp_udc_nuke(ep, -EPROTO);

	len = sp_udc_readep0_fifo_crq(udc, crq);
	DEBUG_DBG("len  = %d", len);

	if (len != sizeof(*crq)) {
		pr_err("setup begin: fifo READ ERROR wanted %d bytes got %d. Stalling out...",
										sizeof(*crq), len);

		/* error send stall */
		udc->reg_write((udc->reg_read(UDLCSTL) | SETEP0STL), UDLCSTL);

		return;
	}

	DEBUG_DBG("bRequestType = %02xh, bRequest = %02xh, wValue = %04xh, wIndex = %04xh,\t"
			"\b\b\b\b\bwLength = %04xh",
			crq->bRequestType, crq->bRequest, crq->wValue, crq->wIndex, crq->wLength);

	/****************************************/
	/* bRequestType				*/
	/* Bit 7 : data transfer direction	*/
	/* 0b = Host-to-device			*/
	/* 1b = Device-to-host			*/
	/* Bit 6 ...				*/
	/* Bit 5 : type				*/
	/* 00b = Standard			*/
	/* 01b = Class				*/
	/* 10b = Vendor				*/
	/* 11b = Reserved			*/
	/* Bit 4 ...				*/
	/* Bit 0 : recipient			*/
	/* 00000b = Device			*/
	/* 00001b = Interface			*/
	/* 00010b = Endpoint			*/
	/* 00011b = Other			*/
	/* 00100b to 11111b = Reserved		*/
	/****************************************/

	/****************************************/
	/* wValue, high-byte			*/
	/* 1 = Device				*/
	/* 2 = Configuration			*/
	/* 3 = String				*/
	/* 4 = Interface			*/
	/* 5 = Endpoint				*/
	/* 6 = Device Qualifier			*/
	/* 7 = Other Speed Configuration	*/
	/* 8 - Interface Power			*/
	/* 9 - On-The-Go (OTG)			*/
	/* 21 = HID (Human interface device)	*/
	/* 22 = Report				*/
	/* 23 = Physical			*/
	/* **************************************/

	/* cope with automagic for some standard requests. */
	udc->req_std = (crq->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD;
	udc->req_config = 0;
	udc->req_pending = 1;
	state = udc->gadget.state;

	switch (crq->bRequest) {
	case USB_REQ_GET_DESCRIPTOR:
		DEBUG_DBG(" ******* USB_REQ_GET_DESCRIPTOR ... ");
		DEBUG_DBG("start get descriptor after bus reset");
		{
			u32 DescType;
			udelay(200);
			DescType = ((crq->wValue) >> 8);

			if (DescType == 0x1) {
				if (udc->reg_read(UDLCSET) & CURR_SPEED) {
					DEBUG_DBG("DESCRIPTOR SPeed = USB_SPEED_FULL");
					udc->gadget.speed = USB_SPEED_FULL;
					bulkep_dma_block_size = FULL_SPEED_DMA_SIZE;
				} else {
					DEBUG_DBG("DESCRIPTOR SPeed = USB_SPEED_HIGH");
					udc->gadget.speed = USB_SPEED_HIGH;
					bulkep_dma_block_size = HIGH_SPEED_DMA_SIZE;
				}
			}
		}
		break;

	case USB_REQ_SET_CONFIGURATION:
		DEBUG_DBG(" ******* USB_REQ_SET_CONFIGURATION ...");
		DEBUG_DBG("udc->gadget.state = %d", udc->gadget.state);
		udc->req_config = 1;
		udc->reg_write(udc->reg_read(UDLCADDR) | (crq->wValue) << 16, UDLCADDR);
		break;

	case USB_REQ_SET_INTERFACE:
		DEBUG_DBG("***** USB_REQ_SET_INTERFACE ****");
		udc->req_config = 1;
		break;

	case USB_REQ_SET_ADDRESS:
		DEBUG_DBG("USB_REQ_SET_ADDRESS ...");
		return;

	case USB_REQ_GET_STATUS:
		DEBUG_DBG("USB_REQ_GET_STATUS ...");
		udelay(200);

#ifdef CONFIG_USB_SUNPLUS_OTG
		if ((crq->bRequestType == 0x80) && (crq->wValue == 0) && (crq->wIndex == 0xf000)
									&& (crq->wLength == 4)) {
			u32 status = 0;

			status = dev_otg_status;
			udc->reg_write(EP0_DIR | CLR_EP0_OUT_VLD, UDEP0CS);
			udc->reg_write(((1 << 2) - 1), UDEP0VB);
			DEBUG_DBG("get otg status, %d, %d", dev_otg_status, status);
			memcpy((char *)(udc->base_addr + UDEP0DP), (char *)(&status), 4);
			udc->reg_write(udc->reg_read(UDLIE) | EP0I_IF, UDLIE);
			udc->reg_write(SET_EP0_IN_VLD | EP0_DIR, UDEP0CS);
			return;
		}
#endif

		if (udc->req_std) {
			if (!sp_udc_get_status(udc, crq))
				return;
		}
		break;

	case USB_REQ_CLEAR_FEATURE:
		DEBUG_DBG(">>> USB_REQ_CLEAR_FEATURE ...");
		break;

	case USB_REQ_SET_FEATURE:
#ifdef CONFIG_USB_SUNPLUS_OTG
		if ((crq->bRequestType == 0) && (crq->wValue == 3) && (crq->wIndex == 0)
									&& (crq->wLength == 0)) {
			DEBUG_DBG("set hnp feature");

	#ifdef CONFIG_USB_GADGET_PORT0_ENABLED
			otg_phy = usb_get_transceiver_sp(0);
	#else
			otg_phy = usb_get_transceiver_sp(1);
	#endif

			if (!otg_phy)
				DEBUG_DBG("Get otg control fail");
			else
				sp_accept_b_hnp_en_feature(otg_phy->otg);

		}
#endif

		DEBUG_DBG("USB_REQ_SET_FEATURE ...");
		break;

	default:
		break;
	}

	if (crq->bRequestType & USB_DIR_IN) {
		udc->ep0state = EP0_IN_DATA_PHASE;
	} else {
		udc->ep0state = EP0_OUT_DATA_PHASE;
		DEBUG_NOTICE("ep0 fifo %x\n", udc->reg_read(UDEP0CS));
		udc->reg_write(0, UDEP0CS);
	}

	if (!udc->driver)
		return;

	/* deliver the request to the gadget driver */
	/* android_setup composite_setup */
	ret = udc->driver->setup(&udc->gadget, crq);
	DEBUG_DBG("udc->driver->setup = %x", ret);

	if (ret < 0) {
		if (udc->req_config) {
			pr_err("config change %02x fail %d?", crq->bRequest, ret);
			return;
		}

		if (ret == -EOPNOTSUPP)
			pr_err("Operation not supported");
		else
			pr_err("udc->driver->setup failed. (%d)", ret);

		udelay(5);

		/* set ep0 stall */
		udc->reg_write(0x1, UDLCSTL);
		udc->ep0state = EP0_IDLE;
	} else if (udc->req_pending) {
		DEBUG_DBG("udc->req_pending... what now?");
		udc->req_pending = 0;

		/* MSC reset command */
		if (crq->bRequest == 0xff) {
			/* ep1SetHalt = 0; */
			/* ep2SetHalt = 0; */
		}
	}

	DEBUG_DBG("ep0state *** %s, Request=%d, RequestType=%d, from=%d",
				ep0states[udc->ep0state], crq->bRequest, crq->bRequestType, cf);
	DEBUG_DBG("<<< %s...", __func__);
}

static inline int sp_udc_write_packet_with_buf(struct sp_udc *udc, int fifo, u8 *buf, unsigned int len, int reg)
{
	int n = 0;
	int m = 0;
	int i = 0;
	u32 *pbuf = (u32*)buf;

	n = len / 4;
	m = len % 4;

	if (n > 0) {
		udc->reg_write(0xf, reg);

		for (i = 0; i < n; i++) {
			udc->reg_write(*pbuf, fifo);
			pbuf++;
		}
	}

	if (m > 0) {
		udc->reg_write(((1 << m) - 1), reg);
		udc->reg_write(*pbuf, fifo);
	}

	return len;
}

static inline int sp_udc_write_packet(struct sp_udc *udc, int fifo, struct sp_request *req, unsigned int max,
										int offset)
{
	unsigned int len = min(req->req.length - req->req.actual, max);
	u8 *buf = req->req.buf + req->req.actual;

	prefetch(buf);

	return sp_udc_write_packet_with_buf(udc, fifo, buf, len, offset);
}

static int sp_udc_write_ep0_fifo(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	unsigned int count;
	int is_last;

	udc->reg_write(EP0_DIR | CLR_EP0_OUT_VLD, UDEP0CS);
	count = sp_udc_write_packet(udc, UDEP0DP, req, ep->ep.maxpacket, UDEP0VB);
	udc->reg_write(EP0_DIR | SET_EP0_IN_VLD, UDEP0CS);
	req->req.actual += count;

	if (count != ep->ep.maxpacket)
		is_last = 1;
	else if (req->req.length != req->req.actual || req->req.zero)
		is_last = 0;
	else
		is_last = 1;

	DEBUG_DBG("write ep0: count=%d, actual=%d, length=%d, last=%d, zero=%d",
			count, req->req.actual, req->req.length, is_last, req->req.zero);

	if (is_last) {
		int cc = 0;

		while (udc->reg_read(UDEP0CS) & EP0_IVLD) {
			udelay(5);

			if (cc++ > 1000) {
				pr_err("Wait write ep0 timeout!.");
				break;
			}
		}

		sp_udc_done(ep, req, 0);
		udc->reg_write(udc->reg_read(UDLIE) & (~EP0I_IF), UDLIE);
		udc->reg_write(udc->reg_read(UDEP0CS) & (~EP0_DIR), UDEP0CS);
	} else {
		udc->reg_write(udc->reg_read(UDLIE) | EP0I_IF, UDLIE);
	}

	return is_last;
}

static inline int sp_udc_read_fifo_packet(struct sp_udc *udc, int fifo, u8 *buf, int length, int reg)
{
	u32 *pbuf = (u32 *)buf;
	int n = 0;
	int m = 0;
	int i = 0;

	n = length / 4;
	m = length % 4;

	udc->reg_write(0xf, reg);
	for (i = 0; i < n; i++) {
		*pbuf = udc->reg_read(fifo);
		pbuf++;
	}

	if (m > 0) {
		udc->reg_write(((1 << m) - 1), reg);
		*pbuf = udc->reg_read(fifo);
	}

	return length;
}

static int sp_udc_read_ep0_fifo(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	unsigned int count;
	int is_last;
	u8 ep0_len;
	u8 *buf;

	if (!req->req.length) {
		DEBUG_DBG("%s: length = 0", __func__);
		// udc->reg_write(udc->reg_read(UDLIE) | EP0I_IF, UDLIE);
		// udc->reg_write(SET_EP0_IN_VLD | EP0_DIR, UDEP0CS);
		// udc->reg_write(udc->reg_read(UDLIE) & (~EP0O_IF), UDLIE);
		// sp_udc_done(ep, req, 0);
		is_last = 1;
	} else {
		udc->reg_write(udc->reg_read(UDLIE) | EP0O_IF, UDLIE);

		buf = req->req.buf + req->req.actual;
		udc->reg_write(udc->reg_read(UDEP0CS) & (~EP0_DIR), UDEP0CS);	/* read direction */

		ep0_len = udc->reg_read(UDEP0DC);

		if (ep0_len > ep->ep.maxpacket)
			ep0_len = ep->ep.maxpacket;

		count = sp_udc_read_fifo_packet(udc, UDEP0DP, buf, ep0_len, UDEP0VB);
		udc->reg_write(udc->reg_read(UDEP0CS) | CLR_EP0_OUT_VLD, UDEP0CS);

		req->req.actual += count;

		if (count != ep->ep.maxpacket)
			is_last = 1;
		else if (req->req.length != req->req.actual || req->req.zero)
			is_last = 0;
		else
			is_last = 1;

		DEBUG_DBG("read ep0: count=%d, actual=%d, length=%d, maxpacket=%d, last=%d,\t"
					"\b\b\b\b\bzero=%d", count, req->req.actual, req->req.length,
						ep->ep.maxpacket, is_last, req->req.zero);
	}

	if (is_last) {
		DEBUG_DBG("%s: is_last = 1", __func__);
		udc->reg_write(udc->reg_read(UDLIE) | EP0I_IF, UDLIE);
		udc->reg_write(SET_EP0_IN_VLD | EP0_DIR, UDEP0CS);
		udc->reg_write(udc->reg_read(UDLIE) & (~EP0O_IF), UDLIE);
		sp_udc_done(ep, req, 0);
	}

	return is_last;
}

static int sp_udc_handle_ep0_proc(struct sp_ep *ep, struct sp_request *req, int cf)
{
	struct sp_udc *udc = ep->udc;
	struct usb_ctrlrequest crq;
	int bOk = 1;
	u32 ep0csr;

	ep0csr = udc->reg_read(UDEP0CS);

	switch (udc->ep0state) {
	case EP0_IDLE:
		DEBUG_DBG("EP0_IDLE_PHASE ... what now?");
		sp_udc_handle_ep0_idle(udc, ep, &crq, ep0csr, cf);
		break;

	case EP0_IN_DATA_PHASE:
		DEBUG_DBG("EP0_IN_DATA_PHASE ... what now?");
		if (sp_udc_write_ep0_fifo(ep, req)) {
			ep->udc->ep0state = EP0_IDLE;
			DEBUG_DBG("ep0 in0 done");
		} else {
			bOk = 0;
		}
		break;

	case EP0_OUT_DATA_PHASE:
		DEBUG_DBG("EP0_OUT_DATA_PHASE *** what now?");
		if (sp_udc_read_ep0_fifo(ep, req)) {
			ep->udc->ep0state = EP0_IDLE;
			DEBUG_DBG("ep0 out1 done");
		} else {
			bOk = 0;
		}
		break;
	}

	return bOk;
}

static void sp_udc_handle_ep0(struct sp_udc *udc)
{
	struct usb_composite_dev *cdev = get_gadget_data(&udc->gadget);
	struct usb_request *req_g = NULL;
	struct sp_request *req = NULL;
	struct sp_ep *ep = &udc->ep[0];

	DEBUG_DBG(">>> %s ...", __func__);

	if (!cdev) {
		pr_err("cdev invalid");
		return;
	}

	req_g = cdev->req;
	req = to_sp_req(req_g);

	if (!req) {
		pr_err("req invalid");
		return;
	}

	sp_udc_handle_ep0_proc(ep, req, 1);
	DEBUG_DBG("<<< %s ... ", __func__);
}

#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
static int sp_udc_ep11_bulkout_pio(struct sp_ep *ep, struct sp_request *req);

static int sp_udc_ep11_bulkout_dma(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int cur_length = req->req.length;
	int actual_length = 0;
	int dma_xferlen;
	unsigned long t;
	u8 *buf;

	DEBUG_DBG(">>> %s...", __func__);
	DEBUG_DBG("req.length=%d req.actual=%d, req->req.dma=%xh UDCIF=%xh",
			req->req.length, req->req.actual, req->req.dma, udc->reg_read(UDCIF));

	udc->reg_write(udc->reg_read(UDNBIE) & (~EP11O_IF), UDNBIE);

	if (req->req.dma == DMA_ADDR_INVALID) {
		req->req.dma = dma_map_single(ep->udc->gadget.dev.parent,
						(u8 *)req->req.buf,
						req->req.actual + udc->dma_xferlen_ep11,
						DMA_FROM_DEVICE);
		if (dma_mapping_error(ep->udc->gadget.dev.parent, req->req.dma)) {
			DEBUG_DBG("dma_mapping_error");
			return 1;
		}
	}

	while (actual_length < udc->dma_xferlen_ep11) {
		buf = (u8 *)(req->req.dma + req->req.actual + actual_length);
		dma_xferlen = udc->dma_xferlen_ep11;

		if ((udc->reg_read(UDEPBFS) & 0x06) == 0x06)
			udc->reg_write(udc->reg_read(UDEPABC) | CLR_EP_OVLD, UDEPABC);

		udc->reg_write((u32) buf, UDEPBDMADA);
		udc->reg_write((udc->reg_read(UDEPBDMACS) & (~DMA_COUNT_MASK)) |
				DMA_COUNT_ALIGN | DMA_WRITE | dma_xferlen | DMA_EN,
				UDEPBDMACS);

		DEBUG_DBG("cur_len=%d actual_len=%d req.dma=%xh dma_len=%d\t"
				"\b\b\b\b\bUDEPBDMACS = %xh UDEPBFS = %xh UDCIF = %xh",
				cur_length, actual_length, req->req.dma, dma_xferlen,
				udc->reg_read(UDEPBDMACS), udc->reg_read(UDEPBFS), udc->reg_read(UDCIF));

		t = jiffies;
		udc->reg_write(udc->reg_read(UDCIE) | EPB_DMA_IF, UDCIE);

		while ((udc->reg_read(UDEPBDMACS) & DMA_EN) != 0) {
			DEBUG_DBG(">>cur_len=%d actual_len=%d req.dma=%xh dma_len=%d\t"
					"\b\b\b\b\bUDEPBDMACS = %xh UDCIE = %xh UDCIF = %xh",
					cur_length, actual_length, req->req.dma,
					dma_xferlen, udc->reg_read(UDEPBDMACS), udc->reg_read(UDCIE),
					udc->reg_read(UDCIF));

			if (time_after(jiffies, t + 10 * HZ)) {
				DEBUG_DBG("dma error: UDEPBDMACS = %xh",
							udc->reg_read(UDEPBDMACS));
				break;
			}
		}

		DEBUG_DBG("UDCIF = %xh", udc->reg_read(UDCIF));

		actual_length += dma_xferlen;
		fsleep(50);
	}

	if (req->req.dma != DMA_ADDR_INVALID) {
		dma_unmap_single(ep->udc->gadget.dev.parent, req->req.dma,
					req->req.actual + udc->dma_xferlen_ep11, DMA_FROM_DEVICE);

		req->req.dma = DMA_ADDR_INVALID;
	}

	if ((udc->reg_read(UDEPBFS) & 0x06) == 0x06)
		udc->reg_write(udc->reg_read(UDEPABC) | CLR_EP_OVLD, UDEPABC);

	udc->reg_write(udc->reg_read(UDCIE) & (~EPB_DMA_IF), UDCIE);
	udc->reg_write(udc->reg_read(UDNBIE) | EP11O_IF, UDNBIE);

	DEBUG_DBG("UDEPBDMACS = %x", udc->reg_read(UDEPBDMACS));
	DEBUG_DBG("<<< %s...", __func__);

	return 1;
}
#endif

static void sp_udc_bulkout_pio(struct sp_udc *udc, u8 *buf, u32 avail)
{
	prefetch(buf);
	sp_udc_read_fifo_packet(udc, UDEPBFDP, buf, avail, UDEPBVB);
	udc->reg_write(udc->reg_read(UDEPABC) | CLR_EP_OVLD, UDEPABC);
}

static int sp_udc_ep11_bulkout_pio(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int pre_is_pingbuf;
	int is_pingbuf;
	int delay_count;
	int is_last;
	u32 count;
	u32 avail;
	u8 *buf;
#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
	char signature[4];
	u32 block_len;
	u32 residule;
	u8 check_len;
	u8 state;
	int i;
#endif

	DEBUG_DBG(">>> %s UDEPBFS = %xh", __func__, udc->reg_read(UDEPBFS));
	DEBUG_DBG("1.req.length=%d req.actual=%d req->req.dma = %xh UDEPBFS = %xh",
			req->req.length, req->req.actual, req->req.dma, udc->reg_read(UDEPBFS));

	if (down_trylock(&ep->wsem))
		return 0;

#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
	state = 0;
	check_len = false;
	residule = 0;
#endif

	is_last = 0;
	delay_count = 0;
	is_pingbuf = (udc->reg_read(UDEPBPPC) & CURR_BUFF) ? 1 : 0;

	do {
#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
		if (check_len == true) {
			check_len = false;

			/* use DMA mode when transfer len >= 0x200 */
			if (residule >= 0x200) {
				udc->dma_xferlen_ep11 = residule;
				residule = udc->dma_xferlen_ep11 & 0x1ff;
				udc->dma_xferlen_ep11 = udc->dma_xferlen_ep11 & ~0x1ff;

				/* dma transfer len must be multiple of 0x200 */
				/* but not be multiple of 0x400 	      */
				if ((udc->dma_xferlen_ep11 % 0x400) == 0) {
					udc->dma_xferlen_ep11 -= 0x200;
					residule += 0x200;
				}

				state = 1;
			} else {
				is_pingbuf = (udc->reg_read(UDEPBPPC) & CURR_BUFF) ? 1 : 0;
				state = 0;
			}
		}
#endif

		pre_is_pingbuf = is_pingbuf;
		count = sp_udc_get_ep_fifo_count(ep->udc, is_pingbuf, UDEPBPIC);
		if (!count) {
			up(&ep->wsem);

			return 1;
		}

		buf = (u8 *)req->req.buf + req->req.actual;

		if (count > ep->ep.maxpacket)
			avail = ep->ep.maxpacket;
		else
			avail = count;

#if (TRANS_MODE == PIO_MODE)
		sp_udc_bulkout_pio(udc, buf, avail);
#elif (TRANS_MODE == DMA_MODE_FOR_NCMH)
		switch (state) {
		case 0: /* PIO mode */
			sp_udc_bulkout_pio(udc, buf, avail);

			for (i = 0; i < 4; i++)
				signature[i] = *(buf + i);

			if (!strncmp(signature, "NCMH", 4) || !strncmp(signature, "ncmh", 4)) {
				if (!strncmp(signature, "NCMH", 4))
					block_len = (u32)((struct usb_cdc_ncm_nth16 *)buf)->wBlockLength;
				else if (!strncmp(signature, "ncmh", 4))
					block_len = (u32)((struct usb_cdc_ncm_nth32 *)buf)->dwBlockLength;

				udc->dma_xferlen_ep11 = block_len - avail - req->req.actual;

				/* use DMA mode when transfer len >= 0x200 */
				if (udc->dma_xferlen_ep11 >= 0x200) {
					residule = udc->dma_xferlen_ep11 & 0x1ff;
					udc->dma_xferlen_ep11 = udc->dma_xferlen_ep11 & ~0x1ff;

					/* dma transfer len must be multiple of 0x200 */
					/* but not be multiple of 0x400 	      */
					if ((udc->dma_xferlen_ep11 % 0x400) == 0) {
						udc->dma_xferlen_ep11 -= 0x200;
						residule += 0x200;
					}

					state = 1;
				}
			}

			break;
		case 1: /* DMA mode */
			sp_udc_ep11_bulkout_dma(ep, req);

			avail = udc->dma_xferlen_ep11;
			check_len = true;

			break;
		}
#endif

		req->req.actual += avail;

#if (TRANS_MODE == PIO_MODE)
		if (count < ep->ep.maxpacket || req->req.length <= req->req.actual)
			is_last = 1;
#elif (TRANS_MODE == DMA_MODE_FOR_NCMH)
		if (block_len <= req->req.actual)
			is_last = 1;
#endif

		DEBUG_DBG("2.req.length = %d req.actual = %d UDEPBFS = %xh UDEPBPPC = %xh\t"
				"\b\b\b\b\bUDEPBPOC = %xh UDEPBPIC = %xh avail = %d is_last = %d",
				req->req.length, req->req.actual, udc->reg_read(UDEPBFS),
				udc->reg_read(UDEPBPPC), udc->reg_read(UDEPBPOC), udc->reg_read(UDEPBPIC),
				avail, is_last);

		if (is_last)
			break;

out_fifo_retry:
		if ((udc->reg_read(UDEPBFS) & 0x22) == 0) {
			udelay(1);
			if (delay_count++ < 20)
				goto out_fifo_retry;

			delay_count = 0;
		}

out_fifo_controllable:
		is_pingbuf = (udc->reg_read(UDEPBPPC) & CURR_BUFF) ? 1 : 0;

		if (is_pingbuf == pre_is_pingbuf)
			goto out_fifo_controllable;
	} while (1);

	DEBUG_DBG("3.req.length=%d req.actual=%d UDEPBFS = %xh count=%d is_last=%d",
			req->req.length, req->req.actual, udc->reg_read(UDEPBFS), count, is_last);

	if (req->req.actual >= 320)
		sp_print_hex_dump_bytes("30->20-", DUMP_PREFIX_OFFSET, req->req.buf, 320);
	else
		sp_print_hex_dump_bytes("30->20-", DUMP_PREFIX_OFFSET, req->req.buf,
									req->req.actual);

	sp_udc_done(ep, req, 0);
	up(&ep->wsem);
	DEBUG_DBG("<<< %s", __func__);

	return is_last;
}

#ifndef BULKIN_ENHANCED
	#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
static int sp_ep1_bulkin_dma(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int dma_xferlen = 0;
	u32 dma_delay = 0;

	DEBUG_DBG(">>> %s", __func__);

	dma_xferlen = min((int)(req->req.length - req->req.actual), 512);
	if (dma_xferlen < 512) return 0;

	if (req->req.dma == DMA_ADDR_INVALID) {
		req->req.dma = dma_map_single(ep->udc->gadget.dev.parent, req->req.buf+req->req.actual, dma_xferlen,
						(ep->bEndpointAddress & USB_DIR_IN) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
	}

	udelay(25);
	udc->reg_write(dma_xferlen | DMA_COUNT_ALIGN, UEP12DMACS);
	udc->reg_write((u32) req->req.dma, UEP12DMADA);
	udc->reg_write(udc->reg_read(UEP12DMACS) | DMA_EN, UEP12DMACS);

	while (udc->reg_read(UEP12DMACS) & DMA_EN) {
		// udc->reg_write(udc->reg_read(UDLIE) | EP1_DMA_IF, UDLIE);

		if (dma_delay++ > 50) {
			if (req->req.dma != DMA_ADDR_INVALID) {
				dma_unmap_single(ep->udc->gadget.dev.parent, req->req.dma, dma_xferlen,
								(ep->bEndpointAddress & USB_DIR_IN)
								? DMA_TO_DEVICE : DMA_FROM_DEVICE);

				req->req.dma = DMA_ADDR_INVALID;
			}

			DEBUG_DBG("dma error: UEP12DMACS = %xh", udc->reg_read(UEP12DMACS));

			return 0;
		}
	}

	udc->reg_write(EP1_DMA_IF, UDLIF);

	req->req.actual += dma_xferlen;
	DEBUG_DBG("req->req.actual = %d UEP12DMACS = %xh", req->req.actual,
		  udc->reg_read(UEP12DMACS));

	if (req->req.dma != DMA_ADDR_INVALID) {
		dma_unmap_single(ep->udc->gadget.dev.parent, req->req.dma, dma_xferlen,
						(ep->bEndpointAddress & USB_DIR_IN)
						? DMA_TO_DEVICE : DMA_FROM_DEVICE);

		req->req.dma = DMA_ADDR_INVALID;
	}

	udc->dma_len_ep1 = 0;
	DEBUG_DBG("<<< %s", __func__);

	return 1;
}
	#endif
#endif

static int sp_udc_int_in(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int count;

	DEBUG_DBG(">>> %s...", __func__);

	count = sp_udc_write_packet(udc, UDEP3DATA, req, ep->ep.maxpacket, UDEP3VB);
	udc->reg_write((1 << 0) | (1 << 3), UDEP3CTRL);
	req->req.actual += count;

	DEBUG_DBG("write ep3, count = %d, actual = %d, length = %d, zero = %d",
				count, req->req.actual, req->req.length, req->req.zero);

	if (req->req.actual == req->req.length) {
		DEBUG_DBG("write ep3, sp_udc_done");
		sp_udc_done(ep, req, 0);
	}

	DEBUG_DBG("<<< %s...", __func__);

	return 1;
}

#ifdef BULKIN_ENHANCED
static int sp_udc_ep1_bulkin_pio(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int pre_is_pingbuf;
	int is_pingbuf;
	int delay_count;
	int is_last;
	u32 count, w_count;

	DEBUG_DBG(">>> %s", __func__);

	if (down_trylock(&ep->wsem))
		return 0;

	is_last = 0;
	delay_count = 0;

	DEBUG_DBG("1.req.actual = %d req.length=%d req->req.dma=%xh UDEP12FS = %xh",
			req->req.actual, req->req.length, req->req.dma, udc->reg_read(UDEP12FS));

	is_pingbuf = (udc->reg_read(UDEP12PPC) & CURR_BUFF) ? 1 : 0;

	while (1) {
		pre_is_pingbuf = is_pingbuf;
		count = sp_udc_get_ep_fifo_count(ep->udc, is_pingbuf, UDEP12PIC);

		w_count = sp_udc_write_packet(udc, UDEP12FDP, req, ep->ep.maxpacket, UDEP12VB);
		udc->reg_write(udc->reg_read(UDEP12C) | SET_EP_IVLD, UDEP12C);
		req->req.actual += w_count;

		if (w_count != ep->ep.maxpacket)
			is_last = 1;
		else if (req->req.length == req->req.actual && !req->req.zero)
			is_last = 1;
		else
			is_last = 0;

		DEBUG_DBG("2.req.length = %d req.actual = %d UDEP12FS = %xh UDEP12PPC = %xh\t"
				"\b\b\b\b\bUDEP12POC = %xh UDEP12PIC = %xh count = %d w_count = %d\t"
				"\b\b\b\b\bis_last = %d",
				req->req.length, req->req.actual, udc->reg_read(UDEP12FS),
				udc->reg_read(UDEP12PPC), udc->reg_read(UDEP12POC), udc->reg_read(UDEP12PIC),
				count, w_count, is_last);

		if (is_last)
			break;

in_fifo_controllable:
		is_pingbuf = (udc->reg_read(UDEP12PPC) & CURR_BUFF) ? 1 : 0;

		if (is_pingbuf == pre_is_pingbuf)
			goto in_fifo_controllable;
	}

	sp_udc_done(ep, req, 0);

	up(&ep->wsem);
	DEBUG_DBG("3.req.actual = %d, count = %d, req.length=%d, UDEP12FS = %xh, is_last = %d",
			req->req.actual, count, req->req.length, udc->reg_read(UDEP12FS), is_last);

	if (req->req.actual >= 142)
		sp_print_hex_dump_bytes("20->30-", DUMP_PREFIX_OFFSET, req->req.buf, 142);
	else
		sp_print_hex_dump_bytes("20->30-", DUMP_PREFIX_OFFSET, req->req.buf,
						req->req.actual);

	DEBUG_DBG("<<< %s...", __func__);

	return is_last;
}

	#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
static int sp_ep1_bulkin_dma(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int actual_length = 0;
	int dma_xferlen;
	u8 *buf;

	DEBUG_DBG(">>> %s", __func__);

	if (req->req.dma == DMA_ADDR_INVALID) {
		req->req.dma = dma_map_single(ep->udc->gadget.dev.parent, req->req.buf, udc->dma_len_ep1,
						(ep->bEndpointAddress & USB_DIR_IN) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
	}

	buf = (u8 *)(req->req.dma + req->req.actual + actual_length);
	dma_xferlen = min(udc->dma_len_ep1 - req->req.actual - actual_length,
							(unsigned int)UDC_FLASH_BUFFER_SIZE);
	DEBUG_DBG("%p dma_xferlen = %d %d", buf, dma_xferlen, udc->dma_len_ep1);
	actual_length = dma_xferlen;

	if (dma_xferlen > 4096)
		DEBUG_NOTICE("dma in len err %d", dma_xferlen);

	udelay(30);
	udc->reg_write(dma_xferlen | DMA_COUNT_ALIGN, UEP12DMACS);
	udc->reg_write((u32) buf, UEP12DMADA);
	udc->reg_write(udc->reg_read(UEP12DMACS) | DMA_EN, UEP12DMACS);

	if (udc->reg_read(UEP12DMACS) & DMA_EN) {
		udc->reg_write(udc->reg_read(UDLIE) | EP1_DMA_IF, UDLIE);

		return 0;
	}

	udc->reg_write(EP1_DMA_IF, UDLIF);

	req->req.actual += actual_length;
	DEBUG_DBG("req->req.actual = %d UEP12DMACS = %xh", req->req.actual,
		  udc->reg_read(UEP12DMACS));

	if (req->req.dma != DMA_ADDR_INVALID) {
		dma_unmap_single(ep->udc->gadget.dev.parent, req->req.dma, udc->dma_len_ep1,
				(ep->bEndpointAddress & USB_DIR_IN)
							? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		req->req.dma = DMA_ADDR_INVALID;
	}

	udc->dma_len_ep1 = 0;
	DEBUG_DBG("<<< %s", __func__);

	return 1;
}

static int sp_udc_ep1_bulkin_dma(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int is_last = 0;
	u32 count;

	DEBUG_DBG(">>> %s", __func__);

	if ((req->req.actual) || (req->req.length == 0))
		goto _TX_BULK_IN_DATA;

	/* DMA Mode */
	udc->dma_len_ep1 = req->req.length - (req->req.length % bulkep_dma_block_size);

	if (udc->dma_len_ep1 == bulkep_dma_block_size) {
		udc->dma_len_ep1 = 0;
		goto _TX_BULK_IN_DATA;
	}

	if (udc->dma_len_ep1) {
		DEBUG_DBG("ep1 bulk in dma mode,zero=%d", req->req.zero);

		udc->reg_write(udc->reg_read(UDLIE) & (~EP1I_IF), UDLIE);

		if (!sp_ep1_bulkin_dma(ep, req)) {
			return 0;
		} else if (req->req.length == req->req.actual && !req->req.zero) {
			is_last = 1;
			goto done_dma;
		} else if (udc->reg_read(UDEP12FS) & 0x1) {
			DEBUG_DBG("ep1 dma->pio wait write!");
			goto done_dma;
		} else {
			count = sp_udc_write_packet(udc, UDEP12FDP, req, ep->ep.maxpacket, UDEP12VB);
			udc->reg_write(udc->reg_read(UDEP12C) | SET_EP_IVLD, UDEP12C);
			req->req.actual += count;
			sp_udc_done(ep, req, 0);
			DEBUG_DBG("ep1 dma->pio write!");
			udc->reg_write(udc->reg_read(UDLIE) | EP1I_IF, UDLIE);

			return 1;
		}
	}

_TX_BULK_IN_DATA:
	udc->reg_write(udc->reg_read(UDLIE) | EP1I_IF, UDLIE);
	is_last = sp_udc_ep1_bulkin_pio(ep, req);

	return is_last;

done_dma:
	DEBUG_DBG("is_last = %d", is_last);
	if (is_last)
		sp_udc_done(ep, req, 0);

	udc->reg_write(udc->reg_read(UDLIE) | EP1I_IF, UDLIE);
	DEBUG_DBG("<<< %s...", __func__);

	return is_last;
}
	#endif

static int sp_udc_ep1_bulkin(struct sp_ep *ep, struct sp_request *req)
{
	#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
	struct sp_udc *udc = ep->udc;
	#endif
	int ret;

	#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
	if (udc->dma_len_ep1 == 0)
		ret = sp_udc_ep1_bulkin_dma(ep, req);
	#else
	ret = sp_udc_ep1_bulkin_pio(ep, req);
	#endif

	return ret;
}
#else
static int sp_udc_ep1_bulkin(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int pre_is_pingbuf;
	int is_pingbuf;
	int delay_count;
	int is_last;
	u32 count, w_count;

	DEBUG_DBG(">>> %s", __func__);

	if (down_trylock(&ep->wsem))
		return 0;

	is_last = 0;
	delay_count = 0;

	DEBUG_DBG("1.req.actual = %d req.length=%d req->req.dma=%xh UDEP12FS = %xh",
			req->req.actual, req->req.length, req->req.dma, udc->reg_read(UDEP12FS));

	is_pingbuf = (udc->reg_read(UDEP12PPC) & CURR_BUFF) ? 1 : 0;

	while (1) {
		pre_is_pingbuf = is_pingbuf;
		w_count = ep->ep.maxpacket;

	#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
		if (!sp_ep1_bulkin_dma(ep, req))
	#endif
		{
			w_count = sp_udc_write_packet(udc, UDEP12FDP, req, ep->ep.maxpacket, UDEP12VB);
			udc->reg_write(udc->reg_read(UDEP12C) | SET_EP_IVLD, UDEP12C);
			count = sp_udc_get_ep_fifo_count(ep->udc, is_pingbuf, UDEP12PIC);
			req->req.actual += w_count;
		}

		if (w_count != ep->ep.maxpacket)
			is_last = 1;
		else if (req->req.length == req->req.actual && !req->req.zero)
			is_last = 1;
		else
			is_last = 0;

		DEBUG_DBG("2.req.length = %d req.actual = %d UDEP12FS = %xh UDEP12PPC = %xh\t"
				"\b\b\b\b\bUDEP12POC = %xh UDEP12PIC = %xh count = %d w_count = %d\t"
				"\b\b\b\b\bis_last = %d",
				req->req.length, req->req.actual, udc->reg_read(UDEP12FS),
				udc->reg_read(UDEP12PPC), udc->reg_read(UDEP12POC), udc->reg_read(UDEP12PIC),
				count, w_count, is_last);

		if (is_last)
			break;

in_fifo_controllable:
		is_pingbuf = (udc->reg_read(UDEP12PPC) & CURR_BUFF) ? 1 : 0;

		if (is_pingbuf == pre_is_pingbuf)
			goto in_fifo_controllable;
	}

	sp_udc_done(ep, req, 0);

	up(&ep->wsem);
	DEBUG_DBG("3.req.actual = %d, count = %d, req.length=%d, UDEP12FS = %xh, is_last = %d",
			req->req.actual, count, req->req.length, udc->reg_read(UDEP12FS), is_last);

	if (req->req.actual >= 142)
		sp_print_hex_dump_bytes("20->30-", DUMP_PREFIX_OFFSET, req->req.buf, 142);
	else
		sp_print_hex_dump_bytes("20->30-", DUMP_PREFIX_OFFSET, req->req.buf,
						req->req.actual);

	DEBUG_DBG("<<< %s...", __func__);

	return is_last;

}
#endif

static int sp_udc_ep11_bulkout(struct sp_ep *ep, struct sp_request *req)
{
	return sp_udc_ep11_bulkout_pio(ep, req);
}

static int sp_udc_handle_ep(struct sp_ep *ep, struct sp_request *req)
{
	struct sp_udc *udc = ep->udc;
	int idx = ep->bEndpointAddress & 0x7f;
	int ret = 0;

	DEBUG_DBG(">>> %s", __func__);

	if (!req) {
		int empty = list_empty(&ep->queue);
		if (empty) {
			req = NULL;
			ret = 1;
			DEBUG_DBG("idx = %d, req is NULL", idx);
		} else {
			req = list_entry(ep->queue.next, struct sp_request, queue);
		}
	}

	if (req) {
		switch (idx) {
		case EP1:
			if ((udc->reg_read(UDEP12FS) & 0x1) == 0)
				ret = sp_udc_ep1_bulkin(ep, req);

			break;
		case EP3:
		case EP4:
		case EP6:
			ret = sp_udc_int_in(ep, req);

			break;
		case EP11:
			if (udc->reg_read(UDEPBFS) & 0x22)
				ret = sp_udc_ep11_bulkout(ep, req);

			break;
		}
	}

	DEBUG_DBG("<<< %s ret = %d", __func__, ret);

	return ret;
}

#ifdef BULKIN_ENHANCED
	#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
static void ep1_dma_handle(struct sp_udc *dev)
{
	struct sp_ep *ep = &dev->ep[1];
	struct sp_udc *udc = ep->udc;
	struct sp_request *req;
		#if 0
	int ret;
		#endif

	if (list_empty(&ep->queue)) {
		req = NULL;
		pr_err("ep1_dma req is NULL\t!");

		return;
	}

	req = list_entry(ep->queue.next, struct sp_request, queue);
	if (req->req.actual != 0) {
		pr_err("WHY ep1");

		if (req->req.actual != req->req.length)
			return;
	}

	req->req.actual += udc->dma_len_ep1;
	if (req->req.dma != DMA_ADDR_INVALID) {
		dma_unmap_single(ep->udc->gadget.dev.parent, req->req.dma, udc->dma_len_ep1,
					(ep->bEndpointAddress & USB_DIR_IN)
						? DMA_TO_DEVICE : DMA_FROM_DEVICE);

		req->req.dma = DMA_ADDR_INVALID;
	}

	if (req->req.length == req->req.actual && !req->req.zero) {
		sp_udc_done(ep, req, 0);
		udc->dma_len_ep1 = 0;

		if (!(udc->reg_read(UDEP12FS) & 0x1))
			sp_udc_handle_ep(ep, NULL);

		DEBUG_DBG("ep1 dma: %d", udc->reg_read(UDEP12FS));

		return;
	}

		#if 0
	if (!(udc->reg_read(UDEP12FS) & 0x1)) {
		ret = sp_udc_write_packet(udc, UDEP12FDP, req, ep->ep.maxpacket, UDEP12VB);
		udc->reg_write(udc->reg_read(UDEP12C) | SET_EP_IVLD, UDEP12C);
		req->req.actual += ret;
		sp_udc_done(ep, req, 0);

		DEBUG_DBG("DMA->write fifo by pio count=%d!", ret);
	} else {
		DEBUG_DBG("wait DMA->write fifo by pio!");
	}
		#endif

	udc->dma_len_ep1 = 0;
	udc->reg_write(udc->reg_read(UDLIE) | EP1I_IF, UDLIE);
}
	#endif
#endif

static void sp_sendto_workqueue(struct sp_udc *udc, int n)
{
	spin_lock(&udc->qlock);
	queue_work(udc->wq[n].queue, &udc->wq[n].sw.work);
	spin_unlock(&udc->qlock);
}

static irqreturn_t sp_udc_irq(int irq, void *_dev)
{
	struct sp_udc *udc = (struct sp_udc *)_dev;
	unsigned long flags;
	u32 irq_en1_flags;
	u32 irq_en2_flags;

	spin_lock_irqsave(&udc->lock, flags);

	irq_en1_flags = udc->reg_read(UDCIF) & udc->reg_read(UDCIE);
	irq_en2_flags = udc->reg_read(UDLIE) & udc->reg_read(UDLIF);

	if (irq_en2_flags & RESET_RELEASE_IF) {
		udc->reg_write(RESET_RELEASE_IF, UDLIF);
		DEBUG_DBG("reset end irq");
	}

	if (irq_en2_flags & RESET_IF) {
		DEBUG_DBG("reset irq");
		/* two kind of reset :		*/
		/* - reset start -> pwr reg = 8	*/
		/* - reset end   -> pwr reg = 0	*/
		udc->gadget.speed = USB_SPEED_UNKNOWN;

		udc->reg_write((udc->reg_read(UDLCSET) | 8) & 0xFE, UDLCSET);
		udc->reg_write(RESET_IF, UDLIF);

		/* Allow LNK to suspend PHY */
		udc->reg_write(udc->reg_read(UDCCS) & (~UPHY_CLK_CSS), UDCCS);
		clearHwState_UDC(udc);

		udc->ep0state = EP0_IDLE;
		udc->gadget.speed = USB_SPEED_FULL;

		spin_unlock_irqrestore(&udc->lock, flags);
		
		return IRQ_HANDLED;
	}

	/* force disconnect interrupt */
	if (irq_en1_flags & VBUS_IF) {
		DEBUG_DBG("vbus_irq[%xh]", udc->reg_read(UDLCSET));
		udc->reg_write(VBUS_IF, UDCIF);
		vbusInt_handle(udc);
	}

	if (irq_en2_flags & SUS_IF) {
		DEBUG_DBG(">>> IRQ: Suspend");
		DEBUG_DBG("clear  Suspend Event");
		udc->reg_write(SUS_IF, UDLIF);

		if (udc->driver) {
			if (udc->gadget.speed != USB_SPEED_UNKNOWN
					&& udc->driver->suspend)
				udc->driver->suspend(&udc->gadget);

			if (udc->driver->disconnect)
				udc->driver->disconnect(&udc->gadget);
		}

		udc->gadget.speed = USB_SPEED_UNKNOWN;
		udelay(100);
		DEBUG_DBG("<<< IRQ: Suspend");
	}

	if (irq_en2_flags & EP0S_IF) {
		udc->reg_write(EP0S_IF, UDLIF);
		udelay(100);
		DEBUG_DBG("IRQ:EP0S_IF %d, udc->ep0state = %d", udc->reg_read(UDEP0CS), udc->ep0state);
		if ((udc->reg_read(UDEP0CS) & (EP0_OVLD | EP0_OUT_EMPTY)) ==
								(EP0_OVLD | EP0_OUT_EMPTY))
			udc->reg_write(udc->reg_read(UDEP0CS) | CLR_EP0_OUT_VLD, UDEP0CS);

		if (udc->ep0state == EP0_IDLE)
			sp_udc_handle_ep0(udc);
	}

	if ((irq_en2_flags & EP0I_IF)) {
		udelay(100);
		DEBUG_DBG("IRQ:EP0I_IF %d, udc->ep0state = %d", udc->reg_read(UDEP0CS), udc->ep0state);
		udc->reg_write(EP0I_IF, UDLIF);

		if (udc->ep0state != EP0_IDLE)
			sp_udc_handle_ep0(udc);
		else
			udc->reg_write(udc->reg_read(UDEP0CS) & (~EP_DIR), UDEP0CS);
	}

	if ((irq_en2_flags & EP0O_IF)) {
		udelay(100);
		DEBUG_DBG("IRQ:EP0O_IF %d, udc->ep0state = %d", udc->reg_read(UDEP0CS), udc->ep0state);
		udc->reg_write(EP0O_IF, UDLIF);

		if (udc->ep0state != EP0_IDLE)
			sp_udc_handle_ep0(udc);
	}

#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
	/* dma finish */
	if (irq_en2_flags & EP1_DMA_IF) {
		DEBUG_DBG("IRQ:UDC ep1 DMA");
		udc->reg_write(EP1_DMA_IF, UDLIF);

	#ifdef BULKIN_ENHANCED
		if (udc->dma_len_ep1)
			ep1_dma_handle(udc);
	#else
		sp_sendto_workqueue(udc, 1);
	#endif
	}
#endif

	if (irq_en2_flags & EP1I_IF) {
		DEBUG_DBG("IRQ:ep1 in %xh", udc->reg_read(UDCIE));
		udc->reg_write(EP1I_IF, UDLIF);
		sp_sendto_workqueue(udc, 1);
	}

	if (irq_en2_flags & EP3I_IF) {
		DEBUG_DBG("IRQ:ep3 in int");
		udc->reg_write(EP3I_IF, UDLIF);
		sp_udc_handle_ep(&udc->ep[3], NULL);
	}

#if (TRANS_MODE == DMA_MODE_FOR_NCMH)
	if (irq_en1_flags & EPB_DMA_IF) {
		DEBUG_DBG("IRQ:UDC ep11 DMA");

		udc->reg_write(EPB_DMA_IF, UDCIF);
		sp_sendto_workqueue(udc, 11);
	}
#endif

	if (udc->reg_read(UDNBIF) & EP11O_IF) {
		udc->reg_write(EP11O_IF, UDNBIF);
		udc->reg_write(udc->reg_read(UDNBIE) & (~EP11O_IF), UDNBIE);
		DEBUG_DBG("IRQ:ep11 out %xh %xh state=%d", udc->reg_read(UDNBIE),
						udc->reg_read(UDEPBFS), udc->gadget.state);
		sp_sendto_workqueue(udc, 11);
	}

	if (udc->reg_read(UDNBIF) & SOF_IF) {
		udc->reg_write(SOF_IF, UDNBIF);
		udc->reg_write(udc->reg_read(UDNBIE) | (SOF_IF), UDNBIE);
	}

	spin_unlock_irqrestore(&udc->lock, flags);

	return IRQ_HANDLED;
}

static int sp_udc_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags)
{
	struct sp_request *req = to_sp_req(_req);
	struct sp_ep *ep = to_sp_ep(_ep);
	struct sp_udc *udc;
	unsigned long flags;
	int idx;

	DEBUG_DBG(">>> %s...", __func__);

	if (unlikely(!_ep || (!ep->ep.desc && ep->ep.name != ep0name))) {
		pr_notice("%s: invalid args", __func__);

		return -EINVAL;
	}

	udc = ep->udc;
	if (unlikely(!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN))
		return -ESHUTDOWN;

	local_irq_save(flags);
	_req->status = -EINPROGRESS;
	_req->actual = 0;

	idx = ep->bEndpointAddress & 0x7f;
	do {
		if (list_empty(&ep->queue))
			if (idx == EP0 && sp_udc_handle_ep0_proc(ep, req, 0))
				break;

		DEBUG_DBG("req = %x, ep=%d, req_config=%d", req, idx, udc->req_config);
		if (likely(req)) {
			list_add_tail(&req->queue, &ep->queue);
			if ((idx && idx != EP3) && (idx && idx != EP4)) {
				sp_sendto_workqueue(udc, idx);
			} else {
				if (idx)
					sp_udc_handle_ep(&udc->ep[3], NULL);
			}
		}
	} while(0);

	local_irq_restore(flags);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct sp_request *req = NULL;
	struct sp_udc *udc;
	struct sp_ep *ep;
	int retval = -EINVAL;

	DEBUG_DBG(">>> %s...", __func__);
	DEBUG_DBG("%s dequeue", _ep->name);

	if (!_ep || !_req)
		return retval;

	ep = to_sp_ep(_ep);
	
	if (!ep->udc->driver)
		return -ESHUTDOWN;

	udc = to_sp_udc(ep->gadget);
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req) {
			list_del_init(&req->queue);
			_req->status = -ECONNRESET;
			retval = 0;
			break;
		}
	}

	if (retval == 0) {
		DEBUG_DBG("dequeued req %p from %s, len %d buf %p",
					req, _ep->name, _req->length, _req->buf);
		sp_udc_done(ep, req, -ECONNRESET);
	}

	DEBUG_DBG("<<< %s...retval = %d", __func__, retval);

	return retval;
}

/* return 0 : disconnect	*/
/* return 1 : connect		*/
static int sp_vbus_detect(void)
{
	return 1;
}

static void sp_udc_enable(void *baseaddr)
{
	/*						*/
	/* usb device interrupt enable			*/
	/* ---force usb bus disconnect enable		*/
	/* ---force usb bus connect interrupt enable	*/
	/* ---vbus interrupt enable			*/
	/*						*/
	DEBUG_DBG(">>> %s", __func__);

	/* usb device controller interrupt flag */
	udc_write(udc_read(UDCIF, baseaddr) & 0xFFFF, UDCIF, baseaddr);

	/* usb device link layer interrupt flag */
	udc_write(0xefffffff, UDLIF, baseaddr);

	udc_write(VBUS_IF, UDCIE, baseaddr);
	udc_write(EP0S_IF | RESET_IF | RESET_RELEASE_IF, UDLIE, baseaddr);

	if (sp_vbus_detect()) {
		udc_write(udc_read(UDLIE, baseaddr) | SUS_IF, UDLIE, baseaddr);
		udelay(200);
		udc_write(udc_read(UDCCS, baseaddr) | UPHY_CLK_CSS, UDCCS, baseaddr);	/* PREVENT SUSP */
		udelay(200);
		/* Force to Connect */
	}

	DEBUG_DBG("func:%s line:%d", __func__, __LINE__);
	DEBUG_DBG("<<< %s", __func__);
}

static int sp_udc_get_frame(struct usb_gadget *_gadget)
{
	struct sp_udc *udc = to_sp_udc(_gadget);
	u32 sof_value;

	DEBUG_DBG(">>> %s...", __func__);
	sof_value = udc->reg_read(UDFRNUM);
	DEBUG_DBG("<<< %s...", __func__);

	return sof_value;
}

static int sp_udc_wakeup(struct usb_gadget *_gadget)
{
	DEBUG_DBG(">>> %s...", __func__);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_set_selfpowered(struct usb_gadget *_gadget, int is_selfpowered)
{
	DEBUG_DBG(">>> %s...", __func__);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_pullup(struct usb_gadget *gadget, int is_on)
{
	struct sp_udc *udc = to_sp_udc(gadget);

	DEBUG_DBG(">>> %s...", __func__);
	
	if (is_on) {
		DEBUG_DBG("Force to Connect");
		/* Force to Connect */
		udc->reg_write((udc->reg_read(UDLCSET) | 8) & 0xFE, UDLCSET);
	} else {
		DEBUG_DBG("Force to Disconnect");
		/* Force to Disconnect */
		udc->reg_write(udc->reg_read(UDLCSET) | SOFT_DISC, UDLCSET);
	}

	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_vbus_session(struct usb_gadget *gadget, int is_active)
{
	DEBUG_DBG(">>> %s...", __func__);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_vbus_draw(struct usb_gadget *_gadget, unsigned int ma)
{
	DEBUG_DBG(">>> %s...", __func__);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_start(struct usb_gadget *gadget, struct usb_gadget_driver *driver)
{
	struct sp_udc *udc = to_sp_udc(gadget);

	DEBUG_DBG(">>> %s...", __func__);

	/* Sanity checks */
	if (!udc)
		return -ENODEV;
	if (udc->driver)
		return -EBUSY;

	if (!driver->bind || !driver->setup || driver->max_speed < USB_SPEED_FULL) {
		pr_err("Invalid driver: bind %p setup %p speed %d",
				driver->bind, driver->setup, driver->max_speed);

		return -EINVAL;
	}

	/* Hook the driver */
	udc->driver = driver;
	sp_udc_enable(udc->base_addr);

	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_stop(struct usb_gadget *gadget)
{
	struct sp_udc *udc = to_sp_udc(gadget);

	if (!udc)
		return -ENODEV;

	DEBUG_DBG(">>> %s...", __func__);

	/* report disconnect */
	// if (udc->driver->disconnect)
	// 	udc->driver->disconnect(&udc->gadget);

	// udc->driver->unbind(&udc->gadget);
	udc->driver = NULL;
	udc->gadget.speed = USB_SPEED_UNKNOWN;

	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static struct usb_ep *find_ep(struct usb_gadget *gadget, const char *name)
{
	struct usb_ep *ep = NULL;

	list_for_each_entry(ep, &gadget->ep_list, ep_list) {
		if (strcmp(ep->name, name) == 0)
			return ep;
	}

	return NULL;
}

static struct usb_ep *sp_match_ep(struct usb_gadget *_gadget,
					struct usb_endpoint_descriptor *desc,
						struct usb_ss_ep_comp_descriptor *ep_comp)
{
	struct usb_ep *ep;
	u8 type;
	
	type = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	desc->bInterval = 5;
	ep = NULL;

	if (type == USB_ENDPOINT_XFER_BULK) {
		ep = find_ep(_gadget, (USB_DIR_IN & desc->bEndpointAddress) ?
							"ep1in-bulk" : "ep11out-bulk");
	} else if (type == USB_ENDPOINT_XFER_INT) {
		ep = find_ep(_gadget, "ep3in-int");
	} else if (type == USB_ENDPOINT_XFER_ISOC) {
		ep = find_ep(_gadget, (USB_DIR_IN & desc->bEndpointAddress) ?
							"ep5-iso" : "ep12-iso");
	}

	return ep;
}

static const struct usb_gadget_ops sp_ops = {
	.get_frame		= sp_udc_get_frame,
	.wakeup			= sp_udc_wakeup,
	.set_selfpowered 	= sp_udc_set_selfpowered,
	.pullup			= sp_udc_pullup,
	.vbus_session		= sp_udc_vbus_session,
	.vbus_draw		= sp_vbus_draw,
	.udc_start		= sp_udc_start,
	.udc_stop		= sp_udc_stop,
	.match_ep		= sp_match_ep,
};

static const struct usb_ep_ops sp_ep_ops = {
	.enable			= sp_udc_ep_enable,
	.disable		= sp_udc_ep_disable,
	.alloc_request	 	= sp_udc_alloc_request,
	.free_request	 	= sp_udc_free_request,
	.queue			= sp_udc_queue,
	.dequeue		= sp_udc_dequeue,
	.set_halt		= sp_udc_set_halt,
};

static void sp_udc_reinit(struct sp_udc *udc)
{
	u32 i = 0;

	DEBUG_DBG(">>> %s", __func__);

	udc->gadget.ep0 = &udc->ep[0].ep;
	
	/* device/ep0 records init */
	INIT_LIST_HEAD(&udc->gadget.ep_list);
	INIT_LIST_HEAD(&udc->gadget.ep0->ep_list);
	udc->ep0state = EP0_IDLE;

	for (i = 0; i < SP_MAXENDPOINTS; i++) {
		struct sp_ep *ep = &udc->ep[i];

		ep->num = i;
		ep->ep.ops = &sp_ep_ops;
		ep->bEndpointAddress = i;

		switch (i) {
		case 0:
			ep->ep.name = ep0name;
			ep->ep.maxpacket = 64;
			break;
		case 1:
			ep->ep.name = "ep1in-bulk";
			ep->ep.maxpacket = 512;
			ep->bEndpointAddress |=  USB_DIR_IN;
			ep->bmAttributes = USB_ENDPOINT_XFER_BULK;
			break;
		case 2:
			ep->ep.name = "ep2out-bulk";
			ep->ep.maxpacket = 512;
			ep->bEndpointAddress |=  USB_DIR_OUT;
			ep->bmAttributes = USB_ENDPOINT_XFER_BULK;
			break;
		case 3:
			ep->ep.name = "ep3in-int";
			ep->ep.maxpacket = 64;
			ep->bEndpointAddress |=  USB_DIR_IN;
			ep->bmAttributes = USB_ENDPOINT_XFER_INT;
			break;
		case 4:
			ep->ep.name = "ep4in-int";
			ep->ep.maxpacket = 64;
			ep->bEndpointAddress |=  USB_DIR_IN;
			ep->bmAttributes = USB_ENDPOINT_XFER_INT;
			break;
		case 5:
			ep->ep.name = "ep5-iso";
			ep->ep.maxpacket = 1024 * 3;
			ep->bEndpointAddress |=  USB_DIR_OUT;
			ep->bmAttributes = USB_ENDPOINT_XFER_ISOC;
			break;
		case 6:
			ep->ep.name = "ep6in-int";
			ep->ep.maxpacket = 64;
			ep->bEndpointAddress |=  USB_DIR_IN;
			ep->bmAttributes = USB_ENDPOINT_XFER_INT;
			break;
		case 7:
			ep->ep.name = "ep7-iso";
			ep->ep.maxpacket = 64;
			ep->bEndpointAddress |=  USB_DIR_OUT;
			ep->bmAttributes = USB_ENDPOINT_XFER_ISOC;
			break;
		case 8:
			ep->ep.name = "ep8in-bulk";
			ep->ep.maxpacket = 64;
			ep->bEndpointAddress |=  USB_DIR_IN;
			ep->bmAttributes = USB_ENDPOINT_XFER_BULK;
			break;
		case 9:
			ep->ep.name = "ep9out-bulk";
			ep->ep.maxpacket = 64;
			ep->bEndpointAddress |=  USB_DIR_OUT;
			ep->bmAttributes = USB_ENDPOINT_XFER_BULK;
			break;
		case 10:
			ep->ep.name = "ep10in-bulk";
			ep->ep.maxpacket = 64;
			ep->bEndpointAddress |=  USB_DIR_IN;
			ep->bmAttributes = USB_ENDPOINT_XFER_BULK;
			break;
		case 11:
			ep->ep.name = "ep11out-bulk";
			ep->ep.maxpacket = 512;
			ep->bEndpointAddress |=  USB_DIR_OUT;
			ep->bmAttributes = USB_ENDPOINT_XFER_BULK;
			break;
		case 12:
			ep->ep.name = "ep12-iso";
			ep->ep.maxpacket = 64;
			ep->bEndpointAddress |=  USB_DIR_OUT;
			ep->bmAttributes = USB_ENDPOINT_XFER_ISOC;
			break;
		}

		if (i != 0)
			list_add_tail(&ep->ep.ep_list, &udc->gadget.ep_list);

		ep->udc = udc;
		ep->desc = NULL;
		ep->halted = 0;
		INIT_LIST_HEAD(&ep->queue);
		usb_ep_set_maxpacket_limit(&ep->ep, ep->ep.maxpacket);
	}

	DEBUG_DBG("<<< %s", __func__);
}

#ifdef CONFIG_FIQ_GLUE
static int fiq_isr(int fiq, void *data)
{
	sp_udc_irq(udc->irq_num, &memory);

	return IRQ_HANDLED;
}

static void fiq_handler(struct fiq_glue_handler *h, void *regs, void *svc_sp)
{
	void __iomem *cpu_base = gic_base(GIC_CPU_BASE);
	u32 irqstat, irqnr;

	irqstat = readl_relaxed(cpu_base + GIC_CPU_HIGHPRI);
	irqnr = irqstat & ~0x1c00;

	if (irqnr == udc->irq_num) {
		readl_relaxed(cpu_base + GIC_CPU_INTACK);
		fiq_isr(irqnr, h);
		writel_relaxed(irqstat, cpu_base + GIC_CPU_EOI);
	}
}
static irqreturn_t udcThreadHandler(int irq, void *dev_id)
{
	DEBUG_DBG("<DSR>");

	return IRQ_HANDLED;
}
#endif

/************************************ PLATFORM DRIVER & DEVICE ************************************/
static void sp_udc_work(struct work_struct *work)
{
	struct sp_work *sw = container_of(work, struct sp_work, work);
	struct sp_ep *ep = sw->ep;
	
	DEBUG_DBG(">>> %s", __func__);

	local_irq_save(ep->flags);
	sp_udc_handle_ep(ep, NULL);
	local_irq_restore(ep->flags);

	DEBUG_DBG("<<< %s", __func__);
}

static int sp_init_work(struct sp_udc *udc, int ep_num, char *desc, void (*func)(struct work_struct *))
{
	int ret = 0;

	sema_init(&udc->ep[ep_num].wsem, 1);

	udc->wq[ep_num].queue = create_singlethread_workqueue(desc);
	if (!udc->wq[ep_num].queue) {
		pr_err("cannot create workqueue %s", desc);
		ret = -ENOMEM;
	}

	udc->wq[ep_num].sw.ep = &udc->ep[ep_num];
	INIT_WORK(&udc->wq[ep_num].sw.work, func);

	return ret;
}

static int sp_udc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sp_udc *udc = NULL;
	struct resource *res;
	int ret;
	u64 rsrc_start, rsrc_len;
#ifdef CONFIG_USB_SUNPLUS_OTG
	struct usb_phy *otg_phy;
#endif

	udc = kzalloc(sizeof(*udc), GFP_KERNEL);
	if (!udc) {
		pr_err("malloc udc fail\n");
		return -ENOMEM;
	}

	udc->base_addr = NULL;

	udc->dev_attr = kzalloc(sizeof(*udc->dev_attr), GFP_KERNEL);
	if (!udc->dev_attr) {
		pr_err("malloc udc fail\n");
		return -ENOMEM;
	}

	udc->dev_attr->attr.mode = VERIFY_OCTAL_PERMISSIONS(0644);
	udc->dev_attr->show = udc_ctrl_show;
	udc->dev_attr->store = udc_ctrl_store;

	/* enable usb controller clock */
	udc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(udc->clk)) {
		pr_err("not found clk source");
		return PTR_ERR(udc->clk);
	}

	clk_prepare(udc->clk);
	clk_enable(udc->clk);

#ifdef CONFIG_USB_GADGET_PORT0_ENABLED
	pdev->id = 1;
#else
	pdev->id = 2;
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	udc->irq_num = platform_get_irq(pdev, 0);
	if (!res || !udc->irq_num) {
		pr_err("Not enough platform resources.");
		ret = -ENODEV;
		goto error;
	}

	rsrc_start = res->start;
	rsrc_len = resource_size(res);
	pr_info("%s: %llx, %llx, irq:%d", __func__, rsrc_start, rsrc_len, udc->irq_num);
	
	base_addr[gadget_num] = ioremap(rsrc_start, rsrc_len);
	udc->base_addr = base_addr[gadget_num];

	if (!udc->base_addr) {
		ret = -ENOMEM;
		goto err_mem;
	}

	udc->gadget.max_speed = USB_SPEED_HIGH;
	udc->gadget.ops = &sp_ops;
	udc->gadget.name = "sp_udc";
	udc->gadget.dev.init_name = "gadget";
	udc->gadget.dev.parent = dev;
	udc->gadget.dev.dma_mask = dev->dma_mask;
	udc->gadget.quirk_avoids_skb_reserve = 1;

	if (gadget_num==0) {
		udc->dev_attr->attr.name = "udc_ctrl0";
		udc->reg_read = udc0_read;
		udc->reg_write = udc0_write;
	} else if (gadget_num==1) {
		udc->dev_attr->attr.name = "udc_ctrl1";
		udc->reg_read = udc1_read;
		udc->reg_write = udc1_write;
	}

	sp_udc_reinit(udc);

	ret = sp_init_work(udc, 11, "sp-udc-ep11", sp_udc_work);
	if (ret != 0)
		goto err_map;

	ret = sp_init_work(udc, 1, "sp-udc-ep1", sp_udc_work);
	if (ret != 0)
		goto err_map;

	ret = request_irq(udc->irq_num, sp_udc_irq, 0, udc->gadget.name, udc);
	if (ret != 0) {
		pr_err("cannot get irq %i, err %d", udc->irq_num, ret);
		ret = -EBUSY;
		goto err_map;
	}

	platform_set_drvdata(pdev, udc);
	udc->vbus = 0;

	spin_lock_init(&udc->lock);
	spin_lock_init(&plock);
	spin_lock_init(&udc->qlock);

	init_ep_spin(udc);

#ifdef CONFIG_USB_SUNPLUS_OTG
	udc->bus_num = pdev->id;
	otg_phy = usb_get_transceiver_sp(pdev->id - 1);
	ret = otg_set_peripheral(otg_phy->otg, &udc->gadget);
	if (ret < 0)
		goto err_add_udc;
#endif

	ret = usb_add_gadget_udc(&pdev->dev, &udc->gadget);
	if (ret)
		goto err_add_udc;

	device_create_file(&pdev->dev, udc->dev_attr);
	gadget_num++;

	pr_info("%s: udc version = %x", __func__, udc->reg_read(VERSION));

	return 0;
err_add_udc:
	if (udc->irq_num)
		free_irq(udc->irq_num, udc);
err_map:
	pr_err("probe sp udc fail");
	return ret;
err_mem:
	release_mem_region(rsrc_start, rsrc_len);
error:
	pr_err("probe sp udc fail");

	return ret;
}

static int sp_udc_remove(struct platform_device *pdev)
{
	struct sp_udc *udc = platform_get_drvdata(pdev);

	DEBUG_DBG("<<< %s...", __func__);

	if (udc->driver)
		return -EBUSY;

	device_remove_file(&pdev->dev, udc->dev_attr);
	usb_del_gadget_udc(&udc->gadget);

	if (udc->irq_num)
		free_irq(udc->irq_num, udc);

	if (udc->base_addr)
		iounmap(udc->base_addr);

	platform_set_drvdata(pdev, NULL);

	/* disable usb controller clock */
	clk_disable(udc->clk);

	kfree(udc->dev_attr);
	kfree(udc);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int sp_udc_suspend(struct platform_device *pdev, pm_message_t state)
{
	DEBUG_DBG(">>> %s...", __func__);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}

static int sp_udc_resume(struct platform_device *pdev)
{
	struct sp_udc *udc = platform_get_drvdata(pdev);
	
	DEBUG_DBG(">>> %s...", __func__);

	sp_udc_enable(udc->base_addr);

	udc->reg_write(udc->reg_read(UDNBIE) | EP8N_IF | EP8I_IF, UDNBIE);
	udc->reg_write(EP_ENA | EP_DIR, UDEP89C);
	udc->reg_write(udc->reg_read(UDNBIE) | EP9N_IF | EP9O_IF, UDNBIE);
	DEBUG_DBG("<<< %s...", __func__);

	return 0;
}
#else
#define sp_udc_suspend	NULL
#define sp_udc_resume	NULL
#endif

#define CONFIG_USB_HOST_RESET_SP	1

#ifdef CONFIG_USB_HOST_RESET_SP
static int udc_init_c(void __iomem *baseaddr)
{
	if (baseaddr==NULL) baseaddr = base_addr[0];
	sp_udc_enable(baseaddr);

	/* iap debug */
	udc_write(udc_read(UDNBIE, baseaddr) | EP8N_IF | EP8I_IF, UDNBIE, baseaddr);
	udc_write(EP_ENA | EP_DIR, UDEP89C, baseaddr);
	udc_write(udc_read(UDNBIE, baseaddr) | EP9N_IF | EP9O_IF, UDNBIE, baseaddr);

	/* pullup */
	udc_write((udc_read(UDLCSET, baseaddr) | 8) & 0xFE, UDLCSET, baseaddr);
	udc_write(udc_read(UDCIE, baseaddr) & (~VBUS_IF), UDCIE, baseaddr);
	udc_write(udc_read(UDCIE, baseaddr) & (~EPC_TRB_IF), UDCIE, baseaddr);
	udc_write(udc_read(UDCIE, baseaddr) & (~EPC_ERF_IF), UDCIE, baseaddr);

	return 0;
}
#endif

void usb_switch(int device)
{
	void __iomem *uphy_ctl_addr = uphy0_res_moon4 + USBC_CTL_OFFSET;

	if (device) {
		writel(RF_MASK_V_SET(1 << 4), uphy_ctl_addr);
		msleep(1);
		writel(RF_MASK_V_CLR(1 << 5), uphy_ctl_addr);
	} else {
		udc0_write(udc0_read(UDLCSET) | SOFT_DISC, UDLCSET);
		msleep(1);
		writel(RF_MASK_V_SET(1 << 5), uphy_ctl_addr);
	}
}
EXPORT_SYMBOL(usb_switch);

/* Controlling squelch signal to slove the uphy bad signal problem */
void ctrl_rx_squelch(void)
{
	/* control rx signal */
	udc0_write(udc0_read(UEP12DMACS) | RX_STEP7, UEP12DMACS);
	DEBUG_DBG("%s UEP12DMACS: %x", __func__, udc0_read(UEP12DMACS));
}
EXPORT_SYMBOL(ctrl_rx_squelch);

void udc_otg_ctrl(void)
{
	u32 val;

	val = USB_CLK_EN | UPHY_CLK_CSS;
	udc0_write(val, UDCCS);
}
EXPORT_SYMBOL(udc_otg_ctrl);

void detech_start(void)
{
#ifdef	CONFIG_USB_HOST_RESET_SP
	udc_init_c(NULL);
#endif

	DEBUG_NOTICE("%s...", __func__);
}
EXPORT_SYMBOL(detech_start);

static const struct of_device_id sunplus_udc_ids[] = {
	{ .compatible = "sunplus,sp7021-usb-udc" },
	{ }
};
MODULE_DEVICE_TABLE(of, sunplus_udc_ids);

static struct platform_driver sunplus_driver_udc = {
	.driver		= {
		.name	= "sunplus-udc",
		.of_match_table = sunplus_udc_ids,
	},
	.probe		= sp_udc_probe,
	.remove		= sp_udc_remove,
	.suspend	= sp_udc_suspend,
	.resume		= sp_udc_resume,
};

static int __init udc_init(void)
{
	DEBUG_INFO("register sunplus_driver_udc");

	return platform_driver_register(&sunplus_driver_udc);
}

static void __exit udc_exit(void)
{
	platform_driver_unregister(&sunplus_driver_udc);
}

module_init(udc_init);
module_exit(udc_exit);

MODULE_AUTHOR("Vincent Shih <vincent.shih@sunplus.com>");
MODULE_DESCRIPTION("Sunplus USB UDC driver");
MODULE_LICENSE("GPL v2");

