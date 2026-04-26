CC = gcc

CFLAGS = -Wall -g

TARGET = shell

SOURCES = shell.c commands.c command_list.c file_list.c memory_list.c process_list.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
