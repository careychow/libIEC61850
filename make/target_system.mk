UNAME := $(shell uname)

MIPSEL_TOOLCHAIN_PREFIX=mipsel-openwrt-linux-
ARM_TOOLCHAIN_PREFIX=arm-linux-gnueabihf-
UCLINUX_ARM_TOOLCHAIN_PREFIX=arm-uclinux-elf-
MINGW_TOOLCHAIN_PREFIX=i586-mingw32msvc-
MINGW64_TOOLCHAIN_PREFIX=x86_64-w64-mingw32-

ifndef TARGET
ifeq ($(UNAME), Linux)
TARGET=POSIX
else ifeq ($(findstring MINGW,$(UNAME)), MINGW)
TARGET=WIN32
endif
endif

ifeq ($(TARGET), WIN32)
ifeq ($(UNAME), Linux)
TOOLCHAIN_PREFIX=$(MINGW_TOOLCHAIN_PREFIX)
else
TOOLCHAIN_PREFIX=
endif
endif

ifeq ($(TARGET), LINUX-MIPSEL)
TOOLCHAIN_PREFIX=$(MIPSEL_TOOLCHAIN_PREFIX)
endif

ifeq ($(TARGET), LINUX-ARM)
TOOLCHAIN_PREFIX=$(ARM_TOOLCHAIN_PREFIX)
endif

ifeq ($(TARGET), UCLINUX-WAGO)
TOOLCHAIN_PREFIX=$(UCLINUX_ARM_TOOLCHAIN_PREFIX)
CFLAGS += -msoft-float
CFLAGS += -Wall
CFLAGS += -DEMBED
CFLAGS += -Dlinux -D__linux__ -Dunix
CFLAGS += -D__uClinux__
LDFLAGS += -Wl,-move-rodata -Wl,-elf2flt
endif

ifeq ($(TARGET), WIN32)
HAL_IMPL = WIN32
LIB_OBJS_DIR = $(LIBIEC_HOME)/build_win32
CFLAGS=-g -DWIN32
LDLIBS=-lws2_32
DYNLIB_LDFLAGS=-Wl,-no-undefined -Wl,--enable-runtime-pseudo-reloc -Wl,--output-def,libiec61850.def,--out-implib,libiec61850.a


# on Windows: only compile with ethernet support if winpcap files are in third_party/winpcap!
ifneq (, $(wildcard  $(LIBIEC_HOME)/third_party/winpcap/Include/.))
LDFLAGS += -L$(LIBIEC_HOME)/third_party/winpcap/Lib
LDLIBS+=-lwpcap
else
$(warning winpcap not found - will build without GOOSE support!)
CFLAGS += -DEXCLUDE_ETHERNET_WINDOWS
endif


else
HAL_IMPL = POSIX

ifeq ($(TARGET), LINUX-MIPSEL)
LIB_OBJS_DIR = $(LIBIEC_HOME)/build-mipsel
else ifeq ($(TARGET), LINUX-ARM)
LIB_OBJS_DIR = $(LIBIEC_HOME)/build-arm
else ifeq ($(TARGET), UCLINUX-WAGO)
LIB_OBJS_DIR = $(LIBIEC_HOME)/build-wago
else
LIB_OBJS_DIR = $(LIBIEC_HOME)/build
endif

CFLAGS += -g

LDLIBS=-lpthread
DYNLIB_LDFLAGS=-lpthread
endif

CC=$(TOOLCHAIN_PREFIX)gcc
CPP=$(TOOLCHAIN_PREFIX)g++
AR=$(TOOLCHAIN_PREFIX)ar
RANLIB=$(TOOLCHAIN_PREFIX)ranlib

ifeq ($(TARGET), WIN32)
PROJECT_BINARY_NAME := $(PROJECT_BINARY_NAME).exe
endif

LIB_NAME = $(LIB_OBJS_DIR)/libiec61850.a

ifeq ($(TARGET), WIN32)
DYN_LIB_NAME = $(LIB_OBJS_DIR)/libiec61850.dll
else
DYN_LIB_NAME = $(LIB_OBJS_DIR)/libiec61850.so
endif
