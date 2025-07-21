# Autark – A self-contained build system for C/C++

**Autark is a self-contained build system for C/C++ projects that requires only the `dash` shell and a `cc` compiler to work!**

The project build initialization script `build.sh` automatically setups the Autark build system, then your project is built using Autark.
Autark handles both internal and external project dependencies much more precisely and cleanly than is typically done with Make or CMake.
Build rules are defined using a specialized DSL in Autark files, which is mostly declarative in nature.

## Key Features of Autark

- To initialize the project build system on the target system, nothing is required except a C99-compliant compiler.
- The build process does not modify the project's source tree in any way.
- Build rules are described using a simple and clear declarative DSL, which is not a programming language.
- The system provides extensive capabilities for extending the build process with custom rules.
- Parallel compilation of C/C++ source files is supported.


## Brief Overview of Autark

An Autark script is a specialized DSL with modest capabilities, yet sufficient for writing concise and elegant build scripts.
The syntax is simple and can be informally described as follows:

- A script consists of a set of rules.
- A rule follows this syntax:

```

RULE:

  rule_name { RULE | LITERAL }

LITERAL:

  text | 'single quoted text' | "double quoted text"

```

- Rules and literals are separated by whitespace.
- Every rule has a body enclosed in curly braces `{}`.
- Rules form lists, similar to syntactic structures in Scheme or Lisp.

More formal syntax decription can be found here: https://github.com/Softmotions/autark/blob/master/scriptx.leg

## Sample Autark script

Below is a demonstration of an Autark script from the demo project:
https://github.com/Softmotions/autark-sample-project
Take a look and try building it!

You can also explore the following real-life C projects that use Autark:
- https://github.com/Softmotions/iwnet
- https://github.com/Softmotions/ejdb

```cfg
# https://github.com/Softmotions/autark-sample-project
#
# This is a sample project demonstrating the Autark build system.

# `meta` is a generic rule that sets all specified key/value pairs under the META_ prefix (except the `let` clause).
# You can use any variables set in the meta section anywhere in your build scripts.
meta {
  # Available in the script context under the META_NAME key.
  name { Hello }
  artifact { hello }
  description { Sample project demonstrating the Autark build system. }
  website { https://github.com/Softmotions/autark-sample-project }
  sources { https://github.com/Softmotions/autark-sample-project.git }
  license { MIT }
  version { 1.0.0 }
}

# Option is a small description of build switch which can be set by system env and or -D flag:
# ENABLE_DEBINFO=1 ./build.sh
# or ./build.sh -DENABLE_DEBINFO=1
option { ENABLE_DEBINFO           Generate debuginfo even in release mode }
option { HELLO_MSG                Hello message provided by libhello }

# The `check` rule takes a list of script files that must be located in the `.autark/` directory relative to
# the current Autark script file. A check script is a dash shell (`sh`) script that runs during the early stage
# of the build process (`init` phase). It verifies system requirements, locates required software or libraries,
# and sets variables that will be available in the Autark script. Check scripts typically use `autark set` to
# define variables used in the build process. You may specify multiple check rules.
check {
  system.sh
}

# Sets the BUILD_TYPE variable, using a previously defined value if available,
# or falling back to the default value 'Release'.
# Note: Variables set by this rule are only available within the current Autark script and its included scripts.
# Note: `set` rules are evaluated lazily.
set {
  BUILD_TYPE
  # Looks up the value of BUILD_TYPE in the current context, then in parent contexts,
  # and finally in system environment variables.
  # If not found, 'Release' will be used as the default.
  ${BUILD_TYPE Release}
}

if { !defined { HELLO_MSG }
  set { HELLO_MSG 'Hello from libhello!' }
}

echo {
  # This message is printed in 'build' phase
  Building my hello app...
  # Printed in first build 'init' stage
  #init {
  #  Project build initialized
  #}
}

set {
  CFLAGS
  # Just build the compiler flag like: -DBUILD_TYPE=Release
  # ^{...} is a string join rule.
  # Note: the space after: '-DBUILD_TYPE=' is required by syntax rules of Autark script.
  ^{-DBUILD_TYPE= ${BUILD_TYPE}}

  # Conditions can be placed anywhere in the build script.
  # This effectively performs tree shaking on the build script's syntax tree.
  if { prefix { ${BUILD_TYPE} Release }
    -O2
    # The condition if { ${...} } evaluates to true if and only if
    # the argument is not an empty string and is not equal to "0".
    if { ${ENABLE_DEBINFO}
      -g
    }
  } else {
    -DDEBUG=1
    -O0
    -g
  }
  # Merge this set of flags with any previously defined CFLAGS if any.
  # Note: The `..` prefix indicates that the elements of ${CFLAGS}
  # will be added to this set, similar to spread syntax in JavaScript.
  ..${CFLAGS}
}

# Note LDFLAGS can be specified in system env variable: `LDFLAGS=... ./build.sh`
set {
  LDFLAGS
  ..${LDFLAGS}
}

set {
  SOURCES
  main.c
  utils.c
}

# Now let's define a rule for compiling C source files.
# The generic form of the `cc` rule:
#
# cc {
#   SOURCES
#   [FLAGS]
#   [COMPILER]
#   [consumes { ... }]  Outputs of other rules this one depends on
#   [objects { NAME }]  Defines the variable name where the list of compiled object files is stored. Defaults to CC_OBJS.
# }
#
# This rule compiles the given source files and produces a set of object (.o) files.
cc {
  ${SOURCES}
  # Here we intentionally show an anonymous `set {..}` using `_` as a placeholder for the variable name.
  # This is used to extend CFLAGS with additional arguments directly in-place.
  set { _ ${CFLAGS} -I ./libhello }
  # Note: CC is set by system.sh check script.
  ${CC}
  consumes {
    # The output of the 'configure' rule applied to hello.h.in
    libhello/hello.h
  }
}

# Include the scipt which build libhello.a static library
include { libhello/Autark }

# Now link the final executable application.
# Note: LIBHELLO_A variable is set by child 'libhello/Autark'
#
# The generic form of the `run` rule:
# run {
#   [always]
#   [exec  {...}] ...
#   [shell {...}] ...
#   [consumes{...}]
#   [produces{...}]
# }
run {
  exec { ${CC} -static ${LDFLAGS} -o ${META_ARTIFACT} ${CC_OBJS} ${LIBHELLO_A} }
  consumes {
    ${CC_OBJS}
    ${LIBHELLO_A}
  }
  produces {
    ${META_ARTIFACT}
  }
}

# Installs executable artifact to the INSTALL_BIN_DIR relative to installation prefix.
install {
  ${INSTALL_BIN_DIR}
  ${META_ARTIFACT}
}
```

https://github.com/Softmotions/autark-sample-project


## Autark reference
