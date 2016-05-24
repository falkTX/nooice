
CC  ?= gcc
CXX ?= g++

_FLAGS    = -Wall -O2
CFLAGS   += $(_FLAGS) $(shell pkg-config --cflags jack) -std=c99
CXXFLAGS += $(_FLAGS) $(shell pkg-config --libs jack) -std=c++11
LDFLAGS  += -lpthread

all: build

build: noice noice.so

noice: main.cpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -o $@

noice.so: main.cpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -fPIC -shared -Wl,--no-undefined -o $@

install: build
	install -d $(DESTDIR)/usr/bin
	install -m 755 noice $(DESTDIR)/usr/bin/

clean:
	rm -f noice
