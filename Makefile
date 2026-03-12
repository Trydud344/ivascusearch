CC      := cc
CFLAGS  := -Wall -Wextra -O2 -Isrc -I/usr/include/libxml2
LDFLAGS :=

LIBS    := -lbeaker -lcurl -lxml2 -lpthread -lm -lssl -lcrypto

SRC_DIR := src
BIN_DIR := bin
OBJ_DIR := obj

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TARGET := $(BIN_DIR)/omnisearch

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build complete: $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled: $<"

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleaned build artifacts"

rebuild: clean all

info:
	@echo "Compiler:    $(CC)"
	@echo "CFlags:      $(CFLAGS)"
	@echo ""
	@echo "Sources to compile:"
	@echo "$(SRCS)" | tr ' ' '\n'
	@echo ""
	@echo "Object files to generate:"
	@echo "$(OBJS)" | tr ' ' '\n'

.PHONY: all run clean rebuild info
