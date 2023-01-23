IMG 	 = filesystem.img

FAT_TYPE = 32
SIZE	 = 100

C_ARGS	 = -O2 -Wall -Werror -g
TARGET	 = fatinfo
SRC		 = main.c

.PHONY = all
all:
	gcc $(C_ARGS) $(SRC) -o $(TARGET)

.PHONY = create
create:
	rm -f $(IMG)
	mkfs.fat -s 2 -F $(FAT_TYPE) -C $(IMG) $(SIZE)

.PHONY = clean
clean:
	rm -f $(TARGET)

.PHONE = run
run:
	./$(TARGET)
