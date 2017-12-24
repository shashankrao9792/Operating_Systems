/* References taken from 
   wiki.osdev.org/
   osdever.net
   wikipedia
   www.jamesmolloy.co.uk/
*/

#include "sys/defs.h"
#include "sys/outin.h"

void outb(uint16_t port, uint8_t val)
{
        __asm__ __volatile__("outb %0, %1": : "a"(val) ,"Nd"(port));
}

uint8_t inb(uint16_t port)
{
        uint8_t ret;
        __asm__ __volatile__("inb %1, %0": "=a"(ret) : "Nd"(port));
        return ret;
}

void outl(uint16_t port, uint32_t val)
{
        __asm__ __volatile__("outl %0, %1": : "a"(val) ,"Nd"(port));
}

uint32_t inl(uint16_t port)
{
        uint32_t ret;
        __asm__ __volatile__("inl %1, %0": "=a"(ret) : "Nd"(port));
        return ret;
}
