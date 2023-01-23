SH = /bin/bash

CC = gcc
CFLAGS = -Wall -Wextra -O2
#CFLAGS += -fanalyzer
CFLAGS += -I./
CFLAGS += -g

OBJ = main.o
OBJ += src/file.o src/fat.o
TARGET = fatinfo

.PHONY=all
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY=clean
clean:
	rm $(OBJ)
	rm $(TARGET)

.PHONY=run
run:
	./$(TARGET)

.PHONY=debug
debug:
	gdb -q ./$(TARGET)

.PHONY=analyze
analyze:
	valgrind --tool=memcheck --leak-check=full -s ./$(TARGET)

.PHONY=count
count:
	wc -l *.c src/*.c include/*.h
