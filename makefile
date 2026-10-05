.PHONY: all prepare

# Desired compiler
CC = gcc

# Sources
SRC_DIR = src
SRC = $(wildcard $(SRC_DIR)/*.c) \
			lib/minimal-json-c-parser/src/json.c \
			lib/c-yaml-parser/src/cyaml.c

# Scripts directory
SCRIPTS_DIR = scripts
BUILD = out

# LLAMA Library
LLAMA_DIR = lib/llama.cpp
LLAMA_LIBS_DIR = $(LLAMA_DIR)/build/bin
LLAMA_LIBS = *.so*

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
			 $(wildcard $(SRC_DIR)/windows/*.c)
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
	clang-format -i "$(CURDIR)/src/*.c" "$(CURDIR)/include/*.h"
	$(CC) "$(SCRIPTS_DIR)/build.c" -o "$(BUILD_DIR)/$(BUILD)"
	cmd /c "$(CURDIR)/$(BUILD_DIR)/$(BUILD).exe" "$(CURDIR)/assets/index.html" _WIN32
	move /Y "index.html" "$(BUILD_DIR)/assets/index.html"

# ---------------------------------------------------------------------------
# Building for Linux platform
# ---------------------------------------------------------------------------
else
GTKFLAGS = -export-dynamic `pkg-config --cflags --libs gtk+-3.0 appindicator3-0.1 x11 webkit2gtk-4.1`
CFLAGS := $(CFLAGS) \
				-Iinclude/linux \
				-Ilib/llama.cpp/include \
				-Ilib/llama.cpp/ggml/include \
				-O2 \
				-xc \
				-std=c23 \
				-D_POSIX_C_SOURCE=200809L
LDFLAGS = -Wl,-rpath,'$$ORIGIN' \
					-ldl \
					-Llib/llama.cpp/build/bin \
					-l:libllama.so
SRC := $(SRC) \
			 $(wildcard $(SRC_DIR)/linux/*.c)

make: $(SCRIPTS_DIR)/$(BUILD)

$(SCRIPTS_DIR)/$(BUILD): $(LLAMA_LIBS_DIR)/$(LLAMA_LIBS)
	clang-format -i $(CURDIR)/src/*.c \
	$(CURDIR)/include/*.h
	mkdir -p "$(BUILD_DIR)"
	rm -rf "$(BUILD_DIR)/assets"
	cp -r "$(CURDIR)/assets" "$(BUILD_DIR)/assets"
	$(CC) "$(dir $@)build.c" -o "$(BUILD_DIR)/$(BUILD)"
	./"$(BUILD_DIR)/$(BUILD)" "$(CURDIR)/assets/index.html" __linux__
	mv "$(CURDIR)/index.html" "$(BUILD_DIR)/assets"

$(LLAMA_LIBS_DIR)/$(LLAMA_LIBS): $(LLAMA_DIR)
	cp $@ "$(BUILD_DIR)"

$(LLAMA_DIR): $(BUILD_DIR)/$(TARGET)
	# cd $@ && \
	# 	mkdir -p build && \
	# 	cd build && \
	# 	cmake .. -DGGML_BUILD_SHARED_LIB=ON -DGGML_CUDA=OFF -DGGML_METAL=OFF -DGGML_SYCL=OFF -DGGML_OPENCL=OFF && \
	# 	cmake --build . --config Release

$(BUILD_DIR)/$(TARGET): $(OBJS)
	mkdir -p "$(dir $@)"
	$(CC) -o $@ $^ $(LDFLAGS) $(GTKFLAGS)

$(OBJS_DIR)/%.o: %.c
	mkdir -p "$(dir $@)"
	$(CC) -c $(CFLAGS) $(GTKFLAGS) -o $@ $<

endif

-include $(DEPS)
