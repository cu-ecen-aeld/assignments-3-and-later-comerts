
#CROSS_COMPILE=aarch64-none-linux-gnu-

CC ?= $(CROSS_COMPILE)gcc

TARGET ?= aesdsocket

ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CFLAGS ?= -ggdb -Werror -Wall -Wextra -Wno-format-truncation

INCLUDES ?= -I$(ROOT_DIR)/include -I$(ROOT_DIR)/../aesd-char-driver

LDFLAGS ?=  -lpthread -lrt

SOURCES := aesdsocket.c threading.c slist.c client.c

all:
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET)

pc:
	gcc $(CFLAGS) $(LDFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET)

clean:
	rm -rf *.o $(TARGET)

