#ifndef __L2SW_DEFINE_H__
#define __L2SW_DEFINE_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/of_address.h>
#include <linux/of_mdio.h>



#if 1
#define DEBUG0 printk
#define DEBUG1() printk("[%s][%d]\n", __FUNCTION__, __LINE__)
#else
#define DEBUG0(str, ...)
#define DEBUG1()
#endif
#define ERROR0 printk

//#define CONFIG_PM
#define INTERRUPT_IMMEDIATELY
//#define RX_POLLING
#define PHY_CONFIG

#define ETHERNET_MAC_ADDRLEN            6

/*config descriptor*/
#define MAC_TX_DESC_NUM                 8      /* hardware limit 2,4 or 8*/
#define MAC_GUARD_DESC_NUM              2
#define MAC_RX_DESC_QUEUE_NUM           1      /* hardware limit 1,2 or 4*/
#define MAC_RX_QUEUE0_DESC_NUM          32      /* hardware limit even && >=4*/
#define MAC_RX_QUEUE1_DESC_NUM          8      /* hardware limit even && >=4*/
#define MAC_RX_QUEUE2_DESC_NUM          8      /* hardware limit even && >=4*/
#define MAC_RX_QUEUE3_DESC_NUM          8      /* hardware limit even  && >= 44*/

//define MAC interrupt status bit
#define MAC_INT_DAISY_MODE_CHG          (1<<31)
#define MAC_INT_PSC                     (1<<19)
#define MAC_INT_ETH0_LINK               (1<<24)
#define MAC_INT_ETH1_LINK               (1<<25)
#define MAC_INT_WDOG1_TMR_EXP           (1<<22)
#define MAC_INT_WDOG0_TMR_EXP           (1<<21)
#define MAC_INT_RX1_LAN_FULL            (1<<9)
#define MAC_INT_RX0_LAN_FULL            (1<<8)
#define MAC_INT_TX_L_DESCF              (1<<7)
#define MAC_INT_TX_H_DESCF              (1<<6)
#define MAC_INT_RX_DONE_L               (1<<5)
#define MAC_INT_RX_DONE_H               (1<<4)
#define MAC_INT_TX_DONE_L               (1<<3)
#define MAC_INT_TX_DONE_H               (1<<2)
#define MAC_INT_TX_DES_ER               (1<<1)
#define MAC_INT_RX_DES_ER               (1<<0)

#define MAC_INT_RX_DONE                 (MAC_INT_RX_DONE_H | MAC_INT_RX_DONE_L)
#define MAC_INT_TX_DONE                 (MAC_INT_TX_DONE_H | MAC_INT_TX_DONE_L)

#define MAC_INT_RX                      (MAC_INT_RX_DONE | MAC_INT_RX0_LAN_FULL | MAC_INT_RX_DES_ER)
#define MAC_INT_TX                      (MAC_INT_TX_DONE | MAC_INT_TX_DES_ER | MAC_INT_TX_H_DESCF | MAC_INT_TX_L_DESCF)


/*define port ability*/
#define PORT_ABILITY_LINK_ST    (1<<24)

/*define PHY command bit*/
#define PHY_WT_DATA_MASK        0xffff0000
#define PHY_RD_CMD              0x00004000
#define PHY_WT_CMD              0x00002000
#define PHY_REG_MASK            0x00001f00
#define PHY_ADR_MASK            0x0000001f

/*define PHY status bit*/
#define PHY_RD_DATA_MASK        0xffff0000
#define PHY_RD_RDY              0x00000002
#define PHY_WT_DONE             0x00000001

/*define other register bit*/
#define DESC_MODE_BIT           0x80000000
#define DIS_AER_PKT             0x00002000
#define EN_RX_VLAN              0x00001000
#define DIS_UC_PAUSE            0x00000800
#define DIS_BC                  0x00000400
#define DIS_MC_HASH             0x00000200
#define DIS_UC_HASH             0x00000100
#define EN_TOS                  0x00000010
#define EN_VLAN_PRI             0x00000008

#define SLEEP_MODE              0x80000000
#define PHY_LINK                0X01000000
#define DIS_PORT_TX             0x00000040
#define DIS_PORT_RX             (1<<24)
#define SOFT_RST_N              0x00000001
#define INT_MASK_VAL            0x000800f8
#define VLAN_ENABLE             0x00010000
#define DIS_BC_REC              0x00000400
#define DIS_MC_REC              0x00000200

#define RX_MAX_LEN_MASK         0x00011000
#define ROUTE_MODE_MASK         0x00000060
#define POK_INT_THS_MASK        0x000E0000
#define VLAN_TH_MASK            0x00000007

#define IP_PKG                  0x03
#define NON_IP                  0

/*define rx descriptor bit*/
#define ERR_CODE                (0xf<<26)
#define RX_TU_BIT               (0x1<<23)


#define PPPA                    (1<<26)
#define RX_CRC_BIT              (1<<18)
#define RX_FAE_BIT              (1<<22)
#define RX_RWT_BIT              (1<<21)
#define RX_PLE_BIT              (1<<20)
#define IPCE                    (1<<17)
#define L4CE                    (1<<15)

/*define tx descriptor bit*/
#define OWN_BIT                 (1<<31)
#define EOR_BIT                 (1<<31)
#define FS_BIT                  (1<<25)
#define LS_BIT                  (1<<24)
#define LEN_MASK                0x000007FF

#define OWC_BIT                 (1<<31)
#define TXOK_BIT                (1<<26)
#define LNKF_BIT                (1<<25)
#define BUR_BIT                 (1<<22)
#define TWDE_BIT                (1<<20)
#define CC_MASK                 0x000f0000
#define TBE_MASK                0x00070000

#define MAC_PHY_ADDR            0x01    /* define by hardware */

/*define CRC size in a package*/
#define MAC_CRC_SIZE            4       /* define by ethernet frame protocol*/

/*config descriptor*/
#define TX_DESC_NUM             8
#define RX_QUEUE0_DESC_NUM      8
#define RX_QUEUE1_DESC_NUM      8
#define RX_DESC_NUM_MAX         (RX_QUEUE0_DESC_NUM > RX_QUEUE1_DESC_NUM ? RX_QUEUE0_DESC_NUM : RX_QUEUE1_DESC_NUM)
#define RX_DESC_QUEUE_NUM       2
#define TX_DESC_QUEUE_NUM       1

#define MAC_RX_LEN_MAX          2047

#define DESC_ALIGN_BYTE         32
#define RX_OFFSET               0
#define TX_OFFSET               0
#define MAC_PHY_ID              32

/*define DMA mask code*/
#define MAC_DMA_32BIT           0xffffffffULL

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define MAC_VLAN_TAG_USED       1
#else
#define MAC_VLAN_TAG_USED       0
#endif

#define MAC_TX_BUFF_SIZE        1536
#define ETHERNET_MAC_ADDR_LEN   6


struct mac_desc {
	volatile u32 cmd1;
	volatile u32 cmd2;
	volatile u32 addr1;
	volatile u32 addr2;
};


struct skb_info {
	struct sk_buff *skb;
	u32 mapping;
	u32 len;
};


struct l2sw_mac {
	struct net_device *net_dev;
	struct net_device_stats dev_stats;
	struct platform_device *pdev;
	spinlock_t lock;
	void *desc_base;
	dma_addr_t desc_dma;
	s32 desc_size;
	struct clk *clk;

	struct mac_desc *rx_desc[RX_DESC_QUEUE_NUM];
	struct skb_info *rx_skb_info[RX_DESC_QUEUE_NUM];
	u32 rx_pos[RX_DESC_QUEUE_NUM];
	u32 rx_desc_num[RX_DESC_QUEUE_NUM];
	u32 rx_desc_buff_size;

	struct mac_desc *tx_desc;
	struct skb_info tx_temp_skb_info[TX_DESC_NUM];
	u32 tx_done_pos;
	u32 tx_pos;
	u32 tx_desc_full;

	u8 mac_addr[ETHERNET_MAC_ADDR_LEN];

	u8 cpu_port;
	u8 vlan_id;
	u8 vlan_memset;

	struct mii_bus *mii_bus;
	struct phy_device *phy_dev;
	int phy_irq[PHY_MAX_ADDR];

#ifndef INTERRUPT_IMMEDIATELY
	struct tasklet_struct rx_tasklet;
	struct tasklet_struct tx_tasklet;
#endif
	u32 int_status;

#ifdef RX_POLLING
	struct napi_struct napi;
#endif

#ifdef PHY_CONFIG
	struct device_node *phy_node;
#endif
};


#endif

