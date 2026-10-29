#include <linux/syscalls.h> /* For SYSCALL_DEFINEi() */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>



struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "modleds: loading\n");
   printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

SYSCALL_DEFINE1(ledctl, unsigned int, leds)
{
    struct tty_driver* kbd_driver= get_kbd_driver_handler();
    unsigned int mask;
    int ret;
    if(kbd_driver == NULL){
        printk(KERN_ERR "[modleds]: error as there is not tty acctive\n");
        return -ENOTTY;
    }


    mask = ((leds& 0x4) >> 1) | ((leds& 0x2) << 1 ) | (leds& 0x1);
    ret = set_leds(kbd_driver,mask); 
    if(ret < 0) {
	return ret;
    }
    printk(KERN_DEBUG "Leds actualizados!!\n");
    return 0;
}
