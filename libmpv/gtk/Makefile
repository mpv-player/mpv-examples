CC ?= gcc
CFLAGS += -g -Wall -pedantic `pkg-config --cflags mpv gtk+-3.0 epoxy`
LDLIBS += `pkg-config --libs mpv gtk+-3.0 epoxy`

SRC = main.c

OBJ = $(SRC:.c=.o)

main: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(OBJ) main

.PHONY: clean
