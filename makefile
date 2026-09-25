.SILENT:
.PHONY: all prepare

# Desired compiler
CC = gcc

# Sources
SRC_DIR = src
SRC = $(wildcard $(SRC_DIR)/*.c) \
			lib/minimal-json-c-parser/src/json.c \
			lib/c-yaml-parser/src/cyaml.c

# Build dir and output names
BUILD_DIR = build
TARGET = WinWidgets

# Build artifacts
OBJS_DIR = $(BUILD_DIR)/objs
OBJS = $(patsubst %.c, $(OBJS_DIR)/%.o, ${SRC})
DEPS = $(patsubst %.c, $(OBJS_DIR)/%.d, ${SRC})

# Compiler flags
CFLAGS := -MMD \
				  -MP \
					-Wextra \
					-Wall \
					-Iinclude \
				  -Ilib/minimal-json-c-parser/include \
			 	  -Ilib/c-yaml-parser/include

# ---------------------------------------------------------------------------
# Building for Windows platform
# ---------------------------------------------------------------------------
ifeq ($(OS), Windows_NT)
MINGW64 := C:/tools/msys64/mingw64
CFLAGS := $(CFLAGS) \
				-Iinclude/windows \
				-I$(MINGW64)/include \
				-isystem lib/WebView2/build/native/include \
				-O3 \
				-xc \
				-std=c23

LDFLAGS := -L$(MINGW64)/lib \
					 -Llib/WebView2/build/native/x64 \
					 -mwindows \
					 -lole32 \
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
					 -lstdc++ \
			  	 "$(CURDIR)/src/windows/resources.o"
SRC := $(SRC) \
			 main.c \
			 src/windows/widget.c \
			 src/windows/remres.c \
			 src/windows/config.c \
			 src/windows/routine.c
WEBVIEWURL = "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2"

all: $(BUILD_DIR)/$(TARGET)
	echo Finished compiling $(TARGET) to directory $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJS_DIR)/%.o: %.c
	if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CC) -c $(CFLAGS) -o $@ $<

prepare:
	@if not exist "$(CURDIR)/lib/WebView2" ( \
		mkdir "$(CURDIR)/lib/WebView2" && \
		curl.exe -L -o "$(CURDIR)/lib/WebView2.zip" "$(WEBVIEWURL)" && \
		powershell -command "Expand-Archive -Force -Path '$(CURDIR)/lib/WebView2.zip' -DestinationPath '$(CURDIR)/lib/WebView2'" && \
		powershell -command "Remove-Item -Force '$(CURDIR)/lib/WebView2.zip'" \
	)
	windres "$(CURDIR)/src/windows/resources.rc" "$(CURDIR)/src/windows/resources.o"
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	- robocopy "$(CURDIR)/assets" "$(BUILD_DIR)/assets" /E
	copy "$(CURDIR)\lib\WebView2\build\native\x64\WebView2Loader.dll" "$(CURDIR)\$(BUILD_DIR)"
	copy "$(MINGW64)\bin\*.dll" "$(CURDIR)\$(BUILD_DIR)" /Y
	clang-format -i "$(CURDIR)/src/*.c" "$(CURDIR)/include/*.h" "$(CURDIR)/main.c"

# ---------------------------------------------------------------------------
# Building for Linux platform
# ---------------------------------------------------------------------------
else
GTKFLAGS = -export-dynamic `pkg-config --cflags --libs gtk+-3.0 appindicator3-0.1 x11 webkit2gtk-4.1`
CFLAGS := $(CFLAGS) \
				-Iinclude/linux \
				-O2 \
				-xc \
				-std=c23 \
				-D_POSIX_C_SOURCE=200809L
LDFLAGS = -ldl
SRC := $(SRC) \
			 src/linux/widget.c \
			 src/linux/window.c

make: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	mkdir -p "$(dir $@)"
	$(CC) -o $@ $^ $(LDFLAGS) $(GTKFLAGS)

$(OBJS_DIR)/%.o: %.c
	mkdir -p "$(dir $@)"
	$(CC) -c $(CFLAGS) $(GTKFLAGS) -o $@ $<

prepare:
	rm -rf "$(BUILD_DIR)"
	mkdir -p "$(BUILD_DIR)"
	cp -r "$(CURDIR)/assets" "$(BUILD_DIR)/assets"
	clang-format -i $(CURDIR)/src/*.c \
	$(CURDIR)/include/*.h \
	$(CURDIR)/main.c
endif

-include $(DEPS)
