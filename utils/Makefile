.PHONY: all

all: pngtoc ttf-to-c

pngtoc: pngtoc.c
	gcc $^ -o $@ -lpng

ttf-to-c: ttf-to-c.c
	gcc -Wall $^ `pkg-config --cflags --libs freetype2` -o $@
