CFLAGS = -I.. -g -Wall -pedantic -DINCLUDE_MAIN

ifeq ($(OS),Windows_NT)
	CFLAGS += -DUNICODE
	LDFLAGS = -lwinhttp
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		LDFLAGS = -framework Cocoa
		CFLAGS += -Wno-gnu-zero-variadic-macro-arguments
	else ifeq ($(UNAME_S),Linux)
		LDFLAGS = -lcurl -lpthread
	endif
endif

test: test.c ../naett.c
	gcc $^ -o $@ $(CFLAGS) $(LDFLAGS)
