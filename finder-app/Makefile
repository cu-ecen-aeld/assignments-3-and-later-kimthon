CC = gcc

ifdef CROSS_COMPILE
CC = $(CROSS_COMPILE)gcc
endif

CFLAGS = -Wall -Wextra -g

SRC = writer.c
OBJ = $(SRC:.c=.o)

TARGET = writer

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
