RUBY.TOS: atari_main.o
	$(CC) -o $@ -L$(CURDIR)/../src $< -lmrubyc.a
	$(STRIP) $@

atari_main.o: atari_main.c
	$(CC) -c -o $@ -I$(CURDIR)/../src -I$(CURDIR)/../src/hal_posix $<
