.POSIX:

include ../config.mk

EXEC_SRCS := test1.c test2.c test3.c test4.c test5.c test6.c test7.c
AKLIB := ../libautark.a
CDEPS := Makefile ../config.mk $(AKLIB) ./test_utils.h
EXECS := $(EXEC_SRCS:.c=)
EXEC_OBJS := $(EXEC_SRCS:.c=.o)

all: $(EXECS)

$(EXECS): $(EXEC_OBJS)

$(EXECS): %: %.o
	$(CC) $(ACFLAGS) $< $(AKLIB) $(ALDFLAGS) -o $@

%.o: %.c $(CDEPS)
	$(CC) -I../ $(ACFLAGS) -c $< -o $@

clean:
	-rm -f $(EXECS) $(EXEC_OBJS)

test: all
	@for exec in $(EXECS); do \
		echo "Running $$exec..."; \
		$(MEM_CHECKER) ./$$exec; \
	done

.PHONY: all clean test