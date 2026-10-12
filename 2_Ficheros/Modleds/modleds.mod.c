#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x943f687a, "vc_cons" },
	{ 0x5a844b26, "__x86_indirect_thunk_r12" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
	{ 0xd272d446, "__fentry__" },
	{ 0xe8213e80, "_printk" },
	{ 0x7ec472ba, "fg_console" },
	{ 0xba157484, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0x943f687a,
	0x5a844b26,
	0xd272d446,
	0x90a48d82,
	0xd272d446,
	0xe8213e80,
	0x7ec472ba,
	0xba157484,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"vc_cons\0"
	"__x86_indirect_thunk_r12\0"
	"__x86_return_thunk\0"
	"__ubsan_handle_out_of_bounds\0"
	"__fentry__\0"
	"_printk\0"
	"fg_console\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "570B4355BB71FA6C11D48C7");
