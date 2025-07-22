# Autark – A self-contained build system for C/C++

**Autark is a self-contained build system for C/C++ projects that requires only the `dash` shell and a `cc` compiler to work!**

Project build start script `build.sh` automatically setups the Autark build system, then your project is built using Autark.
Autark handles both internal and external project dependencies much more precisely and cleanly than is typically done with Make or CMake.
Build rules are defined using a specialized DSL in Autark files, which is mostly declarative in nature.

## Key Features of Autark

- To initialize the project build system on the target system, nothing is required except a C99-compliant compiler.
- The build process does not modify the project's source tree.
- Build rules are described using a simple and clear declarative DSL, which is not a programming language.
- The system provides extensive capabilities for extending the build process with custom rules.
- Parallel compilation of C/C++ source files is supported.

## Installation

To use Autark in your project, simply copy the latest version of Autark:

```sh
cd <root project folder>
wget -O ./build.sh \
  https://raw.githubusercontent.com/Softmotions/autark/refs/heads/master/dist/build.sh
chmod u+x ./build.sh
```

Then, write your Autark build script. The best way to get started is to look at the [Autark sample project](https://github.com/Softmotions/autark-sample-project)
or explore real-life projects that use Autark.

Built artifacts are placed in `./autark-cache` dir by default.

```sh
./build.sh -h                                                                                                                                                                                                                ✘ 1 main
Usage

Common options:
    -V, --verbose               Outputs verbose execution info.
    -v, --version               Prints version info.
    -h, --help                  Prints usage help.

autark [sources_dir/command] [options]
  Build project in given sources dir.
    -H, --cache=<>              Project cache/build dir. Default: ./autark-cache
    -c, --clean                 Clean build cache dir.
    -l, --options               List of all available project options and their description.
    -J  --jobs=<>               Number of jobs used in c/cxx compilation tasks. Default: 4
    -D<option>[=<val>]          Set project build option.
    -I, --install               Install all built artifacts
    -R, --prefix=<>             Install prefix. Default: $HOME/.local
        --bindir=<>             Path to 'bin' dir relative to a `prefix` dir. Default: bin
    ...
```

## Brief Overview of Autark

Autark script is a specialized DSL with modest capabilities, yet sufficient for writing concise and elegant build scripts.
The syntax is simple and can be informally described as follows:

- A script consists of a set of rules.
- A rule follows this syntax:

```

RULE:

  rule_name { RULE | LITERAL }

LITERAL:

  text | 'single quoted text' | "double quoted text"

```

- Rules and literals are separated by `whitespaces`.
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

# The `check` rule takes a list of script files that should be located in the `.autark/` directory relative to
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
#   [CONSUMES { ... }]  Outputs of other rules this one depends on
#   [OBJECTS { NAME }]  Defines the variable name where the list of compiled object files is stored. Defaults to CC_OBJS.
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

## Autark concepts and Project Build Lifecycle

Autark executes rules defined in Autark script files.
The build process goes through the following stages: `init`, `setup`, `build`, and `post_build`.

For each stage, the rules specified in the scripts are executed sequentially, from top to bottom.
However, during the `build` stage, some rules may depend on the results of other rules.
In such cases, the execution order is adjusted to ensure that dependencies are resolved correctly.

The project build process goes through the following phases:

### init

Initialization of the build process.

During the `init` phase, the following steps occur:

- Autark build script files are parsed.
- Initialization logic for rules is executed in sequence so it makes sence order of `set`, `check`, `if`
  rules build script.
- Based on the current state of variables, **tree shaking** is applied to the syntax tree
  of the overall Autark build script.
  As a result, all conditional `if` rules and certain helper rules such as `in-source` and `foreach`
  are removed from the syntax tree completely.

### setup

At this stage, the hierarchical structure of rules (the syntax tree) is finalized and will no longer change.
This phase prepares the rules for the main `build` stage of the process.
You could think of this phase as a pre-build step of main build phase.

### build

This is the main phase of the project build process, where the actual work of the build rules is performed.
During this phase, dependencies between rules are tracked and established, as well as dependencies on
filesystem objects, project source files, environment variables, and more.

The execution order of rules is determined by their dependencies.
A rule typically will not perform its main function if all of its dependencies have already been satisfied
and the rule has been previously executed.

### post-build

This phase is executed after the `build` phase has completed successfully.
It is primarily used for rules that install the built project artifacts.


# Autark reference

## meta

Sets script variables using a simple convention:
each variable name is automatically prefixed with `META_`.
These variables are evaluated during the `init` phase of the build.

The `meta` rule is recommended for defining global variables such as
project version, name, description, maintainer contact information, etc.

These meta-variables are especially convenient to use in `configure` rules.

```cfg
meta {
  [<VARNAME SUFFIX> { ... }] ...
  let { <VARNAME> ... }
}
```

If variable is defined using the `let` clause inside `meta`,
the `META_` prefix is not added to its name.


## option

```cfg
option { <OPTION NAME> [OPTION DESCRIPTION] }
```

This rule declares a build option.
Build options are variables set at the start of the build to configure the desired output.
These variables can be defined either as environment variables:

```sh
BUILD_TYPE=Release BUILD_SHARED_LIBS=1 ./build.sh
```

or as command-line arguments:

```sh
./build.sh -DBUILD_TYPE=Release -DBUILD_SHARED_LIBS=1
```

If you don't use the variable values in shell scripts during the build,
the second method is preferred - it avoids polluting the environment
and prevents these variables from leaking into subprocesses where they might be unnecessary
or accidentally override something.

To list all documented build options, use:
```sh
./build.sh -l
```

## check

This construct is the workhorse for checking system configuration and capabilities
before compiling the project. Checks are performed using `dash` shell scripts.

The main purpose of a check script is to set variables used in Autark scripts.
Additionally, check scripts can declare dependencies on files and environment variables,
and are automatically re-executed when some of dependencies are changed.
(This is handled by invoking the `autark` command from within the check script.)

You can find a collection of useful check scripts at:
https://github.com/Softmotions/autark/tree/master/check_scripts
Feel free to copy any scripts that are relevant to your project.

```cfg
check {
  [SCRIPT] ...
}
````

Check scripts must be located in the `./autark` directory relative to the script that references them,
or in similar directories of parent scripts.

For small projects, it is convenient to place all check scripts in the `.autark` directory
at the root of your project.

Within the context of a check script, the `autark` command is available.
It allows the script to define variables or register dependencies - so that if any of those dependencies change,
the check script will be automatically re-executed.

```sh
autark set <key>=<value>
  Sets key/value pair as output for check script.

autark dep <file>
  Registers a given file as dependency for check script.

autark env <env>
  Registers a given environment variable as dependency for check script.
```

Check script example:

https://github.com/Softmotions/autark/blob/master/check_scripts/test_symbol.sh

```sh
#!/bin/sh

SYMBOL=$1
HEADER=$2
DEFINE_VAR=$3

usage() {
  echo "Usage test_symbol.sh <SYMBOL> <HEADER> <DEFINE_VAR>"
  exit 1
}

[ -n "$SYMBOL" ] || usage
[ -n "$HEADER" ] || usage
[ -n "$DEFINE_VAR" ] || usage

cat > "test_symbol.c" <<EOF
#include <$HEADER>
int main() {
#ifdef $SYMBOL
  return 0; // Symbol is a macro
#else
  (void)$SYMBOL; // Symbol might be a function or variable
  return 0;
#endif
}
EOF

if ${CC:-cc} -Werror ./test_symbol.c; then
  autark set "$DEFINE_VAR=1"
fi

# Re-run script if CC system environment changed
autark env CC
```

The results of check script execution can also be used not only in variables but in project header templates,
such as `config.h.in`. This is typically done via `configure` rules in the Autark script:

```cfg
check {
  test_symbol.sh { CLOCK_MONOTONIC time.h IW_HAVE_CLOCK_MONOTONIC }
}
...

configure {
  config.h.in
}
```

Autark will replace `//autarkdef` lines with the appropriate #define or comment them out,
depending on whether the variable is defined or not.

config.h.in:
```c
//autarkdef IW_HAVE_CLOCK_MONOTONIC 1
...
```

Or you may use variable directly in CFLAGS

```cfg
set {
 CFLAGS
 if { ${IW_HAVE_CLOCK_MONOTONIC}
   -DIW_HAVE_CLOCK_MONOTONIC=1
 }
 ...
}
```

## set

The `set` rule assigns a value to a variable in the build script.

```cfg
set {
   | NAME
   | _
   | parent { NAME }
   | root { NAME }

   [VALUES] ...
}
```

`set` is a lazily evaluated rule - the value of set is only computed if the variable or expression is actually used.

The most common form is `set { NAME [VALUES]... }`

Note that in Autark, all variables are either strings or lists. A list is just a string internally,
encoded in a special form: `\1VAL_ONE\1VAL_TWO\1...`, where list values are separated by the `\1` character.

If there is only one value, e.g. `set { foo bar }`, then the variable foo is evaluated as `"bar"` string.
If you write `set { foo bar baz }`, the value of `foo` becomes a list, encoded as `\1bar\1baz`.

`Set` is an expression actually and can be used inline in other constructs. For example:

```cfg
cc {
  ${SOURCES}
  set { _ ${CFLAGS} -I./libhello }
  ...
}
```

Here, `CFLAGS` is extended with an additional flag `-I./libhello` directly in-place using an anonymous set
where `_` used in place of variable name.

By default, variables defined with set are visible only in the current Autark script
and any child scripts included by `include` directive.

To define or modify a variable in the parent's script context, use:

```cfg
set {
  parent {
    NAME
  }
}
```

To define a variable in the root script context:

```cfg
set {
  root {
    NAME
  }
}
```

## ${..} Variable Evaluation

```cfg
${VARIABLE [DEFAULT]}`
```

The expression `${VARIABLE [DEFAULT]}` is used when the value of a variable needs to be passed
as an argument to another rule.
If the variable is not defined in the current script context, the `DEFAULT` value will be used if provided.


## @{..} Program Output Evaluation

```cfg
@{PROGRAM [ARG1 ARG2 ...]}
```

Invokes the specified program `PROGRAM` and returns its standard output as a string.

### Example:

```cfg
set {
  LDFLAGS
  ..@{${PKGCONF pkgconf} --libs --static libcurl}
  ..${LDFLAGS}
}
```
In this example, the output of `pkgconf --libs --static libcurl` is appended to the `LDFLAGS` list.


## ^{..} Expressions Concatenation

```cfg
^{EXPR1 [EXPR2]...}
```

Concatenates expressions output.
Here we encounter an important syntactic rule in Autark scripts: all rules must be separated by whitespace.
Because of this, the following code is invalid:

```cfg
set {
 CFLAGS
 -DBUILD_TYPE=${BUILD_TYPE} # Wrong!
}
```

In this case, the Autark interpreter treats `-DBUILD_TYPE=$` as a rule name,
which is not what we intend.

To correctly construct the string `-DBUILD_TYPE=${BUILD_TYPE}` using the variable value from the script context,
you must use the string concatenation rule:

```cfg
set {
  CFLAGS
  ^{-DBUILD_TYPE= ${BUILD_TYPE}}
```

Note: the space between `-DBUILD_TYPE=` and `${BUILD_TYPE}` is required.

## if{..} condition

Conditional directive.

```cfg
  if {
    CONDITION_RULE
    EXPR1
  } [  else { EXPR2 } ]
```

Examples:

```cfg
if { or { ${BUILD_BINDING_JNI} ${BUILD_BINDING_NODEJS} }
  include { bindings/Autark }
}

...

if { ${EJDB_BUILD_SHARED_LIBS}
  if {!defined {SYSTEM_DARWIN}
    set {
      LIBEJDB_SO_BASE
      libejdb2.so
    }
  }
  ...
}
```

If the condition `CONDITION_RULE` evaluates to a `truthy` value,
the entire `if` expression is replaced by `EXPR1` in the Autark script’s instruction tree.
Otherwise, if an `else` block is provided, it will be replaced by `EXPR2`.

### Condition Forms

Exclamation mark `!` means expression result negation, when trufly evaluated expressions became false.

`[!]${EXPR}`
<br/>Evaluates as truthy in any of the following cases:

- The result of `EXPR` is a non-empty string and not equal to `"0"`.
- The result is a list with a single element that is not empty and not `"0"`.
 - The result is a list with more than one element, regardless of content.


`[!]defined { VARIABLE }`
<br/>Truthy if the specified variable is defined in the current script context.


`[!]eq { EXPR1 EXPR2 }`
<br/>Truthy if the string value of `EXPR1` is equal to `EXPR2`.


`[!]prefix { EXPR1 EXPR2 }`
<br/>Truthy if the string value of EXPR2 is a prefix of EXPR1.


`[!]contains { EXPR1 EXPR2 }`
<br/>Truthy if EXPR1 contains EXPR2 as a substring.


`[!]or { EXPR1 ... EXPRN }`
<br/>Truthy if any of the expressions inside the directive is truthy
(according to the rules defined above).


`[!]and { EXPR1 ... EXPRN }`
<br/>Truthy if all expressions inside the directive are truthy.


Please Note: Autark syntax does not support `else if` constructs.