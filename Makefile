.POSIX:

include config.mk

SRC = xstr.c ulist.c pool.c log.c utils.c paths.c \
			autark.c project.c
OBJ = $(SRC:.c=.o)
HDRS = config.h basedefs.h alloc.h xstr.h ulist.h pool.h log.h utils.h paths.h \
			 env.h autark.h project.h scriptx.h

all: autark libs
	#@[ "$(TESTS)" = "1" ] && $(MAKE) -C ./tests;

libs: libautark.a

scriptx.h: scriptx.leg
	leg $< > $@

.c.o:
	$(CC) $(ACFLAGS) -c $<

$(OBJ) main.o: main.c config.mk $(HDRS)

autark: $(OBJ) main.o
	$(CC) -o $@ $^ $(ALDFLAGS)

libautark.a: $(OBJ)
	$(AR) rcs $@ $^
	#@[ "$(TESTS)" = "1" ] && $(MAKE) -C ./tests all;

clean:
	rm -f autark $(OBJ) main.o libautark.a
	#$(MAKE) -C ./tests clean

test: all
	$(MAKE) -C ./tests test

.PHONY: all libs clean test