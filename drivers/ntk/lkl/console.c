#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/console.h>

extern unsigned long DbgPrint(const char *, ...);

static void console_write(struct console *con, const char *str, unsigned len)
{
	DbgPrint("%s", str);
}

static struct console stdio_console = {
	.name	= "stdio_console",
	.write	= console_write,	
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
};

int __init stdio_console_init(void)
{
	register_console(&stdio_console);
	return 0;
}

late_initcall(stdio_console_init);


