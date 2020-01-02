/*
 * How to test RTC:
 *
 * hwclock - query and set the hardware clock (RTC)
 *
 * (for i in `seq 5`; do (echo ------ && echo -n 'date      : ' && date && echo -n 'hwclock -r: ' && hwclock -r; sleep 1); done)
 * date 121209002014 # Set system to 2014/Dec/12 09:00
 * (for i in `seq 5`; do (echo ------ && echo -n 'date      : ' && date && echo -n 'hwclock -r: ' && hwclock -r; sleep 1); done)
 * hwclock -s # Set the System Time from the Hardware Clock
 * (for i in `seq 5`; do (echo ------ && echo -n 'date      : ' && date && echo -n 'hwclock -r: ' && hwclock -r; sleep 1); done)
 * date 121213002014 # Set system to 2014/Dec/12 13:00
 * (for i in `seq 5`; do (echo ------ && echo -n 'date      : ' && date && echo -n 'hwclock -r: ' && hwclock -r; sleep 1); done)
 * hwclock -w # Set the Hardware Clock to the current System Time
 * (for i in `seq 10000`; do (echo ------ && echo -n 'date      : ' && date && echo -n 'hwclock -r: ' && hwclock -r; sleep 1); done)
 *
 *
 * How to setup alarm (e.g., 10 sec later):
 *     echo 0 > /sys/class/rtc/rtc0/wakealarm && \
 *     nnn=`date '+%s'` && echo $nnn && nnn=`expr $nnn + 10` && echo $nnn > /sys/class/rtc/rtc0/wakealarm
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/version.h>


/* ---------------------------------------------------------------------------------------------- */
#define RTC_INFO(fmt, args ...) printk(KERN_INFO "[RTC] Info: " fmt, ##args)
#define RTC_ERR(fmt, args ...)  printk(KERN_ERR  "[RTC] Error: " fmt, ##args)
#define RTC_DBG(fmt, args ...)  pr_debug(fmt, ## args)

/* ---------------------------------------------------------------------------------------------- */
struct sunplus_rtc {
	void __iomem *reg_base;
	struct clk *clkc;
	struct reset_control *rstc;
	u32 charging_mode;
};

struct sunplus_rtc_reg {
	volatile u32 rsv00;
	volatile u32 rsv01;
	volatile u32 rsv02;
	volatile u32 rsv03;
	volatile u32 rsv04;
	volatile u32 rsv05;
	volatile u32 rsv06;
	volatile u32 rsv07;
	volatile u32 rsv08;
	volatile u32 rsv09;
	volatile u32 rsv10;
	volatile u32 rsv11;
	volatile u32 rsv12;
	volatile u32 rsv13;
	volatile u32 rsv14;
	volatile u32 rsv15;
	volatile u32 rtc_ctrl;
	volatile u32 rtc_timer_out;
	volatile u32 rtc_divider;
	volatile u32 rtc_timer_set;
	volatile u32 rtc_alarm_set;
	volatile u32 rtc_user_data;
	volatile u32 rtc_reset_record;
	volatile u32 rtc_battery_ctrl;
	volatile u32 rtc_trim_ctrl;
	volatile u32 rsv25;
	volatile u32 rsv26;
	volatile u32 rsv27;
	volatile u32 rsv28;
	volatile u32 rsv29;
	volatile u32 rsv30;
	volatile u32 rsv31;
};


static int sp_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct sunplus_rtc *sp_rtc = dev_get_drvdata(dev);
	struct sunplus_rtc_reg *sp_rtc_reg = sp_rtc->reg_base;
	unsigned long secs;

	RTC_DBG("RTC date/time to %d-%d-%d, %02d:%02d:%02d\n",
		tm->tm_mday, tm->tm_mon+1, tm->tm_year+1900, tm->tm_hour, tm->tm_min, tm->tm_sec);

	secs = rtc_tm_to_time64(tm);
	sp_rtc_reg->rtc_timer_set = (u32)(secs);

	return 0;
}

static int sp_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct sunplus_rtc *sp_rtc = dev_get_drvdata(dev);
	struct sunplus_rtc_reg *sp_rtc_reg = sp_rtc->reg_base;
	unsigned long secs;

	secs = (unsigned long)(sp_rtc_reg->rtc_timer_out);
	rtc_time_to_tm(secs, tm);

	RTC_DBG("RTC date/time to %d-%d-%d, %02d:%02d:%02d\n",
		tm->tm_mday, tm->tm_mon+1, tm->tm_year+1900, tm->tm_hour, tm->tm_min, tm->tm_sec);

	return rtc_valid_tm(tm);
}

#if 0
int sp_rtc_get_time(struct rtc_time *tm)
{
	unsigned long secs;

	secs = (unsigned long)(sp_rtc_reg->rtc_timer_out);
	rtc_time_to_tm(secs, tm);

	return 0;
}
EXPORT_SYMBOL(sp_rtc_get_time);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
static int sp_rtc_set_mmss(struct device *dev, unsigned long secs)
{
	struct sunplus_rtc *sp_rtc = dev_get_drvdata(dev);
	struct sunplus_rtc_reg *sp_rtc_reg = sp_rtc->reg_base;

	RTC_DBG("secs = %lu\n", secs);
	sp_rtc_reg->rtc_timer_set = (u32)(secs);
	return 0;
}
#endif

static int sp_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sunplus_rtc *sp_rtc = dev_get_drvdata(dev);
	struct sunplus_rtc_reg *sp_rtc_reg = sp_rtc->reg_base;
	unsigned long alarm_time;

	alarm_time = rtc_tm_to_time64(&alrm->time);
	RTC_DBG("alarm_time: %u\n", (u32)alarm_time);

	if (alarm_time > 0xFFFFFFFF)
		return -EINVAL;

	sp_rtc_reg->rtc_alarm_set = (u32)alarm_time;
	wmb();
	sp_rtc_reg->rtc_ctrl = (0x003F << 16) | 0x0017;

	return 0;
}

static int sp_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sunplus_rtc *sp_rtc = dev_get_drvdata(dev);
	struct sunplus_rtc_reg *sp_rtc_reg = sp_rtc->reg_base;
	u32 alarm_time;

	alarm_time = sp_rtc_reg->rtc_alarm_set;
	RTC_DBG("alarm_time: %u\n", alarm_time);
	rtc_time64_to_tm((unsigned long)alarm_time, &alrm->time);

	return 0;
}

static const struct rtc_class_ops sp_rtc_ops = {
	.set_time  = sp_rtc_set_time,
	.read_time = sp_rtc_read_time,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
	.set_mmss  = sp_rtc_set_mmss,
#endif
	.set_alarm  = sp_rtc_set_alarm,
	.read_alarm = sp_rtc_read_alarm,
};

static irqreturn_t rtc_irq_handler(int irq, void *data)
{
	RTC_INFO("[RTC] alarm times up\n");

	return IRQ_HANDLED;
}

/*      mode    bat_charge_rsel         bat_charge_dsel         bat_charge_en
	0xE     x                       x                       0                       Disable
	0x1     0                       0                       1                       0.86mA (2K Ohm with diode)
	0x5     1                       0                       1                       1.81mA (250 Ohm with diode)
	0x9     2                       0                       1                       2.07mA (50 Ohm with diode)
	0xD     3                       0                       1                       16.0mA (0 Ohm with diode)
	0x3     0                       1                       1                       1.36mA (2K Ohm without diode)
	0x7     1                       1                       1                       3.99mA (250 Ohm without diode)
	0xB     2                       1                       1                       4.41mA (50 Ohm without diode)
	0xF     3                       1                       1                       16.0mA (0 Ohm without diode)
*/
static int sp_rtc_probe(struct platform_device *plat_dev)
{
	int ret;
	int err, irq;
	struct resource *res;
	struct sunplus_rtc *sp_rtc;
	struct sunplus_rtc_reg *sp_rtc_reg;
	struct rtc_device *rtc;

	sp_rtc = devm_kzalloc(&plat_dev->dev, sizeof(*sp_rtc), GFP_KERNEL);
	if (sp_rtc == NULL) {
		dev_err(&plat_dev->dev,"Failed to allocate memory!\n");
		return -ENOMEM;
	}
	platform_set_drvdata(plat_dev, sp_rtc);

	// find and map our resources
	res = platform_get_resource_byname(plat_dev, IORESOURCE_MEM, "rtc_reg");
	RTC_DBG("res = %llx\n",(u64)res->start);
	if (res) {
		sp_rtc->reg_base = devm_ioremap_resource(&plat_dev->dev, res);
		if (IS_ERR(sp_rtc->reg_base)) {
			dev_err(&plat_dev->dev,"devm_ioremap_resource failed!\n");
		}
	}
	RTC_INFO("reg_base = %px\n", sp_rtc->reg_base);

	// clk
	sp_rtc->clkc = devm_clk_get(&plat_dev->dev,NULL);
	RTC_DBG("sp_rtc->clkc = %px\n",sp_rtc->clkc);
	if(IS_ERR(sp_rtc->clkc)) {
		dev_err(&plat_dev->dev, "devm_clk_get failed!\n");
	}
	ret = clk_prepare_enable(sp_rtc->clkc);

	// reset
	sp_rtc->rstc = devm_reset_control_get(&plat_dev->dev, NULL);
	RTC_DBG( "sp_rtc->rstc = %px \n",sp_rtc->rstc);
	if (IS_ERR(sp_rtc->rstc)) {
		ret = PTR_ERR(sp_rtc->rstc);
		dev_err(&plat_dev->dev, "RTC failed to retrieve reset controller: %d\n", ret);
		goto free_clk;
	}
	ret = reset_control_deassert(sp_rtc->rstc);
	if (ret) {
		goto free_clk;
	}

	sp_rtc_reg = (struct sunplus_rtc_reg*)sp_rtc->reg_base;
	sp_rtc_reg->rtc_ctrl = (1 << (16+4)) | (1 << 4);        /* Keep RTC from system reset */

	// request irq
	irq = platform_get_irq(plat_dev, 0);
	if (irq < 0) {
		RTC_ERR("platform_get_irq failed!\n");
		goto free_reset_assert;
	}

	err = devm_request_irq(&plat_dev->dev, irq, rtc_irq_handler, IRQF_TRIGGER_RISING, "rtc_irq", plat_dev);
	if (err) {
		RTC_ERR("devm_request_irq failed: %d\n", err);
		goto free_reset_assert;
	}

	// Get charging-mode.
	ret = of_property_read_u32(plat_dev->dev.of_node, "charging-mode", &sp_rtc->charging_mode);
	if (ret) {
		RTC_ERR("Failed to retrieve 'charging-mode'!\n");
		goto free_reset_assert;
	}

	RTC_DBG("Battery charging mode: 0x%X\n", sp_rtc->charging_mode);
	sp_rtc_reg->rtc_battery_ctrl = (0x000F << 16) | (sp_rtc->charging_mode & 0x0F);

	device_init_wakeup(&plat_dev->dev, 1);

	rtc = devm_rtc_device_register(&plat_dev->dev, "sp7021-rtc", &sp_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		ret = PTR_ERR(rtc);
		goto free_reset_assert;
	}
	rtc->uie_unsupported = 1;
	return 0;


free_reset_assert:
	reset_control_assert(sp_rtc->rstc);

free_clk:
	clk_disable_unprepare(sp_rtc->clkc);
	return ret;
}

static int sp_rtc_remove(struct platform_device *plat_dev)
{
	struct sunplus_rtc *sp_rtc = platform_get_drvdata(plat_dev);

	reset_control_assert(sp_rtc->rstc);
	return 0;
}

static int sp_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct sunplus_rtc *sp_rtc = platform_get_drvdata(pdev);
	struct sunplus_rtc_reg *sp_rtc_reg = sp_rtc->reg_base;

	sp_rtc_reg->rtc_ctrl = (1 << (16+4)) | (1 << 4);        /* Keep RTC from system reset */
	return 0;
}

static int sp_rtc_resume(struct platform_device *pdev)
{
	struct sunplus_rtc *sp_rtc = platform_get_drvdata(pdev);
	struct sunplus_rtc_reg *sp_rtc_reg = sp_rtc->reg_base;

	/*
	 * Because RTC is still powered during suspend,
	 * there is nothing to do here.
	 */
	sp_rtc_reg->rtc_ctrl = (1 << (16+4)) | (1 << 4);        /* Keep RTC from system reset */
	return 0;
}

static const struct of_device_id sp_rtc_of_match[] = {
	{ .compatible = "sunplus,sp7021-rtc" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sp_rtc_of_match);

static struct platform_driver sp_rtc_driver = {
	.probe   = sp_rtc_probe,
	.remove  = sp_rtc_remove,
	.suspend = sp_rtc_suspend,
	.resume  = sp_rtc_resume,
	.driver  = {
		.name = "sp7021-rtc",
		.owner = THIS_MODULE,
		.of_match_table = sp_rtc_of_match,
	},
};

static int __init sp_rtc_init(void)
{
	int err;

	if ((err = platform_driver_register(&sp_rtc_driver)) != 0)
		return err;

	return 0;
}

static void __exit sp_rtc_exit(void)
{
	platform_driver_unregister(&sp_rtc_driver);
}

MODULE_AUTHOR("Sunplus");
MODULE_DESCRIPTION("Sunplus RTC driver");
MODULE_LICENSE("GPL");

module_init(sp_rtc_init);
module_exit(sp_rtc_exit);
