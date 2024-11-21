CC := gcc

all: clean build

clean:
	if [ -f "./clox" ]; then rm ./clox; fi;

build:
	$(CC) -o clox *.c
