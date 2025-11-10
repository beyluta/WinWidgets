.PHONY: debug release run format

CC = gcc
BUILDDIR = build
OUT = $(BUILDDIR)/WinWidgets
SRC = main.c \
			src/filesystem.c \
			src/utils.c \
			src/parser.c \
			lib/minimal-json-c-parser/src/json.c
RELEASE = -Werror \
					-Wextra \
					-Wall

# ------- 
# Building for Windows platform
# --------
ifeq ($(OS), Windows_NT)
ARGS := -Iinclude \
				-Iinclude/windows \
				-Ilib/minimal-json-c-parser/include \
				-isystem lib/WebView2/build/native/include \
				-O3 \
				-xc \
				-std=c23 \
				-mwindows
LDFLAGS := -lole32 \
					 -loleaut32 \
					 -luuid \
					 -lddraw \
					 -ldxguid \
					 -ldwmapi \
					 -Llib/WebView2/build/native/x64 \
					 -lWebView2Loader \
					 -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
SRC := $(SRC) \
			 src/windows/widget.c
RESRC = "$(CURDIR)/src/windows/resources.o"

format:
	windres "$(CURDIR)/src/windows/resources.rc" "$(CURDIR)/src/windows/resources.o"
	@if not exist $(BUILDDIR) mkdir $(BUILDDIR)
	- robocopy "$(CURDIR)/assets" "$(BUILDDIR)/assets" /E
	copy "$(CURDIR)\lib\WebView2\build\native\x64\WebView2Loader.dll" "$(CURDIR)\$(BUILDDIR)\"
	clang-format -i "$(CURDIR)/src/*.c" "$(CURDIR)/include/*.h" "$(CURDIR)/main.c"
debug: format
	$(CC) $(RESRC) $(SRC) $(ARGS) $(LDFLAGS) -o $(OUT)
release: format
	$(CC) $(RESRC) $(SRC) $(ARGS) $(RELEASE) $(LDFLAGS) -o $(OUT)
run:
	./$(OUT)

# ------- 
# Building for Linux platform
# --------
else ifeq($(UNAME), Linux)
GTKFLAGS = -export-dynamic `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.1`
ARGS := -Iinclude \
				-Ilib/minimal-json-c-parser/include \
				-O2 \
				-xc \
				-std=c23 \
				-D_POSIX_C_SOURCE=200809L
LDFLAGS = -ldl
SRC := $(SRC) \
			 src/linux/widget.c

format:
	rm -r "$(BUILDDIR)"
	mkdir -p "$(BUILDDIR)"
	cp -r "$(CURDIR)/assets" "$(BUILDDIR)/assets"
	clang-format -i $(CURDIR)/src/*.c \
	$(CURDIR)/include/*.h \
	$(CURDIR)/main.c
debug: format
	$(CC) $(SRC) $(ARGS) $(GTKFLAGS) $(LDFLAGS) -o $(OUT)
release: format
	$(CC) $(SRC) $(ARGS) $(RELEASE) $(GTKFLAGS) $(LDFLAGS) -o $(OUT)
run:
	WEBKIT_DISABLE_COMPOSITING_MODE=1 GDK_BACKEND=x11 ./$(OUT)	
endif
