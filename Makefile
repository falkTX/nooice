
CC  ?= gcc
CXX ?= g++

_FLAGS    = -Wall -O2
CFLAGS   += $(_FLAGS) $(shell pkg-config --cflags jack) -std=c99
CXXFLAGS += $(_FLAGS) $(shell pkg-config --libs jack) -std=c++11
LDFLAGS  += -lpthread

JACK_LIBDIR = $(shell pkg-config --variable=libdir jack)/jack/

all: build

build: nooice nooice.so

nooice: main.cpp devices/*.hpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -o $@

nooice.so: main.cpp devices/*.hpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -fPIC -shared -Wl,--no-undefined -o $@

install: build
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)$(JACK_LIBDIR)

	install -m 755 nooice $(DESTDIR)/usr/bin/
	install -m 755 nooice.so $(DESTDIR)$(JACK_LIBDIR)

install-udev: build
	install -d $(DESTDIR)/etc/systemd/system/
	install -d $(DESTDIR)/etc/udev/rules.d
	install -d $(DESTDIR)/usr/sbin

	install -m 644 nooice@.service $(DESTDIR)/etc/systemd/system/
	install -m 644 99-nooice.rules $(DESTDIR)/etc/udev/rules.d/
	install -m 755 nooice-udev-register.sh $(DESTDIR)/usr/sbin/

clean:
	rm -f nooice nooice.so
