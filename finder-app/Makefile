
TARGET := writer

ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CFLAGS := -ggdb -Werror -Wall -Wextra -Wno-format-truncation

CFLAGS += -I$(ROOT_DIR)/include

SOURCES := writer.c

all:
	$(CROSS_COMPILE)gcc $(CFLAGS) $(SOURCES) -o $(TARGET) -lpthread -lrt

clean:
	rm -rf *.o $(TARGET)

