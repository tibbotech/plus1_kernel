/*
 * linux/drivers/usb/gadget/sunplus_udc2.h
 * Sunplus on-chip full/high speed USB device controllers
 *
 * Copyright (C) 2004-2007 Herbert Pötzl - Arnaud Patard
 *	Additional cleanups by Ben Dooks <ben-linux@fluff.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _SUNPLUS_UDC2_H
#define _SUNPLUS_UDC2_H

#include <linux/interrupt.h>
#include <linux/usb/gadget.h>

//#define CONFIG_BOOT_ON_ZEBU

//#define PIO_MODE

#define CACHE_LINE_SIZE 		64

/* run speeed & max ep condig config */
#define UDC_FULL_SPEED			0x1
#define UDC_HIGH_SPEED			0x3

#define UDC_MAX_ENDPOINT_NUM		9
#define EP0				0
#define EP1				1
#define EP2				2
#define EP3				3
#define EP4				4
#define EP5				5
#define EP6				6
#define EP7				7
#define EP8				8

#define AHB_USBD_BASE0 			0x9c102800
#define AHB_USBD_END0			0x9c102b00
#define AHB_USBD_BASE1 			0x9c103800
#define AHB_USBD_END1			0x9c103b00

#define IRQ_USB_DEV_PORT0		182
#define IRQ_USB_DEV_PORT1		186

#define EP_FIFO_SIZE			64
#define BULK_MPS_SIZE		 	512
#define INT_MPS_SIZE			64
#define ISO_MPS_SIZE			1024

#define UDC_EP_TYPE_CTRL		0
#define UDC_EP_TYPE_ISOC		1
#define UDC_EP_TYPE_BULK		2
#define UDC_EP_TYPE_INTR		3

/* event ring config */
#define ENTRY_SIZE			(16)
#define TR_COUNT			(64)
#define EVENT_SEG_COUNT			1

#define EVENT_RING_SIZE			(16*TR_COUNT)
#define EVENT_RING_COUNT		(TR_COUNT)
#define TRANSFER_RING_SIZE		(16*TR_COUNT)
#define TRANSFER_RING_COUNT		(TR_COUNT)

/* sw desc define  */
#define AUTO_RESPONSE			0xff
#define AUTO_SET_CONF			0x1			/* setConfiguration auto response */
#define AUTO_SET_INF			0x2			/* set interface auto response */
#define NOT_AUTO_SETINT			0x0

#define FRAME_TRANSFER_NUM_0		0x0			/* number of  ISO/INT that can transferred 1 micro frame */
#define FRAME_TRANSFER_NUM_1		0x1
#define FRAME_TRANSFER_NUM_2		0x2
#define FRAME_TRANSFER_NUM_3		0x3

/* alignment length define */
#define ALIGN_64_BYTE			(1<<6)
#define ALIGN_4_BYTE			ALIGN_64_BYTE		// (1 << 2)
#define ALIGN_8_BYTE			ALIGN_64_BYTE		// (1 << 3)
#define ALIGN_16_BYTE			ALIGN_64_BYTE		// (1 << 4)
#define ALIGN_32_BYTE			ALIGN_64_BYTE		// (1 << 5)

#define HAL_UDC_ENDPOINT_NUMBER_MASK	0xF
#define EP_NUM(ep_addr)			(ep_addr & HAL_UDC_ENDPOINT_NUMBER_MASK)
#define EP_DIR(ep_addr)			((ep_addr>>7)&0x1)
#define SHIFT_LEFT_BIT4(x)		((uint32_t)(x)>>4)

/* regester define*/
#define DEVC_INTR_ENABLE		(1<<1)			/* DEVC_IMAN */
#define DEVC_INTR_PENDING		(1<<0)			/* DEVC_IMAN */

#define IMOD				(0)			/* DEVC_IMOD Interrupt moderation 0x9c409c40 */
#define EHB				(1<<3)			/* DEVC_ERDP */
#define DESI				(0x7)			/* DEVC_ERDP */
#define VBUS_DIS 			(1<<16)			/* DEVC_CTRL */
#define EINT				(1<<1)			/* DEVC_STS */
#define CLEAR_INT_VBUS			(0x3)			/* DEVC_STS */

#define EP_EN				(1<<0)
#define RDP_EN				(1<<1)			/* reload dequeue pointer */
#define EP_STALL			(1<<2)
#define UDC_RUN				(1<<31)			/* DEV_RS */

#define DRIVE_RESUME			(1<<10)			/* GL_CS */

#define ADDR_VLD			(1<<23)
#define DEV_ADDR			(0x7F)
#define FRNUM				(0x3FFF)

#define UPHY0_CLKEN			(1<<5)
#define USBC0_CLKEN			(1<<4)
#define UPHY0_RESET			(1<<5)
#define USBC0_RESET			(1<<4)

/* transfer ring normal TRB */
#define TRB_C				(1<<0)			/* entry3 */
#define TRB_ISP				(1<<2)
#define TRB_IOC				(1<<5)
#define TRB_IDT				(1<<6)
#define TRB_BEI				(1<<9)
#define TRB_DIR				(1<<16)
#define TRB_FID(x)			((x>>20)& 0x000007FF)	/* ISO */
#define TRB_SIA				(1<<31)			/* ISO */

/* transfer ring Link TRB */
#define	LTRB_PTR(x)			((x>>4)&0x0FFFFFFF)	/* entry0 */
#define LTRB_C				(1<<0)			/* entry3 */
#define LTRB_TC				(1<<1)
#define LTRB_IOC			(1<<5)
#define LTRB_TYPE(x)			((x & 0xfc00) >> 10)

/* Device controller event TRB */
#define ETRB_CC(x)			((x>>24)&0xFF)		/* entry2 */
#define ETRB_C(x)			((x)&(1<<0))
#define ETRB_TYPE(x)			((x>>10)&0x3F)

/* Setup stage event TRB */
#define ETRB_SDL(x)			(x)			/* entry0:wValue, bRequest,bmRequestType */
#define ETRB_SDH(x)			(x)			/* entry1:wLength, wIndex */
#define	ETRB_ENUM(x)			((x>>16)&0xF)		/* entry3 */

/* Transfer event TRB */
#define	ETRB_TRBP(x)			(x)			/* entry0 */
#define	ETRB_LEN(x)			(x&0xFFFFFF)		/* entry2 */
#define	ETRB_EID(x)			((x>>16)&0x1F)		/* entry3 */

/* SOF event TRB */
#define ETRB_FNUM(x)			(x&0x1FFFF)		/* entry2 */
#define	ETRB_SLEN(x)			((x>>11)&0x1FFF)

#define ERDP_MASK			0xFFFFFFF0

/* TRB type */
#define NORMAL_TRB			1
#define SETUP_TRB			2
#define LINK_TRB			6
#define TRANS_EVENT_TRB			32
#define DEV_EVENT_TRB			48
#define SOF_EVENT_TRB			49

/* completion codes */
#define INVALID_TRB			0
#define SUCCESS_TRB			1
#define DATA_BUF_ERR			2
#define BABBLE_ERROR			3
#define TRANS_ERR			4
#define TRB_ERR				5
#define RESOURCE_ERR			7
#define SHORT_PACKET			13
#define EVENT_RING_FULL			21
#define UDC_STOPED			26
#define UDC_RESET			192
#define UDC_SUSPEND			193
#define UDC_RESUME 			194

#define	DMA_ADDR_INVALID		(~(dma_addr_t)0)

/* USB device power state */
enum udc_power_state {
	UDC_POWER_OFF = 0,	/* USB Device power on */
	UDC_POWER_LOW,		/* USB Device low power */
	UDC_POWER_FULL,		/* USB Device power off */
};

/* base TRB data struct */
struct trb_data {
	uint32_t entry0;
	uint32_t entry1;
	uint32_t entry2;
	uint32_t entry3;
};

struct udc_ring {
	struct trb_data *trb_va; 	/* ring start pointer */
	dma_addr_t trb_pa;		/* ring phy address */
	struct trb_data *end_trb_va;	/* ring end trb address*/
	dma_addr_t end_trb_pa;		/* ring end trb phy address */
	uint8_t num_mem;		/* number of ring members */
};

/* device descriptor */
struct sp_desc{
	uint32_t entry0;
	uint32_t entry1;
	uint32_t entry2;
	uint32_t entry3;
};

/**********************************************
 * struct info
 **********************************************/
/* Transfer TRB data struct(normal TRB) */
struct normal_trb {
	uint32_t ptr;		/* Data buffer pointer */
	uint32_t rsvd1;		/* Reserved shall always be 0 */
	uint32_t tlen:17;	/* TRB transfer length */
	uint32_t rsvd2:15;	/* Reserved for future use */
	uint32_t cycbit:1;	/* Cycle bit */
	uint32_t rsvd3:1;	/* Reserved for future use */
	uint32_t isp:1;		/* Interrrupt on short packet */
	uint32_t rsvd4:2;	/* Reserved for future use */
	uint32_t ioc:1;		/* Interrupt on completion */
	uint32_t idt:1;		/* Inmediate data */
	uint32_t rsvd5:2;	/* Reserved for future use */
	uint32_t bei:1;		/* Block event interrupt */
	uint32_t type:6;	/* TRB type */
	uint32_t dir:1;		/* OUT/IN transfer */
	uint32_t rsvd6:3;	/* Reserved */
	uint32_t fid:11;	/* Frame ID */
	uint32_t sia:1;		/* Start ISOchronous ASAP */
};

/* Transfer TRB data struct(link TRB) */
struct link_trb {
	uint32_t rsvd1:4;	/* Reserved shall always be 0 */
	uint32_t ptr:28;	/* Ring segment pointer */
	uint32_t rsvd2;		/* Reserved shall always be 0 */
	uint32_t rfu1;		/* Reserved for future use */
	uint32_t cycbit:1;	/* Cycle bit */
	uint32_t togbit:1;	/* Toggle bit */
	uint32_t rfu2:3;	/* Reserved for future use */
	uint32_t rfu3:5;	/* Reserved for future use */
	uint32_t type:6;	/* Rrb type */
	uint32_t rfu4:16;	/* Reserved for future use */
};

/* Event Ring segment table entry */
struct segment_table {
	uint32_t rsvd0:6;	/* Reserved shall always be 0 */
	uint32_t rsba:26;	/* Ring segment base address */
	uint32_t rsvd1;		/* Reserved for future use */
	uint32_t rssz:16;	/* Reserved for future use */
	uint32_t rsvd2:16;	/* Reserved for future use */
	uint32_t rsvd3;		/* Reserved for future use */
};

/* event TRB (Transfer) */
struct transfer_event_trb {
	uint32_t trbp;		/* The pointer of the TRB which generated this event */
	uint32_t rsvd1:32;	/* Reserved for future use */
	uint32_t len:24;	/* TRB transfer length */
	uint32_t cc:8;		/* Completion code */
	uint32_t cycbit:1;	/* Cycle bit */
	uint32_t rsvd2:9;	/* Reserved for future use */
	uint32_t type:6;	/* TRB type */
	uint32_t eid:5;		/* Endpoint ID */
	uint32_t rsvd3:11;	/* Reserved for future use */
};

/* event TRB (device) */
struct device_event_trb {
	uint32_t rfu1;		/* Reserved for future use */
	uint32_t rfu2;		/* Reserved for future use */
	uint32_t rfu3:24;	/* Reserved for future use */
	uint32_t cc:8;		/* Completion code */
	uint32_t cycbit:1;	/* Cycle bit */
	uint32_t rfu4:9;	/* Reserved for future use */
	uint32_t type:6;	/* TRB type */
	uint32_t rfu5:16;	/* Reserved for future use */
};

/* event TRB (setup) */
struct setup_trb_t {
	uint32_t sdl;		/* Low word of the setup data */
	uint32_t sdh;		/* High word of the setup data */
	uint32_t len:17;	/* Transfer length,always 8 */
	uint32_t rdu1:7;	/* Reserved for future use */
	uint32_t cc:8;		/* Completion code */
	uint32_t cycbit:1;	/* Cycle bit */
	uint32_t rfu:9;		/* Reserved for future use */
	uint32_t type:6;	/* TRB type */
	uint32_t epnum:4;	/* Endpoint number */
	uint32_t rfu2:12;	/* Reserved for future use */
};

/* Endpoint 0 descriptor data struct */
struct endpoint0_desc {
	uint32_t cfgs:8;	/* Device configure setting */
	uint32_t cfgm:8;	/* Device configure mask */
	uint32_t speed:4;	/* Device speed */
	uint32_t rsvd1:12;	/* Reserved for future use */
	uint32_t sofic:3;	/* SOF interrupt control */
	uint32_t rsvd2:1;	/* Reserved for future use */
	uint32_t aset:4;	/* Auto setup */
	uint32_t rsvd3:24;	/* Reserved for future use */
	uint32_t rsvd4:32;	/* Reserved for future use */
	uint32_t dcs:1;		/* De-queue cycle bit */
	uint32_t rsvd5:3;	/* Reserved,shall always be 0 */
	uint32_t dptr:28;	/* TR de-queue pointer */
};

/* Endpoint number 1~15 descriptor data struct */
struct endpointn_desc {
	volatile uint32_t ifs:8;	/* Interface setting */
	volatile uint32_t ifm:8;	/* Interface mask */
	volatile uint32_t alts:8;	/* Alternated setting */
	volatile uint32_t altm:8;	/* Alternated setting mask */
	volatile uint32_t num:4;	/* Endpoint number */
	volatile uint32_t type:2;	/* Endpoint type */
	volatile uint32_t rsvd1:26;	/* Reserved for future use */
	volatile uint32_t mps:16;	/* Maximum packet size of endpoint */
	volatile uint32_t rsvd2:14;	/* Reserved for future use */
	volatile uint32_t mult:2;	/* Ror high speed device */
	volatile uint32_t dcs:1;	/* De-queue cycle bit state */
	volatile uint32_t rsvd3:3;	/* Reserved,shall always be 0 */
	volatile uint32_t dptr:28;	/* TR de-queue pointer */
};

/* USB Device regiester */
struct udc_reg {
	/* Group0 */
	uint32_t DEVC_VER;
	uint32_t DEVC_MMR;
	uint32_t DEVC_REV0[2];
	uint32_t DEVC_PARAM;
	uint32_t DEVC_REV1[3];
	uint32_t GL_CS;
	uint32_t DEVC_REV2[23];

	/* Group1 */
	uint32_t DEVC_IMAN;
	uint32_t DEVC_IMOD;
	uint32_t DEVC_ERSTSZ;
	uint32_t DEVC_REV3[1];
	uint32_t DEVC_ERSTBA;
	uint32_t DEVC_REV4[1];
	uint32_t DEVC_ERDP;
	uint32_t DEVC_REV5[1];
	uint32_t DEVC_ADDR;
	uint32_t DEVC_REV6[1];
	uint32_t DEVC_CTRL;
	uint32_t DEVC_STS;
	uint32_t DEVC_REV7[18];
	uint32_t DEVC_DTOGL;
	uint32_t DEVC_DTOGH;

	/* Group2 */
	uint32_t DEVC_CS;
	uint32_t EP0_CS;
	uint32_t EPN_CS[30];

	/* Group3 */
	uint32_t MAC_FSM;
	uint32_t DEV_EP0_FSM;
	uint32_t EPN_FSM[30];

	/* Group4 */
	uint32_t USBD_CFG;
	uint32_t USBD_INF;
	uint32_t EPN_INF[30];

	/* Group5 */
	uint32_t USBD_FRNUM_ADDR;
	uint32_t USBD_DEBUG_PP;
	#define PP_PORT_IDLE   0x0
	#define PP_FULL_SPEED  0x10
	#define PP_HIGH_SPEED  0x18
	//uint32_t USBD_REV8[30];
};

struct sp_request {
	struct list_head	queue;			/* ep's requests */
	struct usb_request	req;
	struct trb_data		*transfer_trb;		/* pointer transfer trb*/
	uint8_t			*buffer;
};

/* USB Device endpoint struct */
struct udc_endpoint {
	bool			 is_in;			/* Endpoint direction */
	#define			 ENDPOINT_HALT	(1)
	#define			 ENDPOINT_READY	(0)
	uint16_t		 status;		/* Endpoint status */
	uint8_t			 num;			/* Endpoint number 0~15*/
	uint8_t			 type;			/* Endpoint type 0~3,totle is 4 kind of types*/
	uint8_t			 *transfer_buff;	/* Pointer to transfer buffer */
	dma_addr_t 		 transfer_buff_pa;
	uint32_t		 transfer_len;		/* transfer length */
	uint16_t		 maxpacket;		/* Endpoint Max packet size */

	uint8_t			 bEndpointAddress;
	uint8_t 		 bmAttributes;

	struct usb_ep 		 ep;
	struct sp_udc 		 *dev;
	struct usb_gadget 	 *gadget;
	struct udc_ring 	 ep_transfer_ring;	/* One transfer ring per ep */
	struct trb_data		 *ep_trb_ring_dq;	/* transfer ring dequeue */
	spinlock_t 		 lock;
	struct list_head	 queue;
};

void __iomem 		*moon3_reg;
void __iomem 		*moon4_reg;

struct sp_udc {
	bool 			 aset_flag; 			/* auto set flag, If this flag is true, zero packet will not be sent */
	struct clk		*clock;
	int 			 irq_num;
	struct usb_phy	 	*usb_phy;
	int 			 port_num;
	uint32_t 		 frame_num;
	bool 			 bus_reset_finish;
	bool			 def_run_full_speed; 		/* default high speed mode */
	struct timer_list 	 sof_polling_timer;
	struct timer_list 	 before_sof_polling_timer;
	bool			 vbus_active;
	int			 usb_test_mode; 		/* USB IF device test mode */

	spinlock_t		 lock;
	struct tasklet_struct 	 event_task;
	struct usb_gadget	 gadget;
	struct usb_gadget_driver *driver;
	struct device		 *dev;

	volatile struct udc_reg	 *reg;				/* UDC Register  */
	struct sp_desc 		 *ep_desc;			/* ep description pointer */
	dma_addr_t 		 ep_desc_pa;			/* ep desc phy address */
	uint8_t			 event_ccs;			/* Consumer Cycle state */
	uint8_t			 current_event_ring_seg;	/* current event ring segment index */
	uint8_t			 event_ring_seg_total;		/* Total number of event ringg seg */
	struct segment_table 	 *event_seg_table;		/* evnet seg */
	dma_addr_t 		 event_seg_table_pa;
	struct udc_ring		 *event_ring;			/* evnet ring pointer ,pointer all segment event ring */
	dma_addr_t		 event_ring_pa;			/* event ring pointer phy address */
	struct trb_data		 *event_ring_dq;		/* event ring dequeue */
	struct udc_endpoint 	 ep_data[UDC_MAX_ENDPOINT_NUM]; /* endpoint data struct */
};

int32_t hal_udc_init(struct sp_udc *udc);
int32_t hal_udc_deinit(struct sp_udc *udc);
int32_t hal_udc_device_connect(struct sp_udc *udc);
int32_t hal_udc_device_disconnect(struct sp_udc *udc);
int32_t hal_udc_endpoint_stall(struct sp_udc *udc, uint8_t ep_addr, bool stall);
int32_t hal_udc_power_control(struct sp_udc *udc, enum udc_power_state power_state);
int32_t hal_udc_endpoint_transfer(struct sp_udc	*udc, struct sp_request *req, uint8_t ep_addr, uint8_t *data, dma_addr_t data_pa, uint32_t length, uint32_t zero);
int32_t hal_udc_endpoint_unconfigure(struct sp_udc *udc, uint8_t ep_addr);
int32_t hal_udc_endpoint_configure(struct sp_udc *udc, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_max_packet_size);
#endif
