CC=clang

mktrayicon: mktrayicon.c
	${CC} -g `pkg-config --cflags gtk+-3.0` -o $@ $< `pkg-config --libs gtk+-3.0`
