#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm-generic/errno.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("Javier Bautista Luis");
MODULE_LICENSE("GPL");

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0
#define NR_GPIO_LEDS  3
#define NR_LEDS 3
#define DEVICE_NAME "modleds-dev-pi" /* Device name */

const int leds_gpio[NR_LEDS] = {25, 27, 4}; /* D1=25, D2=27, D3=4 */
struct gpio_desc* gpio_descriptors[NR_GPIO_LEDS];

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

    set_pi_leds(mask);

    return len;
}

static inline int set_pi_leds(unsigned int mask) {
  int i;
  for (i = 0; i < NR_GPIO_LEDS; i++)
    gpiod_set_value(gpio_descriptors[i], (mask >> i) & 0x1 );
  return 0;
}

static int __init modleds_init(void)
{
  int i, j;
  int err = 0;
  struct device *device;
  char gpio_str[10];

  for (i = 0; i < NR_GPIO_LEDS; i++) {
    /* Build string ID */
    sprintf(gpio_str, "led_%d", i);
    /* Request GPIO */
    if ((err = gpio_request(led_gpio[i], gpio_str))) {
      pr_err("Failed GPIO[%d] %d request\n", i, led_gpio[i]);
      goto err_handle;
    }

    /* Transforming into descriptor */
    if (!(gpio_descriptors[i] = gpio_to_desc(led_gpio[i]))) {
      pr_err("GPIO[%d] %d is not valid\n", i, led_gpio[i]);
      err = -EINVAL;
      goto err_handle;
    }

    gpiod_direction_output(gpio_descriptors[i], 0);
  }	
  err = misc_register(&leds_misc);

  if (err)
  {
    pr_err("Couldn't register misc device\n");
    goto err_handle;
  }

  device = leds_misc.this_device;

  dev_info(device, "LedsMisc driver registered succesfully. To talk to\n");
  dev_info(device, "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
  dev_info(device, "Remove the module when done.\n");

  set_pi_leds(ALL_LEDS_ON);
  return 0;
err_handle:
  for (j = 0; j < i; j++)
    gpiod_put(gpio_descriptors[j]);
  return err;
}

static void __exit modleds_exit(void) {
  int i = 0;

  misc_deregister(&leds_misc);
  set_pi_leds(ALL_LEDS_OFF);

  for (i = 0; i < NR_GPIO_LEDS; i++)
    gpiod_put(gpio_descriptors[i]);
}

module_init(modleds_init);
module_exit(modleds_exit);
