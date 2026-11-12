#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm-generic/errno.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

MODULE_DESCRIPTION("Misc Display7s Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

/* Bits associated with each segment */
#define DS_A 0x80
#define DS_B 0x40
#define DS_C 0x20
#define DS_D 0x10
#define DS_E 0x08
#define DS_F 0x04
#define DS_G 0x02
#define DS_DP 0x01
#define SEGMENT_COUNT 8

/*Codificacion de los numeros para el 7seg*/
#define NUM_0 (DS_A | DS_B | DS_C | DS_D | DS_E | DS_F)
#define NUM_1 (DS_B | DS_C)
#define NUM_2 (DS_A | DS_B | DS_D | DS_E | DS_G)
#define NUM_3 (DS_A | DS_B | DS_C | DS_D | DS_G)
#define NUM_4 (DS_B | DS_C | DS_F | DS_G)
#define NUM_5 (DS_A | DS_C | DS_D | DS_F | DS_G)
#define NUM_6 (DS_A | DS_C | DS_D | DS_E | DS_F | DS_G)
#define NUM_7 (DS_A | DS_B | DS_C)
#define NUM_8 (DS_A | DS_B | DS_C | DS_D | DS_E | DS_F | DS_G)
#define NUM_9 (DS_A | DS_B | DS_C | DS_D | DS_F | DS_G)
#define NUM_A (DS_A | DS_B | DS_C | DS_E | DS_F | DS_G)
#define NUM_B (DS_C | DS_D | DS_E | DS_F | DS_G)
#define NUM_C (DS_A | DS_D | DS_E | DS_F)
#define NUM_D (DS_B | DS_C | DS_D | DS_E | DS_G)
#define NUM_E (DS_A | DS_D | DS_E | DS_F | DS_G)
#define NUM_F (DS_A | DS_E | DS_F | DS_G)


/* Indices of GPIOs used by this module */
enum
{
	SDI_IDX = 0,
	RCLK_IDX,
	SRCLK_IDX,
	NR_GPIO_DISPLAY
};

/* Pin numbers */
const int display_gpio[NR_GPIO_DISPLAY] = {18, 23, 24};

/* Array to hold GPIO descriptors */
struct gpio_desc *gpio_descriptors[NR_GPIO_DISPLAY];

const char *display_gpio_str[NR_GPIO_DISPLAY] = {"sdi", "rclk", "srclk"};

/* Sequence of segments used by the character device driver */
const int sequence[] = {DS_D, DS_E, DS_F, DS_A, DS_B, DS_C, DS_G, DS_DP, -1};

#define DEVICE_NAME "display7s" /* Device name */
static DEFINE_SPINLOCK(open_lock); /*spinlock: protege el acceso al contados*/
static int usuarios = 0;
static DEFINE_MUTEX(write_mutex);/*mutex: para que solo un hilo pueda ejecutar el display*/

/*
 *  Prototypes
 */
static ssize_t display7s_write(struct file *, const char *, size_t, loff_t *);
static int display7s_open(struct inode *, struct file *);
static int display7s_release(struct inode *, struct file *);

/* Simple initialization of file_operations interface with a single operation */
static struct file_operations fops = {
	/*
	he mirado que si pones .owner no hace falta hacer los puts y get de module
	y el .owner = THIS_MODULE es como seria 
	*/
	.open = display7s_open,
	.release = display7s_release,
	.write = display7s_write,
};

static struct miscdevice display7s_misc = {
	.minor = MISC_DYNAMIC_MINOR, /* kernel dynamically assigns a free minor# */
	.name = DEVICE_NAME,		 /* when misc_register() is invoked, the kernel
								  * will auto-create device file;
								  * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
	.mode = 0666,				 /* ... dev node perms set as specified here */
	.fops = &fops,				 /* connect to this driver's 'functionality' */
};

static int display7s_open(struct inode *inode, struct file *filp){
	int ret = 0;
	//intento incrementar el contador
	if(!try_module_get(THIS_MODULE)) return -ENODEV;
	//lock del spin_lock para que solo 1 proceso lo abra a la vez
	spin_lock(&open_lock);
	//si ya esta abierto devolvemos un error
	if(usuarios != 0) ret = -EBUSY;
	else ++usuarios;

	spin_unlock(&open_lock);
	//si tenemos el error deshacemos el incremento del contador
	if(ret != 0) module_put(THIS_MODULE);

	return ret;
}

static int display7s_release(struct inode *inode, struct file *filp){
	//decremento usuarios (igual 0) para indicar que el dispositivo vuelve a estar disponible
	spin_lock(&open_lock);
	usuarios--;
	spin_unlock(&open_lock);
	//decremento el contador de referencias al modulo, para que se pueda descargar de nuevo
	module_put(THIS_MODULE);
	return 0;
}

/* Update the 7-segment display with the configuration specified by the data parameter */
static void update_7sdisplay(unsigned char data)
{
	int i = 0;
	int value = 0;

	for (i = 0; i < SEGMENT_COUNT; i++)
	{
		/* Explore current bit (from most significant to least significant) */
		if (0x80 & (data << i))
			value = 1;
		else
			value = 0;

		/* Set value of serial input */
		gpiod_set_value(gpio_descriptors[SDI_IDX], value);
		/* Generate clock cycle in shift register */
		gpiod_set_value(gpio_descriptors[SRCLK_IDX], 1);
		msleep(1);
		gpiod_set_value(gpio_descriptors[SRCLK_IDX], 0);
	}

	/* Generate clock cycle in output register to update 7-seg display */
	gpiod_set_value(gpio_descriptors[RCLK_IDX], 1);
	msleep(1);
	gpiod_set_value(gpio_descriptors[RCLK_IDX], 0);
}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/display7s
 * Test with the following command to see the sequence better:
 * $ while true; do echo > /dev/display7s; sleep 0.3; done
 */
static ssize_t
display7s_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	ssize_t ret;
	//lo hago muy similar al write practica 1
	char kbuf[3];
	unsigned char dato;

	mutex_lock(&write_mutex);

	if(len == 0 || len > 2) {
		ret = -EINVAL;
		goto unlock;
	}
	if(copy_from_user(kbuf, buff, len)) {
		ret -EINVAL;
		goto unlock;
	}
	kbuf[len] = '\0';
	//esto es por si hay que quitar el salto de linea del echo
	if(len == 2 && kbuf[1] == '\n'){
		kbuf[1] = '\n';
	}
	else if (len != 1){
		ret = -EINVAL;
		goto unlock;
	}

	switch (kbuf[0])
	{
	case '0': dato = NUM_0; break;
	case '1': dato = NUM_1; break;
	case '2': dato = NUM_2; break;
	case '3': dato = NUM_3; break;
	case '4': dato = NUM_4; break;
	case '5': dato = NUM_5; break;
	case '6': dato = NUM_6; break;
	case '7': dato = NUM_7; break;
	case '8': dato = NUM_8; break;
	case '9': dato = NUM_9; break;
	case 'a':
	case 'A': dato = NUM_A; break;
	case 'b':
	case 'B': dato = NUM_B; break;
	case 'c':
	case 'C': dato = NUM_C; break;
	case 'd':
	case 'D': dato = NUM_D; break;
	case 'e':
	case 'E': dato = NUM_E; break;
	case 'f':
	case 'F': dato = NUM_F; break;
	default: 
		ret = -EINVAL;
		goto unlock;
	}
	update_7sdisplay(dato);
	ret = len;
	//hago la etiqueta unlock para cuando hay un error abrir el mutex y mandar el error
unlock:
	mutex_unlock(&write_mutex);
	return ret;
}

static int __init display7s_misc_init(void)
{
	int i, j;
	int err = 0;
	struct device *device;

	for (i = 0; i < NR_GPIO_DISPLAY; i++)
	{

		/* Request the GPIO */
		if ((err = gpio_request(display_gpio[i], display_gpio_str[i])))
		{
			pr_err("Failed GPIO[%d] %d request\n", i, display_gpio[i]);
			goto err_handle;
		}

		/* Transform number into descriptor */
		if (!(gpio_descriptors[i] = gpio_to_desc(display_gpio[i])))
		{
			pr_err("GPIO[%d] %d is not valid\n", i, display_gpio[i]);
			err = -EINVAL;
			goto err_handle;
		}

		/* Configure as an output pin */
		gpiod_direction_output(gpio_descriptors[i], 0);
	}

	/* Set everything as LOW */
	for (i = 0; i < NR_GPIO_DISPLAY; i++)
		gpiod_set_value(gpio_descriptors[i], 0);

	/* Register misc device that exposes 7-segment display */
	err = misc_register(&display7s_misc);

	if (err)
	{
		pr_err("Couldn't register misc device\n");
		goto err_handle;
	}

	device = display7s_misc.this_device;

	dev_info(device, "Display7s driver registered succesfully. To talk to\n");
	dev_info(device, "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
	dev_info(device, "Remove the module when done.\n");

	return 0;
err_handle:
	for (j = 0; j < i; j++)
		gpiod_put(gpio_descriptors[j]);
	return err;
}

static void __exit display7s_misc_exit(void)
{
	int i = 0;

	/* Unregister character device */
	misc_deregister(&display7s_misc);

	/* Clear display */
	update_7sdisplay(0);

	/* Free up pins */
	for (i = 0; i < NR_GPIO_DISPLAY; i++)
		gpiod_put(gpio_descriptors[i]);

	pr_info("Display7s driver deregistered. Bye\n");
}

module_init(display7s_misc_init);
module_exit(display7s_misc_exit);
