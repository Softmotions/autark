.POSIX:

include ../config.mk

EXEC_SRCS := test_script.c
AKLIB := ../libautark.a
CDEPS := Makefile ../config.mk $(AKLIB) ./test_utils.h
EXECS := $(EXEC_SRCS:.c=)
EXEC_OBJS := $(EXEC_SRCS:.c=.o)

all: $(EXECS)

$(EXECS): $(EXEC_OBJS)

$(EXECS): %: %.o
	$(CC) $(ACFLAGS) $< $(AKLIB) $(ALDFLAGS) -o $@

%.o: %.c $(CDEPS)
	$(CC) $(ACFLAGS) -c $< -o $@

clean:
	-rm -f $(EXECS) $(EXEC_OBJS)

test: all
	@for exec in $(EXECS); do \
		echo "Running $$exec..."; \
		./$$exec; \
	done

.PHONY: all clean test