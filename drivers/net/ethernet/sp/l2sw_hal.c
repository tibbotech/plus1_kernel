#include "l2sw_hal.h"


static struct l2sw_reg* ls2w_reg_base = NULL;


int l2sw_reg_base_set(void __iomem *baseaddr)
{
	printk("@@@@@@@@@l2sw l2sw_reg_base_set =[%x]\n", (int)baseaddr);
	ls2w_reg_base = (struct l2sw_reg*)baseaddr;
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
	gpio_pin_mux_sel(PMX_DAISY_MODE,1);
}

void mac_hw_stop(struct l2sw_mac *mac)
{
	u32 reg;

	ls2w_reg_base->sw_int_mask_0 = 0xffffffff;
	ls2w_reg_base->sw_int_status_0 = 0xffffffff & (~MAC_INT_PSC);

	reg = ls2w_reg_base->cpu_cntl;
	ls2w_reg_base->cpu_cntl = DIS_PORT_TX | reg;

	reg = ls2w_reg_base->port_cntl0;
	ls2w_reg_base->port_cntl0 = DIS_PORT_RX | reg;
}

void mac_hw_reset(struct l2sw_mac *mac)
{
#if 0
	u32 reg;
	wmb();

	reg = ls2w_reg_base->mac_glb_sys_cfgcmd;
	ls2w_reg_base->mac_glb_sys_cfgcmd = reg | SOFT_RST_N;

	while ((reg & SOFT_RST_N) == 0x1){
		reg = ls2w_reg_base->mac_glb_sys_cfgcmd;
	}
#endif
}

void mac_hw_start(struct l2sw_mac *mac)
{
	u32 reg;

	//port 0: VLAN group 0
	//port 1: VLAN group 0
	HWREG_W(PVID_config0,(0<<4)+0);

	//VLAN group 0: soc0+port1+port0
	HWREG_W(VLAN_memset_config0,(0<<8)+0xb);

	//enable soc port0 crc padding
	reg=HWREG_R(cpu_cntl);
	HWREG_W(cpu_cntl,(reg&(~(0x1<<6)))|(0x1<<8));

	//enable port0
	reg=HWREG_R(port_cntl0);
	HWREG_W(port_cntl0,reg&(~(0x1<<24)));

	reg=HWREG_R(cpu_cntl);
	HWREG_W(cpu_cntl,reg&(~(0x3F<<0)));

	//tx_mib_counter_print();

	ls2w_reg_base->sw_int_mask_0 = 0x00000000;
}

void mac_hw_addr_set(struct l2sw_mac *mac)
{
	HWREG_W(w_mac_15_0_bus,mac->mac_addr[0]+(mac->mac_addr[1]<<8));
	HWREG_W(w_mac_47_16,mac->mac_addr[2]+(mac->mac_addr[3]<<8)+(mac->mac_addr[4]<<16)+(mac->mac_addr[5]<<24));
}

void mac_hw_init(struct l2sw_mac *mac)
{
	u32 reg;
	u8 port_map=0x0;
	u8 cpu_port=mac->cpu_port;
	u8 age=0x0;
	u8 proxy=0x0;
	u8 mc_ingress=0x0;

	/* descriptor base address */
	ls2w_reg_base->tx_hbase_addr_0 = mac->desc_dma;
	ls2w_reg_base->rx_lbase_addr_0 = mac->desc_dma + sizeof(struct mac_desc) * (TX_DESC_NUM + MAC_GUARD_DESC_NUM);
	ls2w_reg_base->rx_hbase_addr_0 = mac->desc_dma + sizeof(struct mac_desc) * (TX_DESC_NUM + MAC_GUARD_DESC_NUM + RX_QUEUE0_DESC_NUM);

	/* phy address */
	reg=HWREG_R(mac_force_mode);
	HWREG_W(mac_force_mode,(reg&(~(0x1f<<16)))|(PHY0_ADDR<<16));
	reg=HWREG_R(mac_force_mode);
	HWREG_W(mac_force_mode,(reg&(~(0x1f<<24)))|(PHY1_ADDR<<24));

	//enable soc port0 crc padding
	reg=HWREG_R(cpu_cntl);
	HWREG_W(cpu_cntl,(reg&(~(0x1<<6)))|(0x1<<8));

	//enable port0
	reg=HWREG_R(port_cntl0);
	HWREG_W(port_cntl0,reg&(~(0x3<<24)));

	//MAC address initial
	mac_hw_addr_set(mac);

	HWREG_W(wt_mac_ad0,(port_map<<12)+(cpu_port<<10)+(mac->vlan_id<<7)+(age<<4)+(proxy<<3)+(mc_ingress<<2)+0x1);
	reg=HWREG_R(wt_mac_ad0);
	while ((reg&(0x1<<1))==0x0) {
		printk("wt_mac_ad0 = [%x]\n", reg);
		reg=HWREG_R(wt_mac_ad0);
	}
}

u32 mac_hw_addr_update(struct l2sw_mac *mac)
{
	u32 regl, regh;
	u8  addr[6];

	regl = ls2w_reg_base->MAC_ad_ser0;
	regh = ls2w_reg_base->MAC_ad_ser1;
	addr[0] = regl & 0xFF;
	addr[1] = (regl >> 8) & 0xFF;
	addr[2] = regh & 0xFF;
	addr[3] = (regh >> 8) & 0xFF;
	addr[4] = (regh >> 16) & 0xFF;
	addr[5] = (regh >> 24) & 0xFF;

	if (is_valid_ether_addr(addr)) {
		memcpy(mac->mac_addr, addr, sizeof(addr));
		return 0;
	}

	return -1;
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
			printk("mdio operating...\n\r");
			time = 0;
		}

		value = HWREG_R(phy_cntl_reg1);
	} while ((value & 0x3)==0);

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

/*read or write a single register */
inline u32 reg_control(u8 mode, u32 reg, u32 value)
{
	if (mode == REG_WRITE) {
		switch (reg) {
		case MAC_SW_INT_MASK0:
			ls2w_reg_base->sw_int_mask_0 = value;
			break;

		case MAC_SW_INT_STATUS0:
			ls2w_reg_base->sw_int_status_0 = value;
			break;

		default:
			break;
		}

		return 0;
	}
	else if (mode == REG_READ) {
		switch (reg) {
		case MAC_PORT_ABILITY:
			reg = ls2w_reg_base->port_ability;
			break;

		case MAC_SW_INT_STATUS0:
			reg = ls2w_reg_base->sw_int_status_0;
			break;

		default:
			break;
		}

		return reg;
	}

	return 0;
}

void tx_mib_counter_print()
{
	DEBUG0("@@@@@@ls2w_reg_base_dump\n");
#if 1
	DEBUG0("sw_int_status_0     = %x\r\n",ls2w_reg_base->sw_int_status_0);
	DEBUG0("sw_int_mask_0       = %x\r\n",ls2w_reg_base->sw_int_mask_0);
	DEBUG0("fl_cntl_th          = %x\r\n",ls2w_reg_base->fl_cntl_th);
	DEBUG0("cpu_fl_cntl_th      = %x\r\n",ls2w_reg_base->cpu_fl_cntl_th);
	DEBUG0("pri_fl_cntl         = %x\r\n",ls2w_reg_base->pri_fl_cntl);
	DEBUG0("vlan_pri_th         = %x\r\n",ls2w_reg_base->vlan_pri_th);
	DEBUG0("En_tos_bus          = %x\r\n",ls2w_reg_base->En_tos_bus);
	DEBUG0("Tos_map0            = %x\r\n",ls2w_reg_base->Tos_map0);
	DEBUG0("Tos_map1            = %x\r\n",ls2w_reg_base->Tos_map1);
	DEBUG0("Tos_map2            = %x\r\n",ls2w_reg_base->Tos_map2);
	DEBUG0("Tos_map3            = %x\r\n",ls2w_reg_base->Tos_map3);
	DEBUG0("Tos_map4            = %x\r\n",ls2w_reg_base->Tos_map4);
	DEBUG0("Tos_map5            = %x\r\n",ls2w_reg_base->Tos_map5);
	DEBUG0("Tos_map6            = %x\r\n",ls2w_reg_base->Tos_map6);
	DEBUG0("Tos_map7            = %x\r\n",ls2w_reg_base->Tos_map7);
	DEBUG0("global_que_status   = %x\r\n",ls2w_reg_base->global_que_status);
	DEBUG0("addr_tbl_srch       = %x\r\n",ls2w_reg_base->addr_tbl_srch);
	DEBUG0("addr_tbl_st         = %x\r\n",ls2w_reg_base->addr_tbl_st);
	DEBUG0("MAC_ad_ser0         = %x\r\n",ls2w_reg_base->MAC_ad_ser0);
	DEBUG0("MAC_ad_ser1         = %x\r\n",ls2w_reg_base->MAC_ad_ser1);
	DEBUG0("wt_mac_ad0          = %x\r\n",ls2w_reg_base->wt_mac_ad0);
	DEBUG0("w_mac_15_0_bus      = %x\r\n",ls2w_reg_base->w_mac_15_0_bus);
	DEBUG0("w_mac_47_16         = %x\r\n",ls2w_reg_base->w_mac_47_16);
	DEBUG0("PVID_config0        = %x\r\n",ls2w_reg_base->PVID_config0);
	DEBUG0("PVID_config1        = %x\r\n",ls2w_reg_base->PVID_config1);
	DEBUG0("VLAN_memset_config0 = %x\r\n",ls2w_reg_base->VLAN_memset_config0);
	DEBUG0("VLAN_memset_config1 = %x\r\n",ls2w_reg_base->VLAN_memset_config1);
	DEBUG0("port_ability        = %x\r\n",ls2w_reg_base->port_ability);
	DEBUG0("port_st             = %x\r\n",ls2w_reg_base->port_st);
	DEBUG0("cpu_cntl            = %x\r\n",ls2w_reg_base->cpu_cntl);
	DEBUG0("port_cntl0          = %x\r\n",ls2w_reg_base->port_cntl0);
	DEBUG0("port_cntl1          = %x\r\n",ls2w_reg_base->port_cntl1);
	DEBUG0("port_cntl2          = %x\r\n",ls2w_reg_base->port_cntl2);
	DEBUG0("sw_glb_cntl         = %x\r\n",ls2w_reg_base->sw_glb_cntl);
	DEBUG0("l2sw_rsv1           = %x\r\n",ls2w_reg_base->l2sw_rsv1);
	DEBUG0("led_port0           = %x\r\n",ls2w_reg_base->led_port0);
	DEBUG0("led_port1           = %x\r\n",ls2w_reg_base->led_port1);
	DEBUG0("led_port2           = %x\r\n",ls2w_reg_base->led_port2);
	DEBUG0("led_port3           = %x\r\n",ls2w_reg_base->led_port3);
	DEBUG0("led_port4           = %x\r\n",ls2w_reg_base->led_port4);
	DEBUG0("watch_dog_trig_rst  = %x\r\n",ls2w_reg_base->watch_dog_trig_rst);
	DEBUG0("watch_dog_stop_cpu  = %x\r\n",ls2w_reg_base->watch_dog_stop_cpu);
	DEBUG0("phy_cntl_reg0       = %x\r\n",ls2w_reg_base->phy_cntl_reg0);
	DEBUG0("phy_cntl_reg1       = %x\r\n",ls2w_reg_base->phy_cntl_reg1);
	DEBUG0("mac_force_mode      = %x\r\n",ls2w_reg_base->mac_force_mode);
	DEBUG0("VLAN_group_config0  = %x\r\n",ls2w_reg_base->VLAN_group_config0);
	DEBUG0("VLAN_group_config1  = %x\r\n",ls2w_reg_base->VLAN_group_config1);
	DEBUG0("flow_ctrl_th3       = %x\r\n",ls2w_reg_base->flow_ctrl_th3);
	DEBUG0("queue_status_0      = %x\r\n",ls2w_reg_base->queue_status_0);
	DEBUG0("debug_cntl          = %x\r\n",ls2w_reg_base->debug_cntl);
	DEBUG0("queue_status_0      = %x\r\n",ls2w_reg_base->queue_status_0);
	DEBUG0("debug_cntl          = %x\r\n",ls2w_reg_base->debug_cntl);
	DEBUG0("l2sw_rsv2           = %x\r\n",ls2w_reg_base->l2sw_rsv2);
	DEBUG0("mem_test_info       = %x\r\n",ls2w_reg_base->mem_test_info);
	DEBUG0("sw_int_status_1     = %x\r\n",ls2w_reg_base->sw_int_status_1);
	DEBUG0("sw_int_mask_1       = %x\r\n",ls2w_reg_base->sw_int_mask_1);
	DEBUG0("cpu_tx_trig         = %x\r\n",ls2w_reg_base->cpu_tx_trig);
	DEBUG0("tx_hbase_addr_0     = %x\r\n",ls2w_reg_base->tx_hbase_addr_0);
	DEBUG0("tx_lbase_addr_0     = %x\r\n",ls2w_reg_base->tx_lbase_addr_0);
	DEBUG0("rx_hbase_addr_0     = %x\r\n",ls2w_reg_base->rx_hbase_addr_0);
	DEBUG0("rx_lbase_addr_0     = %x\r\n",ls2w_reg_base->rx_lbase_addr_0);
	DEBUG0("tx_hw_addr_0        = %x\r\n",ls2w_reg_base->tx_hw_addr_0);
	DEBUG0("tx_lw_addr_0        = %x\r\n",ls2w_reg_base->tx_lw_addr_0);
	DEBUG0("rx_hw_addr_0        = %x\r\n",ls2w_reg_base->rx_hw_addr_0);
	DEBUG0("rx_lw_addr_0        = %x\r\n",ls2w_reg_base->rx_lw_addr_0);
	DEBUG0("cpu_port_cntl_reg_0 = %x\r\n",ls2w_reg_base->cpu_port_cntl_reg_0);
#endif
}

void l2sw_enable_port(struct platform_device *pdev)
{
	u32 reg;
	volatile struct moon_regs * MOON5_REG = (volatile struct moon_regs *)devm_ioremap(&pdev->dev ,RF_GRP(5, 0), 32);

	//set clock
	reg = MOON5_REG->sft_cfg[5];
	MOON5_REG->sft_cfg[5] = (reg|0xF<<16|0xF);

	//enable port
	reg=HWREG_R(port_cntl0);
	HWREG_W(port_cntl0,reg&(~(0x3<<24)));

	//phy address
	reg=HWREG_R(mac_force_mode);
	HWREG_W(mac_force_mode,(reg&(~(0x1f<<16)))|(PHY0_ADDR<<16));
	reg=HWREG_R(mac_force_mode);
	HWREG_W(mac_force_mode,(reg&(~(0x1f<<24)))|(PHY1_ADDR<<24));
}

int phy_cfg()
{
	//mac_hw_addr_set(mac);
	return 0;
}
