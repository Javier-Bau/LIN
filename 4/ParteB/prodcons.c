#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h> //porque es dev

MODULE_LICENSE("GPL");

// Variable que guarda el número de items en el buffer circular.
#define MAX_ITEMS_CBUF 4
static int max_items = MAX_ITEMS_CBUF;
module_param(max_items, int, 0444);
MODULE_PARM_DESC(max_items, "Numero maximo de enteros del buffer");

// Resto de variables
#define MAX_CHARS_KBUF 12
#define DEVICE_NAME "prodcons" /* Device name */
struct kfifo cbuf;
struct semaphore elementos,huecos, mtx;


ssize_t prodcons_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    char kbuf[MAX_CHARS_KBUF+1];
    int val=0;
    size_t copy_len;

    if (len > sizeof(kbuf) - 1) {
      copy_len = sizeof(kbuf) - 1;
    } else {
      copy_len = len;
    }

    if (copy_from_user(kbuf, buf, copy_len)) {
      return -EFAULT;
    }
    kbuf[copy_len] = '\0';

    if(sscanf(kbuf, "%i", &val) == 1){
        if (down_interruptible(&huecos))
            return -EINTR;
        
        if (down_interruptible(&mtx)) {
            up(&huecos);
            return -EINTR;
        }
        if (kfifo_in(&cbuf, &val, sizeof(int)) != sizeof(int)) {
            up(&mtx);
            up(&huecos);
            return -EIO;
        }
        up(&mtx);
        up(&elementos);
    }else{
        return -EINVAL;
    }
    return len;
}

ssize_t prodcons_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    char kbuf[MAX_CHARS_KBUF+1];
    int nr_bytes=0;
    int val;

    if (*off > 0)
        return 0;
   
    if (down_interruptible(&elementos))
        return -EINTR;

    /* Extraer el primer entero del buffer, sin eliminarlo */
    if (down_interruptible(&mtx)) {
        up(&elementos);
        return -EINTR;
    }
    if (kfifo_out(&cbuf, &val, sizeof(int)) != sizeof(int)) {
        up(&mtx);
        up(&elementos);
        return -EIO;
    }
    up(&mtx);
    up(&huecos);

    nr_bytes=sprintf(kbuf,"%i\n",val);
    // Aceptamos que si hay algún error de estos 2 se descarta el elemento, es decir, consideramos que el elemento se ha consumido.
    if(nr_bytes > len) 
        return -EINVAL;
    if (copy_to_user(buf, kbuf, nr_bytes))
        return -EFAULT;

    *off+=nr_bytes;
    return nr_bytes;
}

static int prodcons_open(struct inode *inode, struct file *file)
{
    if (!try_module_get(THIS_MODULE))
        return -ENODEV;
    return 0;
}

static int prodcons_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE, //con owner no tengo que estar haciendo las operaciones de module todo el rato
    .read = prodcons_read,
    .write = prodcons_write,
};

static struct miscdevice prodcons_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .mode = 0666,
    .fops = &fops,
};

int modulo_prodcons_init(void)
{
    printk(KERN_INFO "Modulo prodcons cargado\n");

    if (kfifo_alloc(&cbuf,max_items*sizeof(int),GFP_KERNEL))
        return -ENOMEM;

    sema_init(&elementos,0);
    sema_init(&huecos,max_items);
    sema_init(&mtx, 1);

    // Comprobamos si falla al crear dev
    if(misc_register(&prodcons_misc)){
        printk(KERN_ERR "[prodcons]Error al crear /dev/prodcons\n");
        kfifo_free(&cbuf);
        return -ENOMEM;
    }

    return 0;
}

void modulo_prodcons_clean(void)
{
    misc_deregister(&prodcons_misc);
    kfifo_free(&cbuf);
    printk(KERN_INFO "Modulo prodcons descargado.\n");
}

module_init(modulo_prodcons_init);
module_exit(modulo_prodcons_clean);
