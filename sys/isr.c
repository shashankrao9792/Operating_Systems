#include <sys/defs.h>
#include "sys/kprintf.h"
#include "sys/irq.h"
#include "sys/syscall.h"

void isr_handler()
{
   kprintf("\nException!!");
}

void divideByZero_handler_code(registers_t regs){
	kprintf("Divide by zero exception\n");
	sys_exit(regs);
}
