PREFIX ?= /usr/local
BUILD  ?= build

# gnu11 (not c11) keeps POSIX getopt/optarg visible under -Wpedantic
CFLAGS ?= -Os -std=gnu11 -Wall -Wextra -Wshadow -Wconversion -Wpedantic
CFLAGS += -flto -ffunction-sections -fdata-sections
CFLAGS += $(shell curl-config --cflags)

LDFLAGS += -flto -Wl,--gc-sections -s
LDFLAGS += $(shell curl-config --libs)

$(BUILD)/notify: notify.c | $(BUILD)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

$(BUILD):
	mkdir -p $(BUILD)

install: $(BUILD)/notify
	install -Dm755 $(BUILD)/notify $(DESTDIR)$(PREFIX)/bin/notify

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/notify

clean:
	rm -rf $(BUILD)

.PHONY: install uninstall clean
