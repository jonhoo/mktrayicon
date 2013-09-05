mktrayicon: mktrayicon.c
	${CC} `pkg-config --cflags gtk+-3.0` -o $@ $< `pkg-config --libs gtk+-3.0`

clean:
	rm mktrayicon
