.PHONY: build run

CC = gcc
CFLAGS = `pgk-config --cflags --libs gtk+-3.0 webkit2gtk+-4.1`
GTKFLAGS = -export-dynamic `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.1`
BUILDDIR = build
OUT = $(BUILDDIR)/output
SRC = main.c \
			src/filesystem.c

ifeq ($(OS), Windows_NT)
ARGS := -Iinclude \
				-Ilib/WebView2/build/native/include \
				-O2 \
				-xc \
				-std=c23
LDFLAGS := -lole32 \
					 -loleaut32 \
					 -luuid \
					 -Llib/WebView2/build/native/x64 \
					 -lWebView2Loader \
					 -mwindows
SRC := $(SRC) \
			 src/widget_windows.c

build:
	@if not exist $(BUILDDIR) mkdir $(BUILDDIR)
	copy "$(CURDIR)\lib\WebView2\build\native\x64\WebView2Loader.dll" "$(CURDIR)\$(BUILDDIR)\"
	$(CC) $(SRC) $(ARGS) $(LDFLAGS) -o $(OUT)
run: build
	./$(OUT)
else
ARGS := -Iinclude \
				-O2 \
				-xc \
				-Wall \
				-Werror \
				-Wextra \
				-std=c23 \
				-D_POSIX_C_SOURCE=200809L
LDFLAGS = -ldl
SRC := $(SRC) \
			 src/widget_linux.c

build:
	$(CC) $(SRC) $(ARGS) $(GTKFLAGS) $(LDFLAGS) -o $(OUT) 
run: build
	WEBKIT_DISABLE_COMPOSITING_MODE=1 GDK_BACKEND=x11 ./$(OUT)	
endif
