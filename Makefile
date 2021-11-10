CFLAGS = -g -Wall -pedantic -Wno-gnu-zero-variadic-macro-arguments
#CFLAGS = -O2

ifeq ($(OS),Windows_NT)
	
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		LDFLAGS = -framework Cocoa
	else ifeq ($(UNAME_S),Linux)
		#LDFLAGS = -s -lGLU -lGL -lX11
	endif
endif

naett: main.c naett.c naett_osx.c
	gcc $^ -o $@ $(CFLAGS) $(LDFLAGS)
