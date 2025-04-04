PREFIX ?= /usr/local

TESTS := 1
DEBUG := 1
#MEM_CHECKER := valgrind --leak-check=full --error-exitcode=1

CC = clang

CFLAGS = -std=c99 -ggdb -O0 -D_XOPEN_SOURCE=600 -D_DEFAULT_SOURCE \
         -Wall -Wextra -Wpedantic \
         -Wno-typedef-redefinition \
         -Wno-sign-compare \
         -Wno-gnu-label-as-value \
         -Wno-unused-parameter

ifeq ($(DEBUG), 1)
  CFLAGS += -g -DDEBUG
else
  CFLAGS += -DNDEBUG
endif

ifeq ($(TESTS), 1)
  CFLAGS += -DTESTS
endif

LIBS=-lm
ALDFLAGS = $(LIBS) $(LDFLAGS)
ACFLAGS = $(CFLAGS)