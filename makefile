.PHONY: build run

CC = gcc
CFLAGS = `pgk-config --cflags --libs gtk+-3.0 webkit2gtk+-4.1`
LDFLAGS = -ldl
OUT = output
SRC = main.c \
			src/widget.c \
			src/filesystem.c
ARGS = -Iinclude	 \
			 -O2				 

GTKFLAGS = -export-dynamic `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.1`

build:
	$(CC) $(SRC) $(ARGS) $(GTKFLAGS) $(LDFLAGS) -o $(OUT) 
run: build
	WEBKIT_DISABLE_COMPOSITING_MODE=1 GDK_BACKEND=x11 ./$(OUT)	

