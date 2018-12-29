#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/timer.h>

MODULE_AUTHOR("Tadakazu Nagai");
MODULE_DESCRIPTION("kadai-1 driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.1");

static dev_t g_dev;
static struct cdev g_cdv;
static struct class *g_cls = NULL;
static volatile u32 *g_gpio_base = NULL;

static struct timer_list g_timer;
static int g_count;

static void led_dec_2bits(int val) {
#if 0
	printk(KERN_INFO "led_dec_2bits(%d)\n", val);
#endif
	val &= 0x03;
	if (val == 0) { /* 00 */
#define OFF 10
#define ON 7
#define MSb 25
#define LSb 24
		g_gpio_base[OFF] = (1 << MSb) | (1 << LSb);
	}
	else if(val == 1) { /* 01 */
		g_gpio_base[OFF] = (1 << MSb);
		g_gpio_base[ON] = (1 << LSb);
	}
	else if(val == 2) { /* 10 */
		g_gpio_base[ON] = (1 << MSb);
		g_gpio_base[OFF] = (1 << LSb);
	}
	else if(val == 3) { /* 11 */
		g_gpio_base[ON] = (1 << MSb) | (1 << LSb);
#undef OFF
#undef ON
#undef MSb
#undef LSb
	}
}

static void timer_callback(struct timer_list *timer) {
	printk(KERN_INFO "callback()\n");
#if 0
	printk(KERN_INFO "* = %p\n", timer);
	printk(KERN_INFO "* = %p\n", &g_timer);
#endif
	led_dec_2bits(--g_count);
 	if (g_count > 0) {
		const int my_tick = (2000/* 2[s] */ * HZ) / 1000;
		mod_timer(&g_timer, jiffies + my_tick);
	}
}

static ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *pos)
{
	char c;
	if (copy_from_user(&c,buf,sizeof(char))) {
		printk(KERN_ERR "copy_from_user failed\n");
		return -EFAULT;
	}
#if 1
	printk(KERN_INFO "receive %c\n", c);
#endif
	if (g_count == 0) {
		led_dec_2bits(g_count = ((c == '0') ? 0 :
					 (c == '1') ? 1 :
					 (c == '2') ? 2 :
					 (c == '3') ? 3 : 0));
		if (g_count > 0) {
			const int my_tick = (2000/* 2[s] */ * HZ) / 1000;
			g_timer.function = timer_callback;
			g_timer.data = &g_timer;
			g_timer.expires = jiffies + my_tick;
			add_timer(&g_timer);
		}
	}
	return 1;
}

static ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *pos)
{
	static const char letter[] = { '0', '1', '2', '3' };
	if (copy_to_user(buf, (const char *)&(letter[g_count & 0x03]), sizeof(char))) {
		printk(KERN_ERR "sushi : copy_to_user failed\n");
		return -EFAULT;
	}
	return sizeof(char);
}

static struct file_operations g_fops = {
	.owner = THIS_MODULE,
	.write = my_write,
	.read = my_read
};

static int __init init_mod(void)
{
	int retval;
	if ((g_gpio_base = ioremap_nocache(0x3f200000, 0xA0)) == NULL) {
		return -EIO;
	}
#if 1
	printk("g_gpio_base = %p\n", g_gpio_base);
#endif
	{
		const u32 led = 24; /* GPIO Pin */
		const u32 index = led / 10;
		const u32 shift = (led % 10) * 3;
		g_gpio_base[index] |= (1 << shift);
	}
	{
		const u32 led = 25; /* GPIO Pin */
		const u32 index = led / 10;
		const u32 shift = (led % 10) * 3;
		g_gpio_base[index] |= (1 << shift);
	}
#define MY_CHRDEV_NAME "kadai"
	if ((retval = alloc_chrdev_region(&g_dev, 0, 1, MY_CHRDEV_NAME)) < 0) {
		printk(KERN_ERR "alloc_chrdev_region failed.\n");
		return retval;
	}
#if 1
	printk(KERN_INFO "%s is loaded. major: %d\n", __FILE__, MAJOR(g_dev));
#endif
	cdev_init(&g_cdv, &g_fops);
	if ((retval = cdev_add(&g_cdv, g_dev, 1)) < 0) {
		printk(KERN_ERR "cdev_add failed. major: %d, minor:%d", MAJOR(g_dev), MINOR(g_dev));
		return retval;
	}
	if (IS_ERR(g_cls = class_create(THIS_MODULE, MY_CHRDEV_NAME))) {
		printk(KERN_ERR "class_create failed.\n");
		return PTR_ERR(g_cls);
	}
	device_create(g_cls, NULL, g_dev, NULL, MY_CHRDEV_NAME"%d", MINOR(g_dev));
#undef MY_CHRDEV_NAME
	init_timer(&g_timer);
#if 1
	printk(KERN_INFO "timer_setup()\n");
	printk(KERN_INFO "* HZ = %d\n", HZ);
	printk(KERN_INFO "* jiffies = %d\n", jiffies);
#endif
	return 0;
}

static void __exit cleanup_mod(void)
{
	del_timer_sync(&g_timer);
#if 1 // LEDs off
	led_dec_2bits(0);
#endif
	cdev_del(&g_cdv);
	device_destroy(g_cls, g_dev);
	class_destroy(g_cls);
	unregister_chrdev_region(g_dev, 1);
#if 1
	printk(KERN_INFO "%s is unloaded. major: %d\n", __FILE__, MAJOR(g_dev));
#endif
	return;
}

module_init(init_mod);
module_exit(cleanup_mod);
