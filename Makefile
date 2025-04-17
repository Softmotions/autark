.POSIX:

include config.mk

SRC = xstr.c \
			ulist.c \
			pool.c \
			log.c \
			utils.c \
			map.c \
			paths.c \
			autark.c \
			script.c \
			spawn.c \
			node_script.c \
			node_meta.c \
			node_consumes.c \
			node_check.c \
			node_sources.c \
			node_flags.c \
			node_artifact.c \
			node_include.c \
			node_if.c \
			node_subst.c


OBJ = $(SRC:.c=.o)
HDRS = config.h \
			 basedefs.h \
			 alloc.h \
			 xstr.h \
			 ulist.h \
			 map.h \
			 pool.h \
			 log.h \
			 utils.h \
			 paths.h \
			 env.h \
			 autark.h \
			 script.h \
			 spawn.h \
			 scriptx.h \
			 nodes.h

all: autark libs
	@[ "$(TESTS)" = "1" ] && $(MAKE) -C ./tests;

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

clean:
	rm -f autark $(OBJ) main.o libautark.a
	$(MAKE) -C ./tests clean

test: all
	$(MAKE) -C ./tests test

.PHONY: all libs clean test