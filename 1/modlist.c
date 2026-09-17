#include <linux/module.h>
#include <linux/kernel.h>
MODULE_LICENSE("GPL");

// Double-linked list def:
struct list_head mylist;
struct list_item {
	int data;
	struct list_head links;
};

// Needed proc_ops:
struct proc_ops pops = {
	.proc_read = myproc_read, //read()
	.proc_write = myproc_write, //write()
};

int modulo_modlist_init(void)
{
	printk(KERN_INFO "Modulo MODLIST cargado\n");
	proc_entry = proc_create("modlist", 0666, NULL, &pops);
	return 0;
}

void modulo_modlist_clean(void)
{
	printk(KERN_INFO "Modulo MODLIST descargado.\n");
}

module_init(modulo_lin_init);
module_exit(modulo_lin_clean);
