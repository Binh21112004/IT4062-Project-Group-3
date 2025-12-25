CC = gcc

# Base flags
CFLAGS = -Wall -g

# libpq flags (pkg-config)
# Trên Ubuntu/Debian: sudo apt install libpq-dev pkg-config
LIBPQ_CFLAGS = $(shell pkg-config --cflags libpq 2>/dev/null)
LIBPQ_LDFLAGS = $(shell pkg-config --libs libpq 2>/dev/null)

# Fallback cho Linux nếu chưa cài pkg-config
ifeq ($(strip $(LIBPQ_CFLAGS)),)
LIBPQ_CFLAGS = -I/usr/include/postgresql
LIBPQ_LDFLAGS = -lpq
endif

CFLAGS  += $(LIBPQ_CFLAGS)

# pthread cho Linux
LDFLAGS = $(LIBPQ_LDFLAGS) -pthread

# Tên file thực thi
SERVER_BIN = server_app
CLIENT_BIN = client_app

# Source
SERVER_SRC = server/server.c server/config.c server/postgres_db.c server/session.c common/protocol.c
CLIENT_SRC = client/client.c common/protocol.c server/config.c

# Object
SERVER_OBJ = $(SERVER_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) -o $(SERVER_BIN) $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $(CLIENT_BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) $(SERVER_OBJ) $(CLIENT_OBJ)
