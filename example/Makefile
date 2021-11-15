CFLAGS = -I.. -g -Wall -pedantic -Wno-gnu-zero-variadic-macro-arguments
#CFLAGS = -O2

ifeq ($(OS),Windows_NT)
	
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		LDFLAGS = -framework Cocoa
	else ifeq ($(UNAME_S),Linux)
		LDFLAGS = -lcurl
	endif
endif

naett: main.c ../naett.c
	gcc $^ -o $@ $(CFLAGS) $(LDFLAGS)
