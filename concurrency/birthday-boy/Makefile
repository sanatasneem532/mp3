CC = gcc
CFLAGS = -Wall -Wextra -pthread -O3
TARGET = birthday

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)

