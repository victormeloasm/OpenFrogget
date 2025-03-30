# OpenFrogget Makefile
# Author: Víctor Duarte Melo


CXX := g++
CXXFLAGS := -O3 -march=native -Wall -Wextra -std=c++20 -Iinclude
LDFLAGS := -lssl -lcrypto -lgmp -lgmpxx
TARGET := openfrogget

# Paths
SRC_DIR := src
OBJ_DIR := build
BIN_DIR := bin

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Default target
all: setup build

# Install dependencies (g++, OpenSSL, GMP, UPX)
setup:
	@echo "[*] Installing required packages..."
	sudo apt update
	sudo apt install -y g++ libssl-dev libgmp-dev upx

# Create necessary folders
prepare:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Compile and link
build: prepare $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJS)
	@echo "[*] Linking executable..."
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "[*] Compressing binary with UPX..."
	upx --best --lzma $@ || echo "[!] Warning: UPX compression failed or was skipped."
	@echo "[✓] Build complete: $@"

# Compile individual object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "[*] Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "[*] Cleaning build files..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Rebuild from scratch
rebuild: clean all

# Phony targets
.PHONY: all setup prepare build clean rebuild
