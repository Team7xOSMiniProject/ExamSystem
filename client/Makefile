# Compiler and flags
CC = g++
CFLAGS = -g -Wall -Wextra -I ../server -I ../client -I ../data -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare
LDFLAGS = -pthread

# Source files for the client
CLIENT_SRC = client.cpp ui.cpp main.cpp

# Executable
CLIENT_EXEC = client

# The default target to build the client
all: $(CLIENT_EXEC)

# Compile client application
$(CLIENT_EXEC): $(CLIENT_SRC)
	@echo "Building client..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(CLIENT_EXEC) $(CLIENT_SRC)

# Clean the build files
clean:
	@echo "Cleaning client build files..."
	rm -f $(CLIENT_EXEC)

# Phony targets
.PHONY: all clean
