#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");

#define BUFFER_LENGTH  256
struct list_head mylist;
struct list_item {
	int data;
	struct list_head links;
};

#ifdef PARTE_OPCIONAL2
int list_size = 0; // Variable global que lleva el conteo de elementos en la lista
#endif

static struct proc_dir_entry *proc_entry;

void cleanup(struct list_head* head){
    struct list_item *cursor, *temporal; // Variables neceserarias para iterar en la lista y asi hacer remove

    // Hago lo mismo que en remove pero para toda la lista
    list_for_each_entry_safe(cursor, temporal, head, links){ 
        list_del(&cursor->links);
#ifdef PARTE_OPCIONAL2
        list_size = 0;
#endif
        kfree(cursor);
    }
}

void delete_if_equal(struct list_head* head, int num){
    struct list_item *cursor, *temporal; // Variables neceserarias para iterar en la lista y asi hacer remove

    list_for_each_entry_safe(cursor, temporal, head, links){ 
        if(cursor->data == num){
            list_del(&cursor->links);
            kfree(cursor);
#ifdef PARTE_OPCIONAL2
            list_size--;
#endif
        }
    }
}


// TODO estamos accediendo a una variable global desde un contexto que puede ser interrumpido, deberíamos usar mutex o algo así
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
      list_add_tail(&new_item->links,&mylist);
#ifdef PARTE_OPCIONAL2
      list_size++;
#endif

      printk(KERN_INFO "[MODLIST] Number %i added correctly\n", num);
    }
    else if(sscanf(kbuf, "remove %i", &num) == 1){ 
      printk(KERN_INFO "[MODLIST] Removing number: %i\n", num);
      delete_if_equal(&mylist, num);
      printk(KERN_INFO "[MODLIST] Todas las instancias de %i han sido borradas\n", num);
    }
    else if(strncmp(kbuf, "cleanup", 7)==0){
      printk(KERN_INFO "[MODLIST] Cleaning up...\n"); 
      cleanup(&mylist);
      printk(KERN_INFO "[MODLIST] Se ha limpiado la lista de manera exitosa\n");
    }

    return len;
}
#ifdef PARTE_OPCIONAL2

static void *modlist_seq_start(struct seq_file *m, loff_t *pos)
{
	struct list_head *pos_ptr;
    loff_t i = 0;

    // Si nos piden una posición mayor o igual al número de elementos, devolvemos NULL (EOF)
    if (*pos >= list_size) {
        return NULL;
    }
    /** Iteramos sobre los elementos desde head para llegar 
     * al objeto que está en el índice que nos piden */
    list_for_each(pos_ptr, &mylist) { 
        if (i == *pos) {
            return pos_ptr;
        }
        i++;
    }
    return NULL;
}

static void *modlist_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    struct list_head *pos_ptr = (struct list_head *)v;
    (*pos)++;
    if(*pos >= list_size)
        return NULL;
    
    return pos_ptr->next;
}

static void modlist_seq_stop(struct seq_file *m, void *v)
{
    // Como no usamos locks, no hay nada que liberar.
}

static int modlist_seq_show(struct seq_file *m, void *v)
{
    struct list_item *item = list_entry((struct list_head *)v, struct list_item, links);
    seq_printf(m, "%d\n", item->data);
    return 0;
}
static const struct seq_operations modlist_seq_ops = {
    .start = modlist_seq_start,
    .next  = modlist_seq_next,
    .stop  = modlist_seq_stop,
    .show  = modlist_seq_show
};

static int modlist_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &modlist_seq_ops);
}


static const struct proc_ops pops = {
	.proc_flags	= PROC_ENTRY_PERMANENT,
	.proc_open	= modlist_open,
	.proc_read_iter	= seq_read_iter,
	.proc_lseek	= seq_lseek,
	.proc_release	= seq_release,
    .proc_write = myproc_write
};

#else

static ssize_t myproc_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
  char kbuf[BUFFER_LENGTH];
  struct list_item* item = NULL;
  struct list_head* cur_node = NULL;
  char temp[32];

  int bytes_escritos = 0;        // Variable que lleva el conteo de bytes que se han escrito en kbuf antes de vaciarlo a buf.
  int bytes_escritos_total = 0;  // Variable que lleva el conteo de los bytes totales que se han volcado a buf.
  int pos = 0;                   // Para desde donde volcar los datos en buf
  int pos_off = 0;

  int res_snprintf, bytes_utiles, off_temp;

  printk(KERN_INFO "[MODLIST] Mostrando elementos...\n");
  
  list_for_each(cur_node, &mylist) {
      item = list_entry(cur_node, struct list_item, links);

      res_snprintf = snprintf(temp, sizeof(temp), "%d\n", item->data);
      if(pos_off + res_snprintf <= *off){
        pos_off += res_snprintf;
        continue;
      }

      off_temp = 0;
      if(pos_off < *off) {
        off_temp = *off-pos_off;
      }
      bytes_utiles = res_snprintf - off_temp;

      if(res_snprintf + bytes_escritos_total > len) break;
      if(res_snprintf + bytes_escritos > BUFFER_LENGTH) {
        if (copy_to_user(buf+pos, kbuf, bytes_escritos)) // Copiamos kbuf en buf
            return -EFAULT;
        pos += bytes_escritos;
        bytes_escritos = 0;
      }

      memcpy(kbuf + bytes_escritos, temp+off_temp, bytes_utiles);

      bytes_escritos += bytes_utiles;
      bytes_escritos_total += bytes_utiles;
      pos_off += res_snprintf;
  }

  if(bytes_escritos > 0) { // Copiar datos no copiados en el bucle
    if (copy_to_user(buf+pos, kbuf, bytes_escritos)){
        return -EFAULT;
    }
  }

  *off+=bytes_escritos_total;
  return bytes_escritos_total;
}

// Needed proc_ops:
struct proc_ops pops = {
	.proc_read = myproc_read, //read()
	.proc_write = myproc_write, //write()
};

#endif

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
