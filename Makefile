OS := $(shell uname -s)

ifeq ($(OS), FreeBSD)
    CC      := clang
    CFLAGS  := -Wall -Wextra -O2 -Isrc -I/usr/local/include/libxml2 -I/usr/local/include
    LDFLAGS := -L/usr/local/lib
else
    CC      := gcc
    CFLAGS  := -Wall -Wextra -O2 -Isrc -I/usr/include/libxml2
    LDFLAGS :=
endif

LIBS    := -lbeaker -lcurl -lxml2 -lpthread -lm

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
	@echo "Build complete for $(OS): $(TARGET)"

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
	@echo "Detected OS: $(OS)"
	@echo "Compiler:    $(CC)"
	@echo "CFlags:      $(CFLAGS)"
	@echo ""
	@echo "Sources to compile:"
	@echo "$(SRCS)" | tr ' ' '\n'
	@echo ""
	@echo "Object files to generate:"
	@echo "$(OBJS)" | tr ' ' '\n'

.PHONY: all run clean rebuild info
