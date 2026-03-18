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

PREFIX      ?= /usr
INSTALL_BIN_DIR := $(PREFIX)/bin
DATA_DIR    := /etc/omnisearch
CONF_DIR    := /etc/omnisearch
VAR_DIR     := /var/lib/omnisearch
LOG_DIR     := /var/log/omnisearch
CACHE_DIR   := /var/cache/omnisearch
USER        := omnisearch
GROUP       := omnisearch

SYSTEMD_DIR := /etc/systemd/system
OPENRC_DIR  := /etc/init.d
DINIT_DIR   := /etc/dinit.d

install:
	@echo "Available install targets:"
	@echo "  make install-systemd"
	@echo "  make install-openrc"
	@echo "  make install-runit"
	@echo "  make install-s6"
	@echo "  make install-dinit"
	@echo ""
	@echo "Example: doas/sudo make install-openrc"

install-systemd: $(TARGET)
	@mkdir -p $(DATA_DIR)/templates $(DATA_DIR)/static $(LOG_DIR) $(CACHE_DIR)
	@cp -rf templates/* $(DATA_DIR)/templates/
	@cp -rf static/* $(DATA_DIR)/static/
	@cp -n example-config.ini $(DATA_DIR)/config.ini || true
	install -m 755 $(TARGET) $(INSTALL_BIN_DIR)/omnisearch
	@echo "Setting up user '$(USER)'..."
	@(grep -q '^$(GROUP):' /etc/group || groupadd $(GROUP)) 2>/dev/null || true
	@id -u $(USER) >/dev/null 2>&1 || useradd --system --home $(DATA_DIR) --shell /usr/sbin/nologin -g $(GROUP) $(USER)
	@chown -R $(USER):$(GROUP) $(LOG_DIR) $(CACHE_DIR) $(VAR_DIR) $(DATA_DIR) 2>/dev/null || true
	@chown $(USER):$(GROUP) $(DATA_DIR)/config.ini 2>/dev/null || true
	install -m 644 init/systemd/omnisearch.service $(SYSTEMD_DIR)/omnisearch.service
	@echo ""
	@echo "Config: $(DATA_DIR)/config.ini"
	@echo "Edit config with: nano $(DATA_DIR)/config.ini"
	@echo "Installed systemd service to $(SYSTEMD_DIR)/omnisearch.service"
	@echo "Run 'systemctl enable --now omnisearch' to start"

install-openrc: $(TARGET)
	@mkdir -p $(DATA_DIR)/templates $(DATA_DIR)/static $(LOG_DIR) $(CACHE_DIR)
	@cp -rf templates/* $(DATA_DIR)/templates/
	@cp -rf static/* $(DATA_DIR)/static/
	@cp -n example-config.ini $(DATA_DIR)/config.ini || true
	install -m 755 $(TARGET) $(INSTALL_BIN_DIR)/omnisearch
	@echo "Setting up user '$(USER)'..."
	@(grep -q '^$(GROUP):' /etc/group || groupadd $(GROUP)) 2>/dev/null || true
	@id -u $(USER) >/dev/null 2>&1 || useradd --system --home $(DATA_DIR) --shell /usr/sbin/nologin -g $(GROUP) $(USER)
	@chown -R $(USER):$(GROUP) $(LOG_DIR) $(CACHE_DIR) $(VAR_DIR) $(DATA_DIR) 2>/dev/null || true
	@chown $(USER):$(GROUP) $(DATA_DIR)/config.ini 2>/dev/null || true
	install -m 755 init/openrc/omnisearch $(OPENRC_DIR)/omnisearch
	@echo ""
	@echo "Config: $(DATA_DIR)/config.ini"
	@echo "Edit config with: nano $(DATA_DIR)/config.ini"
	@echo "Installed openrc service to $(OPENRC_DIR)/omnisearch"
	@echo "Run 'rc-update add omnisearch default' to enable"

install-runit: $(TARGET)
	@mkdir -p $(DATA_DIR)/templates $(DATA_DIR)/static $(LOG_DIR) $(CACHE_DIR)
	@cp -rf templates/* $(DATA_DIR)/templates/
	@cp -rf static/* $(DATA_DIR)/static/
	@cp -n example-config.ini $(DATA_DIR)/config.ini || true
	install -m 755 $(TARGET) $(INSTALL_BIN_DIR)/omnisearch
	@echo "Setting up user '$(USER)'..."
	@(grep -q '^$(GROUP):' /etc/group || groupadd $(GROUP)) 2>/dev/null || true
	@id -u $(USER) >/dev/null 2>&1 || useradd --system --home $(DATA_DIR) --shell /usr/sbin/nologin -g $(GROUP) $(USER)
	@chown -R $(USER):$(GROUP) $(LOG_DIR) $(CACHE_DIR) $(VAR_DIR) $(DATA_DIR) 2>/dev/null || true
	@chown $(USER):$(GROUP) $(DATA_DIR)/config.ini 2>/dev/null || true
	@mkdir -p /etc/service/omnisearch/log/supervise/control
	install -m 755 init/runit/run /etc/service/omnisearch/run
	install -m 755 init/runit/log/run /etc/service/omnisearch/log/run
	install -m 755 init/runit/log/run /etc/service/omnisearch/log/supervise/control
	@echo ""
	@echo "Config: $(DATA_DIR)/config.ini"
	@echo "Edit config with: nano $(DATA_DIR)/config.ini"
	@echo "Installed runit service to /etc/service/omnisearch"
	@echo "Service will start automatically"

install-s6: $(TARGET)
	@mkdir -p $(DATA_DIR)/templates $(DATA_DIR)/static $(LOG_DIR) $(CACHE_DIR)
	@cp -rf templates/* $(DATA_DIR)/templates/
	@cp -rf static/* $(DATA_DIR)/static/
	@cp -n example-config.ini $(DATA_DIR)/config.ini || true
	install -m 755 $(TARGET) $(INSTALL_BIN_DIR)/omnisearch
	@echo "Setting up user '$(USER)'..."
	@(grep -q '^$(GROUP):' /etc/group || groupadd $(GROUP)) 2>/dev/null || true
	@id -u $(USER) >/dev/null 2>&1 || useradd --system --home $(DATA_DIR) --shell /usr/sbin/nologin -g $(GROUP) $(USER)
	@chown -R $(USER):$(GROUP) $(LOG_DIR) $(CACHE_DIR) $(VAR_DIR) $(DATA_DIR) 2>/dev/null || true
	@chown $(USER):$(GROUP) $(DATA_DIR)/config.ini 2>/dev/null || true
	@mkdir -p /var/service/omnisearch/log/supervise/control
	install -m 755 init/s6/run /var/service/omnisearch/run
	install -m 755 init/s6/log/run /var/service/omnisearch/log/run
	install -m 755 init/s6/log/run /var/service/omnisearch/log/supervise/control
	@echo ""
	@echo "Config: $(DATA_DIR)/config.ini"
	@echo "Edit config with: nano $(DATA_DIR)/config.ini"
	@echo "Installed s6 service to /var/service/omnisearch"
	@echo "Service will start automatically"

install-dinit: $(TARGET)
	@mkdir -p $(DATA_DIR)/templates $(DATA_DIR)/static $(LOG_DIR) $(CACHE_DIR)
	@cp -rf templates/* $(DATA_DIR)/templates/
	@cp -rf static/* $(DATA_DIR)/static/
	@cp -n example-config.ini $(DATA_DIR)/config.ini || true
	install -m 755 $(TARGET) $(INSTALL_BIN_DIR)/omnisearch
	@echo "Setting up user '$(USER)'..."
	@(grep -q '^$(GROUP):' /etc/group || groupadd $(GROUP)) 2>/dev/null || true
	@id -u $(USER) >/dev/null 2>&1 || useradd --system --home $(DATA_DIR) --shell /usr/sbin/nologin -g $(GROUP) $(USER)
	@chown -R $(USER):$(GROUP) $(LOG_DIR) $(CACHE_DIR) $(VAR_DIR) $(DATA_DIR) 2>/dev/null || true
	@chown $(USER):$(GROUP) $(DATA_DIR)/config.ini 2>/dev/null || true
	install -m 644 init/dinit/omnisearch $(DINIT_DIR)/omnisearch
	@echo ""
	@echo "Config: $(DATA_DIR)/config.ini"
	@echo "Edit config with: nano $(DATA_DIR)/config.ini"
	@echo "Installed dinit service to $(DINIT_DIR)/omnisearch"
	@echo "Run 'dinitctl enable omnisearch' to start"

uninstall:
	rm -f $(INSTALL_BIN_DIR)/omnisearch
	rm -rf $(DATA_DIR)
	rm -f $(SYSTEMD_DIR)/omnisearch.service
	rm -f $(OPENRC_DIR)/omnisearch
	rm -f $(DINIT_DIR)/omnisearch
	rm -rf /etc/service/omnisearch
	rm -rf /var/service/omnisearch
	@id -u $(USER) >/dev/null 2>&1 && userdel $(USER) 2>/dev/null || true
	@grep -q '^$(GROUP):' /etc/group 2>/dev/null && groupdel $(GROUP) 2>/dev/null || true
	@echo "Uninstalled omnisearch"

.PHONY: all run clean rebuild info install install-systemd install-openrc install-runit install-s6 install-dinit uninstall
