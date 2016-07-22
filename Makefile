CPPFLAGS = -DNDEBUG
CFLAGS = -std=c99 -Wall -pedantic -Wextra -Wshadow -Wconversion

all: fancontroller_linux

fancontroller_linux: fancontroller_linux.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

clean:
	$(RM) fancontroller_linux
