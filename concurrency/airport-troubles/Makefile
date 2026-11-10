CC = gcc
CFLAGS = -Wall -Wextra -pthread -O3
TARGET = airport-troubles

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)

