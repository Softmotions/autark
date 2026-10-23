#ifndef CXX_MODULES_H
#define CXX_MODULES_H

#ifndef _AMALGAMATE_
#include "script.h"
#include "spawn.h"
#include "deps.h"

#include <stdbool.h>
#endif

/// C++20 modules compilation support of `cxx`/`host-cxx` rules.
///
/// Module interface units are compiled before all other sources of the rule (in
/// their declaration order) and produce a Binary Module Interface (BMI) in the
/// project cache. BMI lookup of importers is compiler specific:
///
///  - clang: `-fprebuilt-module-path=<dir>`, BMI files are named after the
///    module name (`<name>.pcm`).
///  - gcc: a single module map file (`-fmodule-mapper`) mapping module names to
///    BMI files (`<name>.gcm`).
///
/// Module partitions are not supported.
struct cxx_modules;

/// @return true if `path` has an extension of a C++20 module unit
bool cxx_modules_is_module_path(const char *path);

/// Creates module compilation state of a C++ rule.
///
/// @param n Rule node
/// @param compiler C++ compiler command
/// @param tag Toolchain object tag of the compilation scope. Zero for the native target
struct cxx_modules* cxx_modules_create(struct node *n, const char *compiler, const char *tag);

/// Destroys module compilation state. Registered modules remain in the project
/// wide module registry, which is released by `cxx_modules_dispose()`.
void cxx_modules_destroy(struct cxx_modules*);

/// Releases the project wide module registry.
///
/// The registry refers to rule nodes of the current build environment, so it is
/// released together with that environment (`autark_dispose()`) and is recreated
/// on demand. Safe to call multiple times.
void cxx_modules_dispose(void);

/// Scans the source and registers it as a module unit if it declares a module.
///
/// @param required Source is explicitly declared as module unit, so the
///                 declaration is mandatory (regardless of the extension)
///
/// @return true if the source is a module interface unit (produces a BMI)
bool cxx_modules_add_source(struct cxx_modules*, const char *src, bool required);

/// @return true if the absolute source path is a module interface unit
bool cxx_modules_is_interface(struct cxx_modules*, const char *src);

/// Writes the module map file (gcc only) before compilation.
void cxx_modules_prepare(struct cxx_modules*);

/// Appends module related compiler arguments. Must be called before the
/// `-c <source>` arguments are added to the spawn.
///
/// @param src Absolute source path
void cxx_modules_args_add(struct cxx_modules*, struct spawn*, const char *src);

/// Registers BMIs and the module map file as node dependencies.
void cxx_modules_deps_add(struct cxx_modules*, struct deps *deps);

#endif
