# Makefile for TCP Socket Project (Linux/Ubuntu)

CC = gcc
CFLAGS = -Wall -Wextra -I. -pthread
LDFLAGS = -pthread

# Directories
SERVER_DIR = server
CLIENT_DIR = client
COMMON_DIR = common
BUILD_DIR = build
DATA_DIR = data

# Source files
COMMON_SRC = $(COMMON_DIR)/protocol.c $(COMMON_DIR)/cJSON.c
SERVER_SRC = $(SERVER_DIR)/server.c $(SERVER_DIR)/file_db.c $(SERVER_DIR)/session.c
CLIENT_SRC = $(CLIENT_DIR)/client.c

# Object files
COMMON_OBJ = $(BUILD_DIR)/protocol.o $(BUILD_DIR)/cJSON.o
SERVER_OBJ = $(BUILD_DIR)/server_main.o $(BUILD_DIR)/file_db.o $(BUILD_DIR)/session.o
CLIENT_OBJ = $(BUILD_DIR)/client_main.o

# Executables
SERVER_EXE = $(BUILD_DIR)/server
CLIENT_EXE = $(BUILD_DIR)/client

# Default target
all: $(BUILD_DIR) $(DATA_DIR) $(SERVER_EXE) $(CLIENT_EXE)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Create data directory
$(DATA_DIR):
	mkdir -p $(DATA_DIR)

# Build server
$(SERVER_EXE): $(COMMON_OBJ) $(SERVER_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo Server built successfully!

# Build client
$(CLIENT_EXE): $(COMMON_OBJ) $(CLIENT_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo Client built successfully!

# Compile common sources
$(BUILD_DIR)/protocol.o: $(COMMON_DIR)/protocol.c $(COMMON_DIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cJSON.o: $(COMMON_DIR)/cJSON.c $(COMMON_DIR)/cJSON.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile server sources
$(BUILD_DIR)/server_main.o: $(SERVER_DIR)/server.c $(SERVER_DIR)/server.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/file_db.o: $(SERVER_DIR)/file_db.c $(SERVER_DIR)/file_db.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/session.o: $(SERVER_DIR)/session.c $(SERVER_DIR)/session.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile client sources
$(BUILD_DIR)/client_main.o: $(CLIENT_DIR)/client.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build files!"

# Clean all (including data)
clean-all: clean
	rm -rf $(DATA_DIR)
	@echo "Cleaned all files including data!"

# Run server
run-server: $(SERVER_EXE)
	@echo "Starting server..."
	@./$(SERVER_EXE)

# Run client
run-client: $(CLIENT_EXE)
	@echo "Starting client..."
	@./$(CLIENT_EXE)

.PHONY: all clean clean-all run-server run-client
