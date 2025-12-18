#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TSU student");
MODULE_DESCRIPTION("Simple Linux kernel module (TSU)");
MODULE_VERSION("1.0");

static int __init tsu_module_init(void)
{
    printk(KERN_INFO "TSU module loaded: Welcome to Tomsk State University\n");
    return 0;
}

static void __exit tsu_module_exit(void) {printk(KERN_INFO "TSU module unloaded: Tomsk State University forever!\n");}

module_init(tsu_module_init);
module_exit(tsu_module_exit);
