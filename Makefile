# Compiler and flags
CC=gcc
CXX=g++
CFLAGS=-std=c99 -O0 -g
CXXFLAGS=-g -pthread -O0

# Directories
SRC_DIR=src
LIB_DIR=lib
BIN_DIR=bin

# Sources and objects
UTILS_SRC=$(LIB_DIR)/utils.c
EVICTION_SRC=$(LIB_DIR)/eviction.c
COVERT_SRC=$(LIB_DIR)/covert.c
TEST_SRC=$(SRC_DIR)/test.c
VICTIM_SRC=$(SRC_DIR)/victim.c

UTILS_OBJ=$(BIN_DIR)/utils.o
EVICTION_OBJ=$(BIN_DIR)/eviction.o
COVERT_OBJ=$(BIN_DIR)/covert.o
TEST_OBJ=$(BIN_DIR)/test.o
VICTIM_OBJ=$(BIN_DIR)/victim.o

# Targets
TEST_OUT=$(BIN_DIR)/test.out
VICTIM_OUT=$(BIN_DIR)/victim.out

# Default target
all: $(TEST_OUT) $(VICTIM_OUT)

# Rules for object files
$(UTILS_OBJ): $(UTILS_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(EVICTION_OBJ): $(EVICTION_SRC)
	$(CC) $(CFLAGS) -pthread -c $< -o $@

$(COVERT_OBJ): $(COVERT_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_OBJ): $(TEST_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(VICTIM_OBJ): $(VICTIM_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Rules for executables
$(TEST_OUT): $(TEST_OBJ) $(COVERT_OBJ) $(EVICTION_OBJ) $(UTILS_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(VICTIM_OUT): $(VICTIM_OBJ) $(COVERT_OBJ) $(EVICTION_OBJ) $(UTILS_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Clean rule
clean:
	rm -f $(BIN_DIR)/*.o $(BIN_DIR)/*.out

