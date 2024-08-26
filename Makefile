CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

SRCS = server.c sync.c tftp.c
OBJS = $(SRCS:.c=.o)
HEADERS = sync.h tftp.h

TARGET = server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
