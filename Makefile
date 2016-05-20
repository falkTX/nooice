
CC  ?= gcc
CXX ?= g++

_FLAGS    = -Wall -O0 -g
CFLAGS   += $(_FLAGS) $(shell pkg-config --cflags jack) -std=c99
CXXFLAGS += $(_FLAGS) $(shell pkg-config --libs jack) -std=c++11
LDFLAGS  += -lpthread

all: build

build: noice

noice: main.cpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -o $@

install: build

clean:
	rm -f noice
