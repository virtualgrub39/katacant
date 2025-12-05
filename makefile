# Config

SRC = main.c
OUTPUT = katacant

CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O2
CFLAGS += -std=gnu11 -D_GNU_SOURCE

# ------------------------------------------ #

$(OUTPUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)
