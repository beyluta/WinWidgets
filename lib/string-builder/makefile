.PHONY: clean build test

CC = clang
SRC = src/string-builder.c \
			main.c
INCL = -Iinclude
CFLAGS = -std=c23 \
				 -Wall \
				 -Werror \
				 -Wextra
BUILD = build
OUT = output

clean:
	rm -rf build
build: clean
	mkdir -p "$(BUILD)"
	$(CC) $(SRC) $(INCL) $(CFLAGS) -o "$(BUILD)/$(OUT)"
test: build
	./"$(BUILD)/$(OUT)"
