
CC  ?= gcc
CXX ?= g++

_FLAGS    = -Wall -O2
CFLAGS   += $(_FLAGS) $(shell pkg-config --cflags jack) -std=c99
CXXFLAGS += $(_FLAGS) $(shell pkg-config --libs jack) -std=c++11
LDFLAGS  += -lpthread

JACK_LIBDIR = $(shell pkg-config --variable=libdir jack)/jack/

all: build

build: noice noice.so

noice: main.cpp devices/*.hpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -o $@

noice.so: main.cpp devices/*.hpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -fPIC -shared -Wl,--no-undefined -o $@

install: build
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)$(JACK_LIBDIR)

	install -m 755 noice $(DESTDIR)/usr/bin/
	install -m 755 noice.so $(DESTDIR)$(JACK_LIBDIR)

install-udev: build
	install -d $(DESTDIR)/etc/systemd/system/
	install -d $(DESTDIR)/etc/udev/rules.d
	install -d $(DESTDIR)/usr/sbin

	install -m 644 noice@.service $(DESTDIR)/etc/systemd/system/
	install -m 644 99-noice.rules $(DESTDIR)/etc/udev/rules.d/
	install -m 755 noice-udev-register.sh $(DESTDIR)/usr/sbin/

clean:
	rm -f noice noice.so
