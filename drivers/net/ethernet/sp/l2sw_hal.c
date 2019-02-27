#include "l2sw_hal.h"


static struct l2sw_reg* ls2w_reg_base = NULL;


int l2sw_reg_base_set(void __iomem *baseaddr)
{
	ls2w_reg_base = (struct l2sw_reg*)baseaddr;
	ETH_INFO("[%s] ls2w_reg_base =0x%08x\n", __FUNCTION__, (int)ls2w_reg_base);

	if (ls2w_reg_base == NULL){
		return -1;
	}
	else{
		return 0;
	}
}

void l2sw_pinmux_set()
{
	gpio_pin_mux_sel(PMX_L2SW_CLK_OUT,40);
	gpio_pin_mux_sel(PMX_L2SW_MAC_SMI_MDC,34);
	gpio_pin_mux_sel(PMX_L2SW_LED_FLASH0,35);
	gpio_pin_mux_sel(PMX_L2SW_LED_FLASH1,23);
	gpio_pin_mux_sel(PMX_L2SW_LED_ON0,44);
	gpio_pin_mux_sel(PMX_L2SW_LED_ON1,32);
	gpio_pin_mux_sel(PMX_L2SW_MAC_SMI_MDIO,33);
	gpio_pin_mux_sel(PMX_L2SW_P0_MAC_RMII_TXEN,43);
	gpio_pin_mux_sel(PMX_L2SW_P0_MAC_RMII_TXD0,41);
	gpio_pin_mux_sel(PMX_L2SW_P0_MAC_RMII_TXD1,42);
	gpio_pin_mux_sel(PMX_L2SW_P0_MAC_RMII_CRSDV,37);
	gpio_pin_mux_sel(PMX_L2SW_P0_MAC_RMII_RXD0,38);
	gpio_pin_mux_sel(PMX_L2SW_P0_MAC_RMII_RXD1,39);
	gpio_pin_mux_sel(PMX_L2SW_P0_MAC_RMII_RXER,36);
	gpio_pin_mux_sel(PMX_L2SW_P1_MAC_RMII_TXEN,31);
	gpio_pin_mux_sel(PMX_L2SW_P1_MAC_RMII_TXD0,29);
	gpio_pin_mux_sel(PMX_L2SW_P1_MAC_RMII_TXD1,30);
	gpio_pin_mux_sel(PMX_L2SW_P1_MAC_RMII_CRSDV,25);
	gpio_pin_mux_sel(PMX_L2SW_P1_MAC_RMII_RXD0,26);
	gpio_pin_mux_sel(PMX_L2SW_P1_MAC_RMII_RXD1,27);
	gpio_pin_mux_sel(PMX_L2SW_P1_MAC_RMII_RXER,24);
}

void mac_hw_stop(void)
{
	u32 reg;

	ls2w_reg_base->sw_int_mask_0 = 0xffffffff;
	ls2w_reg_base->sw_int_status_0 = 0xffffffff & (~MAC_INT_PORT_ST_CHG);

	reg = ls2w_reg_base->cpu_cntl;
	ls2w_reg_base->cpu_cntl = (0x3<<6) | reg;       // Disable cpu0 and cpu 1.

	reg = ls2w_reg_base->port_cntl0;
	ls2w_reg_base->port_cntl0 = (0x3<<24) | reg;    // Disable port 0 and port 1.
}

void mac_hw_reset(struct l2sw_mac *mac)
{
}

void mac_hw_start(void)
{
	u32 reg;

#ifdef CONFIG_DUAL_NIC
	//port 0: VLAN group 0
	//port 1: VLAN group 1
	HWREG_W(PVID_config0, (1<<4)+0);

	//VLAN group 0: cpu0+port0
	//VLAN group 1: cpu0+port1
	HWREG_W(VLAN_memset_config0, (0xa<<8)+0x9);
#else
	//port 0: VLAN group 0
	//port 1: VLAN group 0
	HWREG_W(PVID_config0, (0<<4)+0);

	//VLAN group 0: cpu0+port1+port0
	HWREG_W(VLAN_memset_config0, (0x0<<8)+0xb);
#endif

	//enable cpu port 0 (6) & port 0 crc padding (8)
	reg = HWREG_R(cpu_cntl);
	HWREG_W(cpu_cntl, (reg & (~(0x1<<6))) | (0x1<<8));

	//enable lan port0 & port 1
	reg = HWREG_R(port_cntl0);
	HWREG_W(port_cntl0, reg & (~(0x3<<24)));

	//regs_print();
}

void mac_hw_addr_set(struct l2sw_mac *mac)
{
	u32 reg;

	HWREG_W(w_mac_15_0, mac->mac_addr[0]+(mac->mac_addr[1]<<8));
	HWREG_W(w_mac_47_16, mac->mac_addr[2]+(mac->mac_addr[3]<<8)+(mac->mac_addr[4]<<16)+(mac->mac_addr[5]<<24));

	HWREG_W(wt_mac_ad0, (mac->cpu_port<<10)+(mac->vlan_id<<7)+(1<<4)+0x1);  // Set aging=1
	do {
		reg = HWREG_R(wt_mac_ad0);
		udelay(1);
		//ETH_INFO(" wt_mac_ad0 = 0x%08x\n", reg);
	} while ((reg&(0x1<<1)) == 0x0);
	//ETH_INFO(" mac_ad0 = %08x, mac_ad = %08x%04x\n", HWREG_R(wt_mac_ad0), HWREG_R(w_mac_47_16), HWREG_R(w_mac_15_0)&0xffff);

	//mac_hw_addr_print();
}

void mac_hw_addr_del(struct l2sw_mac *mac)
{
	u32 reg;

	HWREG_W(w_mac_15_0, mac->mac_addr[0]+(mac->mac_addr[1]<<8));
	HWREG_W(w_mac_47_16, mac->mac_addr[2]+(mac->mac_addr[3]<<8)+(mac->mac_addr[4]<<16)+(mac->mac_addr[5]<<24));

	HWREG_W(wt_mac_ad0,(0x1<<12)+(mac->vlan_id<<7)+0x1);
	do {
		reg = HWREG_R(wt_mac_ad0);
		udelay(1);
		//ETH_INFO(" wt_mac_ad0 = 0x%08x\n", reg);
	} while ((reg&(0x1<<1)) == 0x0);
	//ETH_INFO(" mac_ad0 = %08x, mac_ad = %08x%04x\n", HWREG_R(wt_mac_ad0), HWREG_R(w_mac_47_16), HWREG_R(w_mac_15_0)&0xffff);

	//mac_hw_addr_print();
}

#if 0
void mac_hw_addr_print(void)
{
	u32 reg, regl, regh;

	// Wait for address table being idle.
	do {
		reg = HWREG_R(addr_tbl_srch);
		udelay(1);
	} while (!(reg & MAC_ADDR_LOOKUP_IDLE));

	// Search address table from start.
	HWREG_W(addr_tbl_srch, HWREG_R(addr_tbl_srch) | MAC_BEGIN_SEARCH_ADDR);
	mb();
	while (1){
		do {
			reg = HWREG_R(addr_tbl_st);
			udelay(1);
			//ETH_INFO(" addr_tbl_st = %08x\n", reg);
		} while (!(reg & (MAC_AT_TABLE_END|MAC_AT_DATA_READY)));

		if (reg & MAC_AT_TABLE_END) {
			break;
		}

		regl = HWREG_R(MAC_ad_ser0);
		ndelay(10);                     // delay here is necessary or you will get all 0 of MAC_ad_ser1.
		regh = HWREG_R(MAC_ad_ser1);

		//ETH_INFO(" addr_tbl_st = %08x\n", reg);
		ETH_INFO(" AT #%u: port=0x%01x, cpu=0x%01x, vid=%u, aging=%u, proxy=%u, mc_ingress=%u,"
			" HWaddr=%02x:%02x:%02x:%02x:%02x:%02x\n",
			(reg>>22)&0x3ff, (reg>>12)&0x3, (reg>>10)&0x3, (reg>>7)&0x7,
			(reg>>4)&0x7, (reg>>3)&0x1, (reg>>2)&0x1,
			regl&0xff, (regl>>8)&0xff,
			regh&0xff, (regh>>8)&0xff, (regh>>16)&0xff, (regh>>24)&0xff);

		// Search next.
		HWREG_W(addr_tbl_srch, HWREG_R(addr_tbl_srch) | MAC_SEARCH_NEXT_ADDR);
	}
}
#endif

void mac_hw_init(struct l2sw_mac *mac)
{
	u32 reg;

	/* descriptor base address */
	ls2w_reg_base->tx_hbase_addr_0 = mac->mac_comm->desc_dma;
	ls2w_reg_base->rx_lbase_addr_0 = mac->mac_comm->desc_dma + sizeof(struct mac_desc) * (TX_DESC_NUM + MAC_GUARD_DESC_NUM);
	ls2w_reg_base->rx_hbase_addr_0 = mac->mac_comm->desc_dma + sizeof(struct mac_desc) * (TX_DESC_NUM + MAC_GUARD_DESC_NUM + RX_QUEUE0_DESC_NUM);

	/* phy address */
	reg = HWREG_R(mac_force_mode);
	HWREG_W(mac_force_mode, (reg & (~(0x1f<<16))) | (PHY0_ADDR<<16));
	reg = HWREG_R(mac_force_mode);
	HWREG_W(mac_force_mode, (reg & (~(0x1f<<24))) | (PHY1_ADDR<<24));

	//disable cpu port0 aging (12)
	//disable cpu port0 learning (14)
	//enable UC and MC packets
	reg = HWREG_R(cpu_cntl);
	HWREG_W(cpu_cntl, (reg & (~((0x1<<14)|(0x3c<<0)))) | (0x1<<12));

#ifdef CONFIG_DUAL_NIC
	//RMC forward: to cpu
	//LED: 60mS
	//BC storm prev: 31 BC
	reg = HWREG_R(sw_glb_cntl);
	HWREG_W(sw_glb_cntl, (reg & (~((0x3<<25)|(0x3<<23)|(0x3<<4)))) | (0x1<<25)|(0x1<<23)|(0x1<<4));
#else
	//RMC forward: broadcast
	//LED: 60mS
	//BC storm prev: 31 BC
	reg = HWREG_R(sw_glb_cntl);
	HWREG_W(sw_glb_cntl, (reg & (~((0x3<<25)|(0x3<<23)|(0x3<<4)))) | (0x0<<25)|(0x1<<23)|(0x1<<4));
#endif
	ls2w_reg_base->sw_int_mask_0 = 0x00000000;
}

void rx_mode_set(struct l2sw_mac *mac)
{
	struct net_device *netdev;

	netdev = mac->net_dev;
	if (netdev == NULL) {
		return;
	}
}

#define MDIO_READ_CMD   0x02
#define MDIO_WRITE_CMD  0x01


static int mdio_access(u8 op_cd, u8 dev_reg_addr, u8 phy_addr, u32 wdata){
	u32 value, time=0;

	HWREG_W(phy_cntl_reg0, (wdata << 16) | (op_cd << 13) | (dev_reg_addr << 8) | phy_addr);
	do {
		if (++time > MDIO_RW_TIMEOUT_RETRY_NUMBERS) {
			ETH_ERR("mdio failed to operate!\n");
			time = 0;
		}

		value = HWREG_R(phy_cntl_reg1);
	} while ((value & 0x3) == 0);

	if (time == 0)
		return -1;

	return value >> 16;
}

u32 mdio_read(u32 phy_id, u16 regnum)
{
	int sdVal = 0;

	sdVal = mdio_access(MDIO_READ_CMD,regnum,phy_id,0);
	if (sdVal < 0)
		return -EIO;

	return sdVal;
}

u32 mdio_write(u32 phy_id, u32 regnum, u16 val)
{
	int sdVal = 0;

	sdVal = mdio_access(MDIO_WRITE_CMD,regnum,phy_id,val);
	if (sdVal < 0)
		return -EIO;

	return 0;
}

inline void rx_finished(u32 queue, u32 rx_count)
{
}

inline void tx_trigger(u32 tx_pos)
{
	u32 reg;

	reg = ls2w_reg_base->tx_lbase_addr_0;
	reg &= ~(0xff << 16);
	reg |= tx_pos << 16;
	ls2w_reg_base->tx_lbase_addr_0 = reg;

	HWREG_W(cpu_tx_trig,(0x1<<0));
}

inline void write_sw_int_mask0(u32 value)
{
	ls2w_reg_base->sw_int_mask_0 = value;
}

inline void write_sw_int_status0(u32 value)
{
	ls2w_reg_base->sw_int_status_0 = value;
}

inline u32 read_sw_int_status0(void)
{
	return ls2w_reg_base->sw_int_status_0;
}

inline u32 read_port_ability(void)
{
	return ls2w_reg_base->port_ability;
}

void l2sw_enable_port(struct platform_device *pdev)
{
	u32 reg;
	volatile struct moon_regs * MOON5_REG = (volatile struct moon_regs *)devm_ioremap(&pdev->dev ,RF_GRP(5, 0), 32);

	//set clock
	reg = MOON5_REG->sft_cfg[5];
	MOON5_REG->sft_cfg[5] = (reg|0xF<<16|0xF);

	//phy address
	reg = HWREG_R(mac_force_mode);
	HWREG_W(mac_force_mode, (reg & (~(0x1f<<16))) | (PHY0_ADDR<<16));
	reg = HWREG_R(mac_force_mode);
	HWREG_W(mac_force_mode, (reg & (~(0x1f<<24))) | (PHY1_ADDR<<24));
}

int phy_cfg()
{
	//mac_hw_addr_set(mac);
	return 0;
}


#if 0
void regs_print()
{
	ETH_INFO("sw_int_status_0     = %08x\n", ls2w_reg_base->sw_int_status_0);
	ETH_INFO("sw_int_mask_0       = %08x\n", ls2w_reg_base->sw_int_mask_0);
	ETH_INFO("fl_cntl_th          = %08x\n", ls2w_reg_base->fl_cntl_th);
	ETH_INFO("cpu_fl_cntl_th      = %08x\n", ls2w_reg_base->cpu_fl_cntl_th);
	ETH_INFO("pri_fl_cntl         = %08x\n", ls2w_reg_base->pri_fl_cntl);
	ETH_INFO("vlan_pri_th         = %08x\n", ls2w_reg_base->vlan_pri_th);
	ETH_INFO("En_tos_bus          = %08x\n", ls2w_reg_base->En_tos_bus);
	ETH_INFO("Tos_map0            = %08x\n", ls2w_reg_base->Tos_map0);
	ETH_INFO("Tos_map1            = %08x\n", ls2w_reg_base->Tos_map1);
	ETH_INFO("Tos_map2            = %08x\n", ls2w_reg_base->Tos_map2);
	ETH_INFO("Tos_map3            = %08x\n", ls2w_reg_base->Tos_map3);
	ETH_INFO("Tos_map4            = %08x\n", ls2w_reg_base->Tos_map4);
	ETH_INFO("Tos_map5            = %08x\n", ls2w_reg_base->Tos_map5);
	ETH_INFO("Tos_map6            = %08x\n", ls2w_reg_base->Tos_map6);
	ETH_INFO("Tos_map7            = %08x\n", ls2w_reg_base->Tos_map7);
	ETH_INFO("global_que_status   = %08x\n", ls2w_reg_base->global_que_status);
	ETH_INFO("addr_tbl_srch       = %08x\n", ls2w_reg_base->addr_tbl_srch);
	ETH_INFO("addr_tbl_st         = %08x\n", ls2w_reg_base->addr_tbl_st);
	ETH_INFO("MAC_ad_ser0         = %08x\n", ls2w_reg_base->MAC_ad_ser0);
	ETH_INFO("MAC_ad_ser1         = %08x\n", ls2w_reg_base->MAC_ad_ser1);
	ETH_INFO("wt_mac_ad0          = %08x\n", ls2w_reg_base->wt_mac_ad0);
	ETH_INFO("w_mac_15_0          = %08x\n", ls2w_reg_base->w_mac_15_0);
	ETH_INFO("w_mac_47_16         = %08x\n", ls2w_reg_base->w_mac_47_16);
	ETH_INFO("PVID_config0        = %08x\n", ls2w_reg_base->PVID_config0);
	ETH_INFO("PVID_config1        = %08x\n", ls2w_reg_base->PVID_config1);
	ETH_INFO("VLAN_memset_config0 = %08x\n", ls2w_reg_base->VLAN_memset_config0);
	ETH_INFO("VLAN_memset_config1 = %08x\n", ls2w_reg_base->VLAN_memset_config1);
	ETH_INFO("port_ability        = %08x\n", ls2w_reg_base->port_ability);
	ETH_INFO("port_st             = %08x\n", ls2w_reg_base->port_st);
	ETH_INFO("cpu_cntl            = %08x\n", ls2w_reg_base->cpu_cntl);
	ETH_INFO("port_cntl0          = %08x\n", ls2w_reg_base->port_cntl0);
	ETH_INFO("port_cntl1          = %08x\n", ls2w_reg_base->port_cntl1);
	ETH_INFO("port_cntl2          = %08x\n", ls2w_reg_base->port_cntl2);
	ETH_INFO("sw_glb_cntl         = %08x\n", ls2w_reg_base->sw_glb_cntl);
	ETH_INFO("l2sw_rsv1           = %08x\n", ls2w_reg_base->l2sw_rsv1);
	ETH_INFO("led_port0           = %08x\n", ls2w_reg_base->led_port0);
	ETH_INFO("led_port1           = %08x\n", ls2w_reg_base->led_port1);
	ETH_INFO("led_port2           = %08x\n", ls2w_reg_base->led_port2);
	ETH_INFO("led_port3           = %08x\n", ls2w_reg_base->led_port3);
	ETH_INFO("led_port4           = %08x\n", ls2w_reg_base->led_port4);
	ETH_INFO("watch_dog_trig_rst  = %08x\n", ls2w_reg_base->watch_dog_trig_rst);
	ETH_INFO("watch_dog_stop_cpu  = %08x\n", ls2w_reg_base->watch_dog_stop_cpu);
	ETH_INFO("phy_cntl_reg0       = %08x\n", ls2w_reg_base->phy_cntl_reg0);
	ETH_INFO("phy_cntl_reg1       = %08x\n", ls2w_reg_base->phy_cntl_reg1);
	ETH_INFO("mac_force_mode      = %08x\n", ls2w_reg_base->mac_force_mode);
	ETH_INFO("VLAN_group_config0  = %08x\n", ls2w_reg_base->VLAN_group_config0);
	ETH_INFO("VLAN_group_config1  = %08x\n", ls2w_reg_base->VLAN_group_config1);
	ETH_INFO("flow_ctrl_th3       = %08x\n", ls2w_reg_base->flow_ctrl_th3);
	ETH_INFO("queue_status_0      = %08x\n", ls2w_reg_base->queue_status_0);
	ETH_INFO("debug_cntl          = %08x\n", ls2w_reg_base->debug_cntl);
	ETH_INFO("queue_status_0      = %08x\n", ls2w_reg_base->queue_status_0);
	ETH_INFO("debug_cntl          = %08x\n", ls2w_reg_base->debug_cntl);
	ETH_INFO("l2sw_rsv2           = %08x\n", ls2w_reg_base->l2sw_rsv2);
	ETH_INFO("mem_test_info       = %08x\n", ls2w_reg_base->mem_test_info);
	ETH_INFO("sw_int_status_1     = %08x\n", ls2w_reg_base->sw_int_status_1);
	ETH_INFO("sw_int_mask_1       = %08x\n", ls2w_reg_base->sw_int_mask_1);
	ETH_INFO("cpu_tx_trig         = %08x\n", ls2w_reg_base->cpu_tx_trig);
	ETH_INFO("tx_hbase_addr_0     = %08x\n", ls2w_reg_base->tx_hbase_addr_0);
	ETH_INFO("tx_lbase_addr_0     = %08x\n", ls2w_reg_base->tx_lbase_addr_0);
	ETH_INFO("rx_hbase_addr_0     = %08x\n", ls2w_reg_base->rx_hbase_addr_0);
	ETH_INFO("rx_lbase_addr_0     = %08x\n", ls2w_reg_base->rx_lbase_addr_0);
	ETH_INFO("tx_hw_addr_0        = %08x\n", ls2w_reg_base->tx_hw_addr_0);
	ETH_INFO("tx_lw_addr_0        = %08x\n", ls2w_reg_base->tx_lw_addr_0);
	ETH_INFO("rx_hw_addr_0        = %08x\n", ls2w_reg_base->rx_hw_addr_0);
	ETH_INFO("rx_lw_addr_0        = %08x\n", ls2w_reg_base->rx_lw_addr_0);
	ETH_INFO("cpu_port_cntl_reg_0 = %08x\n", ls2w_reg_base->cpu_port_cntl_reg_0);
}
#endif
