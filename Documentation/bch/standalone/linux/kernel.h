#ifndef _STANDALONE_KERNEL_H
#define _STANDALONE_KERNEL_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define kmalloc(_sz, _f)       malloc(_sz)
#define kzalloc(_sz, _f)       calloc(1,(_sz))
#define kfree(_ptr)            free(_ptr)
#define ARRAY_SIZE(_a)         (sizeof(_a)/sizeof((_a)[0]))
#define DIV_ROUND_UP(n,d)      (((n)+(d)-1)/(d))
#define EXPORT_SYMBOL_GPL(x)
#define MODULE_LICENSE(x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define printk                 printf
#define KERN_ERR	       ""

#include <arpa/inet.h>
#define cpu_to_be32(_x)        htonl(_x)

#define fls(_x)                (32-__builtin_clz(_x))

#endif
