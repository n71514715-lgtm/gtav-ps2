CC = ee-gcc
CFLAGS = -D_EE -O2 -G0 -Wall \
         -I$(PS2SDK)/ee/include \
         -I$(PS2SDK)/common/include \
         -I$(PS2SDK)/ports/include \
         -I/usr/local/ps2dev/gsKit/include
LDFLAGS = -L$(PS2SDK)/ee/lib \
          -L$(PS2SDK)/ports/lib \
          -L/usr/local/ps2dev/gsKit/lib \
          -lgskit -ldmakit -lpadx -lpad -lc -lkernel

SRC = src/engine/main.c
OBJ = $(SRC:.c=.o)

all: gtav.elf

gtav.elf: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)
	@echo ">>> GTAV-PS2 BUILT SUCCESSFULLY <<<"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) gtav.elf
