CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=gnu11
MBEDTLS_DIR ?= ../vendor/mbedtls
CFLAGS += -I$(MBEDTLS_DIR)/include
LDLIBS = -L$(MBEDTLS_DIR)/library -lmbedtls -lmbedx509 -lmbedcrypto

SRC = src/main.c src/log.c src/config.c src/sim_monitor.c src/report.c \
      src/http_client.c src/https_client.c
BIN = build/guardd

MBEDTLS_LIBS = $(MBEDTLS_DIR)/library/libmbedtls.a

all: mbedtls $(BIN)

build:
	mkdir -p build

# Builds the vendored mbedTLS static libs if they are not already built.
# (Done once; `make -C vendor/mbedtls clean` if you need to force a rebuild.)
mbedtls: $(MBEDTLS_LIBS)

$(MBEDTLS_LIBS):
	$(MAKE) -C $(MBEDTLS_DIR) lib

$(BIN): $(SRC) src/log.h src/config.h src/sim_monitor.h src/report.h src/http_client.h src/https_client.h $(MBEDTLS_LIBS) | build
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDLIBS)

clean:
	rm -rf build

.PHONY: all clean mbedtls
