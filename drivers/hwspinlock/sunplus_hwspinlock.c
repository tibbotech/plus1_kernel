#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "hwspinlock_internal.h"

#define SP_MUTEX_NUM_LOCKS	16

/* Possible values of SPINLOCK_LOCK_REG */
#define SPINLOCK_UNLOCKED		(0)	/* free */
#define SPINLOCK_LOCKED			(1)	/* locked */

#define RESET_SEMAPHORE			(0x554E4C4B)	/* free */

struct sp_hwspinlock {
	struct clk *clk;
	struct hwspinlock_device bank;
};


static int sp_hwspinlock_trylock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	/* attempt to acquire the lock by reading its value */
	return (SPINLOCK_UNLOCKED == readl(lock_addr));
}

static void sp_hwspinlock_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	/* release the lock by writing magic data to it */
	writel(RESET_SEMAPHORE, lock_addr);
}


static void sp_hwspinlock_relax(struct hwspinlock *lock)
{
	ndelay(50);
}

static const struct hwspinlock_ops sp_hwspinlock_ops = {
	.trylock = sp_hwspinlock_trylock,
	.unlock = sp_hwspinlock_unlock,
	.relax = sp_hwspinlock_relax,
};

static int sp_hwspinlock_probe(struct platform_device *pdev)
{

	struct sp_hwspinlock *hw;
	void __iomem *io_base;
	size_t array_size;
	int i, ret;

	io_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(io_base))
		return PTR_ERR(io_base);

	array_size = SP_MUTEX_NUM_LOCKS * sizeof(struct hwspinlock);
	hw = devm_kzalloc(&pdev->dev, sizeof(*hw) + array_size, GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	hw->clk = devm_clk_get(&pdev->dev, "hsem");

	if (IS_ERR(hw->clk))
		return PTR_ERR(hw->clk);

	for (i = 0; i < SP_MUTEX_NUM_LOCKS; i++)
		hw->bank.lock[i].priv = io_base + i * sizeof(u32);

	platform_set_drvdata(pdev, hw);
	pm_runtime_enable(&pdev->dev);

	ret = hwspin_lock_register(&hw->bank, &pdev->dev, &sp_hwspinlock_ops,
				   0, SP_MUTEX_NUM_LOCKS);

	if (ret)
		pm_runtime_disable(&pdev->dev);

	return ret;
}

static int sp_hwspinlock_remove(struct platform_device *pdev)
{
	struct sp_hwspinlock *hw = platform_get_drvdata(pdev);
	int ret;

	ret = hwspin_lock_unregister(&hw->bank);
	if (ret)
		dev_err(&pdev->dev, "%s failed: %d\n", __func__, ret);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static int __maybe_unused sp_hwspinlock_runtime_suspend(struct device *dev)
{
	struct sp_hwspinlock *hw = dev_get_drvdata(dev);

	clk_disable_unprepare(hw->clk);

	return 0;
}

static int __maybe_unused sp_hwspinlock_runtime_resume(struct device *dev)
{
	struct sp_hwspinlock *hw = dev_get_drvdata(dev);

	clk_prepare_enable(hw->clk);

	return 0;
}

static const struct dev_pm_ops sp_hwspinlock_pm_ops = {
	SET_RUNTIME_PM_OPS(sp_hwspinlock_runtime_suspend,
			   sp_hwspinlock_runtime_resume,
			   NULL)
};
static const struct of_device_id sp_hwspinlock_of_match[] = {
	{ .compatible = "sp,sunplus-hwspinlock", },
	{ /* end */ },
};
MODULE_DEVICE_TABLE(of, sp_hwspinlock_of_match);

static struct platform_driver sp_hwspinlock_driver = {
	.probe		= sp_hwspinlock_probe,
	.remove		= sp_hwspinlock_remove,
	.driver		= {
		.name	= "sp_hwspinlock",
		.of_match_table = of_match_ptr(sp_hwspinlock_of_match),
		.pm	= &sp_hwspinlock_pm_ops,
	},
};

static int __init sp_hwspinlock_init(void)
{
	return platform_driver_register(&sp_hwspinlock_driver);
}
/* board init code might need to reserve hwspinlocks for predefined purposes */
postcore_initcall(sp_hwspinlock_init);

static void __exit sp_hwspinlock_exit(void)
{
	platform_driver_unregister(&sp_hwspinlock_driver);
}
module_exit(sp_hwspinlock_exit);


