#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define GPIO_BUTTON 584  //gpio13
#define GPIO_GLED 595  //gpio24
#define GPIO_RLED 597  //gpio26
#define DEVICE_NAME "/dev/led"

static int major;
static int btn_count;
static int irq_num;
static int btn_pressed = 0;  //turn off 0; turn on 1
static unsigned long last_jiffies = 0;
static unsigned long debounce_delay = HZ / 5;
static int data_ready = 0;
static wait_queue_head_t wq;

//interrupt
static irqreturn_t btn_isr(int irq, void *dev_id){
	unsigned long now = jiffies;
	if (time_before(now, last_jiffies + debounce_delay))	return IRQ_HANDLED;
	last_jiffies = now;

	btn_pressed = 1;
	btn_count = (btn_count + 1 + 3) % 3;
	
	data_ready = 1;
	wake_up_interruptible(&wq);

	return IRQ_HANDLED;
}

static unsigned int gpio_poll(struct file *file, poll_table *wait){
	poll_wait(file, &wq, wait);
	if (data_ready) return POLLIN | POLLRDNORM;
	return 0;
}

static int gpio_open(struct inode *inode, struct file *file){
	return 0;
}

static int gpio_release(struct inode *inode, struct file *file){
	return 0;
}

static ssize_t gpio_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
	char msg[64];
	int msg_len;

	if (*offset > 0) return 0;
	//interrupt
	msg_len = snprintf(msg, sizeof(msg), "button:%d\nled:%d\n", btn_pressed, btn_count);
	btn_pressed = 0;
	data_ready = 0;

	if (copy_to_user(buf, msg, msg_len)) return -EFAULT;
	*offset = 0;

	return msg_len;
}

static ssize_t gpio_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){
	char kbuf[32] = {0};
	int val;

	if (copy_from_user(&kbuf, buf, len > 31 ? 31 : len)) return -EFAULT;
	
	if ((strncmp(kbuf, "reset", 5)) == 0) {
		btn_count = 1;
		data_ready = 0;
		btn_pressed = 0;
		gpio_set_value(GPIO_GLED, 1);
		gpio_set_value(GPIO_RLED, 0);
		printk(KERN_INFO "LED is reset");
		return 1;
	} else if ((strncmp(kbuf, "cancel", 6)) == 0) {
		btn_count = 0;
		data_ready = 0;
		btn_pressed = 0;
		gpio_set_value(GPIO_GLED, 0);
		gpio_set_value(GPIO_RLED, 0);
		printk(KERN_INFO "LED is cancel");
		return 1;
	}

	sscanf(kbuf, "led=%d", &val);  //get led number
	printk(KERN_INFO "LED set to %d\n", val);

	if (val == 0){
		gpio_set_value(GPIO_GLED, 0);
		gpio_set_value(GPIO_RLED, 0);
	} else if (val == 1){
		gpio_set_value(GPIO_GLED, 1);
		gpio_set_value(GPIO_RLED, 0);
	} else if (val == 2){
		gpio_set_value(GPIO_GLED, 0);
		gpio_set_value(GPIO_RLED, 1);
	} else {
		pr_err("Invalid value received: %c\n", kbuf);
		return -EINVAL;
	}
	return 1;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = gpio_open,
	.read = gpio_read,
	.write = gpio_write,
	.release = gpio_release,
	.poll = gpio_poll,
};

static int __init gpio_init(void){
	int ret;

	btn_count = 0;
	init_waitqueue_head(&wq);

	if (!gpio_is_valid(GPIO_BUTTON)){
		pr_err("Invalid GPIO %d\n", GPIO_BUTTON);
		return -ENODEV;
	}
	if (!gpio_is_valid(GPIO_GLED)){
		pr_err("Invalid GPIO %d\n", GPIO_GLED);
		return -ENODEV;
	}
	if (!gpio_is_valid(GPIO_RLED)){
		pr_err("Invalid GPIO %d\n", GPIO_RLED);
		return -ENODEV;
	}

	ret = gpio_request(GPIO_BUTTON, "gpio_button");
	if (ret){
		pr_err("Failed to request GPIO %d\n", GPIO_BUTTON);
		return ret;
	}
	gpio_direction_input(GPIO_BUTTON);
	
	ret = gpio_request(GPIO_GLED, "gpio_green_led");
	if (ret){
		pr_err("Failed to request GPIO %d\n", GPIO_GLED);
		return ret;
	}
	gpio_direction_output(GPIO_GLED, 0);

	ret = gpio_request(GPIO_RLED, "gpio_red_led");
	if (ret){
		pr_err("Failed to request GPIO %d\n", GPIO_RLED);
		return ret;
	}
	gpio_direction_output(GPIO_RLED, 0);

	irq_num = gpio_to_irq(GPIO_BUTTON);
	ret = request_irq(irq_num, btn_isr, IRQF_TRIGGER_FALLING, "gpio_button_irq", NULL);
	if (ret) {
		pr_err("Failed to requist IRQ\n");
		return ret;
	}

	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0){
		pr_err("Failed to register char device\n");
		gpio_free(GPIO_BUTTON);
		gpio_free(GPIO_GLED);
		gpio_free(GPIO_RLED);
		return major;
	}

	pr_info("gpio loaded. Major: %d\n", major);
	return 0;
}

static void __exit gpio_exit(void){
	free_irq(irq_num, NULL);
	unregister_chrdev(major, DEVICE_NAME);
	gpio_set_value(GPIO_GLED, 0);
	gpio_set_value(GPIO_RLED, 0);
	gpio_free(GPIO_BUTTON);
	gpio_free(GPIO_GLED);
	gpio_free(GPIO_RLED);
	pr_info("gpio unloaded.\n");
}

module_init(gpio_init);
module_exit(gpio_exit);
MODULE_LICENSE("GPL");
