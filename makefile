.PHONY: debug release run prepare

CC = gcc
BUILDDIR = build
OUT = $(BUILDDIR)/WinWidgets
SRC = src/filesystem.c \
			src/utils.c \
			src/parser.c \
			src/sysinfo.c \
			lib/minimal-json-c-parser/src/json.c \
			lib/string-builder/src/string-builder.c \
			lib/c-yaml-parser/src/cyaml.c
INCL = -Ilib/minimal-json-c-parser/include \
			 -Ilib/string-builder/include \
			 -Ilib/c-yaml-parser/include
RELEASE = -Werror \
					-Wextra \
					-Wall

# ---------------------------------------------------------------------------
# Building for Windows platform
# ---------------------------------------------------------------------------
ifeq ($(OS), Windows_NT)
MINGW64 := C:/tools/msys64/mingw64
ARGS := -Iinclude \
				-Iinclude/windows \
				-I$(MINGW64)/include \
				-L$(MINGW64)/lib \
				-Llib/WebView2/build/native/x64 \
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
					 -lshlwapi \
					 -lcurl \
					 -lzip \
					 -lz \
					 -lWebView2Loader \
					 -lpthread \
					 -lstdc++
SRC := $(SRC) \
			 main.c \
			 src/windows/widget.c \
			 src/windows/remres.c \
			 src/windows/config.c
RESRC = "$(CURDIR)/src/windows/resources.o"
WEBVIEWURL = "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2"

prepare:
	@if not exist "$(CURDIR)/lib/WebView2" ( \
		mkdir "$(CURDIR)/lib/WebView2" && \
		curl.exe -L -o "$(CURDIR)/lib/WebView2.zip" "$(WEBVIEWURL)" && \
		powershell -command "Expand-Archive -Force -Path '$(CURDIR)/lib/WebView2.zip' -DestinationPath '$(CURDIR)/lib/WebView2'" && \
		powershell -command "Remove-Item -Force '$(CURDIR)/lib/WebView2.zip'" \
	)
	windres "$(CURDIR)/src/windows/resources.rc" "$(CURDIR)/src/windows/resources.o"
	@if not exist $(BUILDDIR) mkdir $(BUILDDIR)
	- robocopy "$(CURDIR)/assets" "$(BUILDDIR)/assets" /E
	copy "$(CURDIR)\lib\WebView2\build\native\x64\WebView2Loader.dll" "$(CURDIR)\$(BUILDDIR)"
	copy "$(MINGW64)\bin\*.dll" "$(CURDIR)\$(BUILDDIR)" /Y
	clang-format -i "$(CURDIR)/src/*.c" "$(CURDIR)/include/*.h" "$(CURDIR)/main.c"
debug: prepare
	$(CC) $(RESRC) $(SRC) $(INCL) $(ARGS) $(LDFLAGS) -g -o $(OUT)
release: prepare
	$(CC) $(RESRC) $(SRC) $(INCL) $(ARGS) $(RELEASE) $(LDFLAGS) -o $(OUT)
run:
	./$(OUT)

# ---------------------------------------------------------------------------
# Building for Linux platform
# ---------------------------------------------------------------------------
else ifeq($(UNAME), Linux)
GTKFLAGS = -export-dynamic `pkg-config --cflags --libs gtk+-3.0 appindicator3-0.1 x11 webkit2gtk-4.1`
ARGS := -Iinclude \
				-Iinclude/linux \
				-O2 \
				-xc \
				-std=c23 \
				-D_POSIX_C_SOURCE=200809L
LDFLAGS = -ldl
SRC := $(SRC) \
			 src/linux/widget.c \
			 src/linux/window.c

prepare:
	rm -rf "$(BUILDDIR)"
	mkdir -p "$(BUILDDIR)"
	cp -r "$(CURDIR)/assets" "$(BUILDDIR)/assets"
	clang-format -i $(CURDIR)/src/*.c \
	$(CURDIR)/include/*.h \
	$(CURDIR)/main.c
debug: prepare
	$(CC) $(SRC) $(INCL) $(ARGS) $(GTKFLAGS) $(LDFLAGS) -g -o $(OUT)
release: prepare
	$(CC) $(SRC) $(INCL) $(ARGS) $(RELEASE) $(GTKFLAGS) $(LDFLAGS) -o $(OUT)
run:
	./$(OUT)	
endif
