CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=gnu11
SRC = src/main.c src/log.c src/config.c src/sim_monitor.c
BIN = build/guardd

all: $(BIN)

build:
	mkdir -p build

$(BIN): $(SRC) src/log.h src/config.h src/sim_monitor.h | build
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

clean:
	rm -rf build

.PHONY: all clean
