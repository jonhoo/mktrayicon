mktrayicon: mktrayicon.c
	${CC} `echo $${DEBUG+-DDEBUG}` `pkg-config --cflags gtk+-3.0` -o $@ $< `pkg-config --libs gtk+-3.0` `pkg-config --cflags --libs x11`

clean:
	rm mktrayicon
