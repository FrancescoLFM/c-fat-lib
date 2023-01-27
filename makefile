SH = /bin/bash

IMG 	 = filesystem.img

FAT_TYPE = 32
SIZE	 = 100

CC = gcc
CFLAGS = -Wall -Wextra 
#CFLAGS += -fanalyzer
CFLAGS += -I./
CFLAGS += -g

OBJ = main.o
OBJ += src/file.o src/fat.o src/array.o
TARGET = fatinfo

.PHONY=all
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY = create
create:
	rm -f $(IMG)
	mkfs.fat -s 2 -F $(FAT_TYPE) -C $(IMG) $(SIZE)

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
	wc -l src/*.c *.c
