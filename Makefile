CC = gcc
CFLAGS = -Wall -Wextra -g -I./src
LDFLAGS = -lSDL3
SRC = $(wildcard src/audio/*.c) $(wildcard src/cpu/*.c) $(wildcard src/display/*.c) $(wildcard src/input/*.c) $(wildcard src/*.c) 
OBJ = $(patsubst src/%.c, build/%.o, $(SRC))
TARGET = gba

all: $(TARGET)

build/%.o: src/%.c 
	mkdir -p build/audio build/cpu build/display build/input
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf build $(TARGET)
