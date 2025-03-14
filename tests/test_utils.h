#pragma once

#include <stdlib.h>

#define _QSTR(x__) #x__
#define _Q(x__)    _QSTR(x__)

#define ASSERT(label__, expr__) \
        if (!(expr__)) {        \
          fputs(__FILE__ ":" _Q(__LINE__) " " _Q(expr__) "\n", stderr); \
          goto label__;         \
        }


