# makefile for testing and benchmarking the bch library

COMMON_CFLAGS	:= -Wall -Wextra -Wno-unused-parameter -g -O3 -Istandalone
#COMMON_CFLAGS += -DCONFIG_BCH_CONST_M=13
#COMMON_CFLAGS += -DCONFIG_BCH_CONST_T=4
#COMMON_CFLAGS += -DCONFIG_BCH_CONST_PARAMS

SRC 	= ../../lib/bch.c
HEADER	= ../../include/linux/bch.h
DEPS	= $(SRC) $(HEADER)
PROGS	:=

all: nat

M13T4 = -DCONFIG_BCH_CONST_M=13 -DCONFIG_BCH_CONST_T=4 -DCONFIG_BCH_CONST_PARAMS
M13T8 = -DCONFIG_BCH_CONST_M=13 -DCONFIG_BCH_CONST_T=8 -DCONFIG_BCH_CONST_PARAMS
CHIEN = -DUSE_CHIEN_SEARCH -Wno-unused-function

# arch specific targets

ARCH	:= arm9
XCFLAGS	:= $(COMMON_CFLAGS) -mtune=arm926ej-s -static
CROSS	:= /opt/arm-eglibc/bin/arm-none-linux-gnueabi-
XSHELL	:= /system/bin/sh
XRUN	:= qemu-arm -cpu arm926
include arch.mk

ARCH	:= ca8
XCFLAGS	:= $(COMMON_CFLAGS) -mtune=cortex-a8 -march=armv7-a -mfpu=neon -static
CROSS	:= /opt/arm-eglibc/bin/arm-none-linux-gnueabi-
XSHELL	:= /bin/sh
XRUN	:= qemu-arm -cpu cortex-a8
include arch.mk

ARCH	:= ppc64
XCFLAGS	:= $(COMMON_CFLAGS) -mpowerpc64 -static -m32
CROSS	:= /usr/local/freescale-2010.09/bin/powerpc-linux-gnu-
XSHELL	:= /bin/sh
XRUN	:= qemu-ppc64abi32
include arch.mk

ARCH	:= k8
XCFLAGS	:= $(COMMON_CFLAGS) -mtune=k8 -m64 -static
CROSS	:= /usr/bin/
XSHELL	:= /bin/sh
XRUN	:=
include arch.mk

ARCH	:= mips
XCFLAGS	:= $(COMMON_CFLAGS) -march=mips32 -static
CROSS	:= /usr/local/mips-4.4/bin/mips-linux-gnu-
XSHELL	:= /bin/sh
XRUN	:= qemu-mips
include arch.mk

ARCH	:= nat
XCFLAGS	:= $(COMMON_CFLAGS) -mtune=native
CROSS	:= /usr/bin/
XSHELL	:= /bin/sh
XRUN	:=
include arch.mk

clean:
	rm -f *.o *~ $(PROGS) core
