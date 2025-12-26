CC = gcc

# Base flags
CFLAGS = -Wall -g

# libpq flags (Homebrew / pkg-config)
# Nếu pkg-config chưa có: brew install pkgconf
LIBPQ_CFLAGS = $(shell pkg-config --cflags libpq 2>/dev/null)
LIBPQ_LDFLAGS = $(shell pkg-config --libs libpq 2>/dev/null)

# Fallback nếu máy bạn chưa cài pkg-config hoặc libpq formula
# (Apple Silicon Homebrew path)
ifeq ($(strip $(LIBPQ_CFLAGS)),)
LIBPQ_CFLAGS = -I/opt/homebrew/opt/libpq/include
LIBPQ_LDFLAGS = -L/opt/homebrew/opt/libpq/lib -lpq
endif

CFLAGS  += $(LIBPQ_CFLAGS)

# pthread trên macOS thường không cần -lpthread, nhưng để an toàn ta dùng -pthread
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
