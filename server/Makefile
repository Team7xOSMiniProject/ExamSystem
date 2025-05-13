# Compiler and flags
CC = g++
CFLAGS = -g -Wall -Wextra -I ../server -I ../client -I ../data -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare
LDFLAGS = -pthread

# Source files for the server
SERVER_SRC = server.cpp auth.cpp exam_manager.cpp main.cpp

# Executable
SERVER_EXEC = server

# The default target to build the server
all: $(SERVER_EXEC)

# Compile server application
$(SERVER_EXEC): $(SERVER_SRC)
	@echo "Building server..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(SERVER_EXEC) $(SERVER_SRC)

# Clean the build files
clean:
	@echo "Cleaning server build files..."
	rm -f $(SERVER_EXEC)

# Phony targets
.PHONY: all clean
