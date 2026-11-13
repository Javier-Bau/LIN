#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>

MODULE_LICENSE("GPL");

#define MAX_ITEMS_CBUF 4
#define MAX_CHARS_KBUF 12
struct kfifo cbuf;
struct semaphore elementos,huecos;

static struct proc_dir_entry *proc_entry;

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
        kfifo_in(&cbuf,&val,sizeof(int));
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

    if ((*off)>0)
        return 0;
   
    if (down_interruptible(&elementos))
        return -EINTR;

    /* Extraer el primer entero del buffer */
    if(kfifo_out(&cbuf,&val,sizeof(int)) != 1)
        return -EAGAIN;
   
    up(&huecos);

    nr_bytes=sprintf(kbuf,"%i\n",val);
    if (copy_to_user(buf, kbuf, nr_bytes)){
        kfree(kbuf);
        return -EFAULT;
    }
    kfree(kbuf);
    return nr_bytes;
}


// Needed proc_ops:
struct proc_ops pops = {
.proc_read = prodcons_read, //read()
.proc_write = prodcons_write, //write()
};

int modulo_prodcons_init(void)
{
    printk(KERN_INFO "Modulo prodcons cargado\n");
    proc_entry = proc_create("prodcons", 0666, NULL, &pops);

    if (kfifo_alloc(&cbuf,MAX_ITEMS_CBUF*sizeof(int),GFP_KERNEL))
        return -ENOMEM;
    sema_init(&elementos,0);
    sema_init(&huecos,MAX_ITEMS_CBUF);

    // Comprobamos si falla al crear proc por si acaso
    if(proc_entry == NULL){
        printk(KERN_ERR "[prodcons]Error al crear /proc/prodcons\n");
        return -ENOMEM;
    }

    return 0;
}

void modulo_prodcons_clean(void)
{

    remove_proc_entry("prodcons", NULL);

    printk(KERN_INFO "Modulo prodcons descargado.\n");
}

module_init(modulo_prodcons_init);
module_exit(modulo_prodcons_clean);
