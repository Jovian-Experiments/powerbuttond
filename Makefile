CFLAGS := $(shell pkg-config --cflags libevdev) -Wall -Wextra -O2 -Werror
LDFLAGS := $(shell pkg-config --libs libevdev) -O2

all: powerbuttond
.PHONY: clean install

powerbuttond: powerbuttond.o
	$(CC) $(LDFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

clean:
	rm -f powerbuttond powerbuttond.o

install: all LICENSE
	install -D -m 755 powerbuttond $(DESTDIR)/usr/lib/hwsupport/powerbuttond
	install -D -m 644 LICENSE $(DESTDIR)/usr/share/licenses/powerbuttond
	install -D -m 644 99-powerbuttond-buttons.rules $(DESTDIR)/usr/lib/udev/rules.d/99-powerbuttond-buttons.rules
