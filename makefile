SH = /bin/bash

IMG 	 = filesystem.img

FAT_TYPE = 32
SIZE	 = 200

CC = gcc
CFLAGS = -Wall -Wextra 
#CFLAGS += -fanalyzer
CFLAGS += -I./
CFLAGS += -g

OBJ = main.o
OBJ += src/cache.o src/fs.o src/table.o src/file.o src/dir.o
TARGET = fatinfo
TESTFILE = /prova.txt

.PHONY=all
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY = create
create:
	rm -f $(IMG)
	mkfs.fat -F $(FAT_TYPE) -C $(IMG) $(SIZE)

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
	valgrind --tool=memcheck -s ./$(TARGET) $(TESTFILE)

.PHONY=count
count:
	wc -l src/*.c *.c
