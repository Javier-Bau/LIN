#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm-generic/errno.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("Javier Bautista Luis");
MODULE_LICENSE("GPL");

#define NR_LEDS 3
#define DEVICE_NAME "modleds-dev-pi" /* Device name */

const int leds_gpio[NR_LEDS] = {25, 27, 4}; /* D1=25, D2=27, D3=4 */
const char *leds_gpio_str[NR_LEDS] = {"LED_D1", "LED_D2", "LED_D3"};

static ssize_t modleds_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
	.write = modleds_write,
};

static struct miscdevice leds_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .mode = 0666,
    .fops = &fops,
};

/* Update the 7-segment display with the configuration specified by the data parameter */
static ssize_t modleds_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    char kbuf[3];
    int mask;

    if (len == 0 || len > 2)
        return -EINVAL;
    
    if (copy_from_user(kbuf, buff, len))
        return -EFAULT;

    kbuf[len] = '\0';

    if (len == 2 && kbuf[1] != '\n') {
        return -EINVAL;
    }

    if (kbuf[0] < '0' || kbuf[0] > '7') {
        return -EINVAL; 
    }

    /* Convertir carácter ASCII a valor entero (de 0 a 7) */
    mask = kbuf[0] - '0';

    update_leds(mask);

    return len;
}

void update_leds(int mask)
{
    int i;
    for (i = 0; i < NR_LEDS; i++) {
        if (mask & (1 << i)) {
            gpio_set_value(leds_gpio[i], 1); /* Turn on LED */
        } else {
            gpio_set_value(leds_gpio[i], 0); /* Turn off LED */
        }
    }
}

static int __init modleds_init(void)
{

	int i, j;
	int err = 0;
	struct device *device;

	for (i = 0; i < NR_LEDS; i++)
	{

		/* Request the GPIO */
		if ((err = gpio_request(leds_gpio[i], leds_gpio_str[i])))
		{
			pr_err("Failed GPIO[%d] %d request\n", i, leds_gpio[i]);
			goto err_handle;
		}

		/* Transform number into descriptor */
		if (!(gpio_descriptors[i] = gpio_to_desc(leds_gpio[i])))
		{
			pr_err("GPIO[%d] %d is not valid\n", i, leds_gpio[i]);
			err = -EINVAL;
			goto err_handle;
		}

		/* Configure as an output pin */
		gpiod_direction_output(gpio_descriptors[i], 0);
	}

	/* Set everything as LOW */
	for (i = 0; i < NR_LEDS; i++)
		gpiod_set_value(gpio_descriptors[i], 0);

	/* Register misc device that exposes 7-segment display */
	err = misc_register(&leds_misc);

	if (err)
	{
		pr_err("Couldn't register misc device\n");
		goto err_handle;
	}

	device = leds_misc.this_device;

	dev_info(device, "Modleds driver registered succesfully. To talk to\n");
	dev_info(device, "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
	dev_info(device, "Remove the module when done.\n");

	return 0;
err_handle:
	for (j = 0; j < i; j++)
		gpiod_put(gpio_descriptors[j]);
	return err;
}

static void __exit modleds_exit(void)
{
    int i;

    misc_deregister(&leds_misc);

    /* Free GPIOs */
    for (i = 0; i < NR_LEDS; i++) {
        gpio_set_value(leds_gpio[i], 0); /* Turn off LED */
        gpio_free(leds_gpio[i]);
    }

    printk(KERN_INFO "modleds device unregistered\n");
}

module_init(modleds_init);
module_exit(modleds_exit);
