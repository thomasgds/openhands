#
# RTOS Project - Top-level Makefile
#
CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -fPIC -g -O0
CFLAGS  += -I$(CURDIR)/kernel/include
CFLAGS  += -I$(CURDIR)/fs/include
CFLAGS  += -I$(CURDIR)/fatfs/include
CFLAGS  += -I$(CURDIR)/net/include
CFLAGS  += -I$(CURDIR)/shell/include
CFLAGS  += -I$(CURDIR)/services/include
CFLAGS  += -I$(CURDIR)/hal/include
LDFLAGS := -lm -lpthread -lreadline -ldl

# Collect all source files
KERNEL_SRCS := $(wildcard kernel/src/*.c)
FS_SRCS    := $(wildcard fs/src/*.c)
FATFS_SRCS := $(wildcard fatfs/src/*.c)
NET_SRCS   := $(wildcard net/src/*.c)
SHELL_SRCS := $(wildcard shell/src/*.c)
SVC_SRCS   := $(wildcard services/src/*.c)
HAL_SRCS   := $(wildcard hal/src/*.c)
MAIN_SRCS  := main.c

ALL_SRCS := $(KERNEL_SRCS) $(FS_SRCS) $(FATFS_SRCS) $(NET_SRCS) \
            $(SHELL_SRCS) $(SVC_SRCS) $(HAL_SRCS) $(MAIN_SRCS)
ALL_OBJS := $(ALL_SRCS:.c=.o)

TARGET := rtos.elf

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(ALL_OBJS) $(TARGET)
	@echo "Clean complete"
