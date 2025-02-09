
CROSS_COMPILE=aarch64-none-linux-gnu-

TARGET := aesdsocket

ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CFLAGS := -ggdb -Werror -Wall -Wextra -Wno-format-truncation -lpthread -lrt

CFLAGS += -I$(ROOT_DIR)/include

SOURCES := aesdsocket.c threading.c slist.c client.c

all:
	$(CROSS_COMPILE)gcc $(CFLAGS) $(SOURCES) -o $(TARGET)

pc:
	gcc $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -rf *.o $(TARGET)

