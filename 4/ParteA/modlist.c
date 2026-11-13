#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");

#define BUFFER_LENGTH  256
struct list_head mylist;
struct list_item {
	int data;
	struct list_head links;
};

static struct proc_dir_entry *proc_entry;
static int list_entries = 0; // Variable global que lleva el conteo de entradas en la lista.
DEFINE_SPINLOCK(sp);

static void cleanup(struct list_head* head){
    struct list_item *cursor, *temporal; // Variables neceserarias para iterar en la lista y asi hacer remove

    // Hago lo mismo que en remove pero para toda la lista
    spin_lock(&sp);
    list_for_each_entry_safe(cursor, temporal, head, links){ 
        list_del(&cursor->links);
        kfree(cursor);
    }
    list_entries = 0;
    spin_unlock(&sp);
}

static void delete_if_equal(struct list_head* head, int num){
    struct list_item *cursor, *temporal; // Variables neceserarias para iterar en la lista y asi hacer remove

    spin_lock(&sp);
    list_for_each_entry_safe(cursor, temporal, head, links){ 
        if(cursor->data == num){
            list_del(&cursor->links);
            list_entries--;
            kfree(cursor);
        }
    }
    spin_unlock(&sp);
}

static ssize_t myproc_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    char kbuf[BUFFER_LENGTH]; // Buffer en kernel space
    int num;                  // Entero deonde guardaremos el número a procesar
    size_t copy_len;          // Para controlar la copia de datos de buf a kbuf
    struct list_item *new_item;

    if (len > sizeof(kbuf) - 1) {
      copy_len = sizeof(kbuf) - 1;
    } else {
      copy_len = len;
    }

    if (copy_from_user(kbuf, buf, copy_len)) {
      return -EFAULT; 
    }

    kbuf[copy_len] = '\0';
    if(sscanf(kbuf, "add %i", &num) == 1) {
      printk(KERN_INFO "[MODLIST] Adding number: %i\n", num);
      new_item = kmalloc(sizeof(struct list_item), GFP_KERNEL); // Usamos kmalloc al ser poca cantidad de memoria
      
      if(new_item == NULL){
        printk(KERN_ERR "[MODLIST] Error while trying to create new node\n");
        return -ENOMEM;       // Out of memory error
      }

      new_item->data = num;
      spin_lock(&sp);
      list_add_tail(&new_item->links,&mylist);
      list_entries++;
      spin_unlock(&sp);

      printk(KERN_INFO "[MODLIST] Number %i added correctly\n", num);
    }
    else if(sscanf(kbuf, "remove %i", &num) == 1){ 
      printk(KERN_INFO "[MODLIST] Removing number: %i\n", num);
      delete_if_equal(&mylist, num);
      printk(KERN_INFO "[MODLIST] Todas las instancias de %i han sido borradas\n", num);
    } 
    else if(strcmp(kbuf, "cleanup\n") == 0){
      printk(KERN_INFO "[MODLIST] Cleaning up...\n"); 
      cleanup(&mylist);
      printk(KERN_INFO "[MODLIST] Se ha limpiado la lista de manera exitosa\n");
    } else {
        return -EINVAL;
    }

    return len;
}


static ssize_t myproc_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
  char* kbuf;
  struct list_item* item = NULL;
  struct list_head* cur_node = NULL;
  char temp[32];

  int bytes_escritos = 0;        // Variable que lleva el conteo de bytes que se han escrito en kbuf antes de vaciarlo a buf.
  int res_snprintf;

  if (*off > 0 || len == 0)
    return 0;

  
  /*spin_lock(&sp);
  uint64_t min_s = (len < 12*list_entries)? len : 12*list_entries; // Calculamos el mínimo entre len y el tamaño de kbuf para no sobrepasar ninguno de los dos.
  spin_unlock(&sp);*/

  uint64_t min_s = min(len, (size_t)4*PAGE_SIZE); // consideramos que los números caben en 4 páginas(unos 3400 elementos) o que por lo menos las lecturas solo se pueden hacer de los números que quepan en una página.
  kbuf = kvmalloc(min_s, GFP_KERNEL);
  if (kbuf == NULL) {
    return -ENOMEM;
  }

  printk(KERN_INFO "[MODLIST] Mostrando elementos...\n");
  
  spin_lock(&sp);
  list_for_each(cur_node, &mylist) {
      item = list_entry(cur_node, struct list_item, links);

      res_snprintf = snprintf(temp, sizeof(temp), "%d\n", item->data);

      if(res_snprintf + bytes_escritos > min_s || res_snprintf < 1) break; // Si sobrepasamos len paramos la ejecución o el tamaño de kbuf paramos la ejecución. No debería de pasar de igual manera lo de kbuf

      memcpy(kbuf + bytes_escritos, temp, res_snprintf);

      bytes_escritos += res_snprintf;
  }
  spin_unlock(&sp);

  if(bytes_escritos > 0) { // Copiar datos no copiados en el bucle
    if (copy_to_user(buf, kbuf, bytes_escritos)){
        kvfree(kbuf);
        return -EFAULT;
    }
  }
  kvfree(kbuf);

  *off+=bytes_escritos;
  return bytes_escritos;
}

static int myproc_open(struct inode *inode, struct file *file) {
    if (!try_module_get(THIS_MODULE))
        return -ENODEV;
    return 0;
}

static int myproc_release(struct inode *inode, struct file *file) {
    module_put(THIS_MODULE);
    return 0;
}

// Needed proc_ops:
struct proc_ops pops = {
	.proc_read = myproc_read, //read()
	.proc_write = myproc_write, //write()
    .proc_open = myproc_open,
    .proc_release = myproc_release,
};

int modulo_modlist_init(void)
{
    printk(KERN_INFO "Modulo MODLIST cargado\n");
    INIT_LIST_HEAD(&mylist);
    proc_entry = proc_create("modlist", 0666, NULL, &pops);

    // Comprobamos si falla al crear proc por si acaso
    if(proc_entry == NULL){
        printk(KERN_ERR "[MODLIST]Error al crear /proc/modlist\n");
        return -ENOMEM;
    }

    return 0;
}

void modulo_modlist_clean(void)
{

    remove_proc_entry("modlist", NULL);

    // Libero la lista de la misma manera que cuando hago cleanup en el write para cuando cierro el modulo
    cleanup(&mylist);

    printk(KERN_INFO "Modulo MODLIST descargado.\n");
}

module_init(modulo_modlist_init);
module_exit(modulo_modlist_clean);
