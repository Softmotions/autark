#ifndef TOOLCHAIN_H
#define TOOLCHAIN_H

#ifndef _AMALGAMATE_
#include "pool.h"
#include "ulist.h"

#include <stdbool.h>
#endif

/// A single key/value pair declared by a toolchain descriptor.
struct toolchain_var {
  const char *key; // Normalized key name (lowercase, `-` and `.` replaced by `_`)
  const char *var; // Published build script variable name (e.g. TOOLCHAIN_SYSROOT)
  const char *val; // Resolved value
};

/// Cross compilation toolchain descriptor.
///
/// Descriptors are ordinary Autark DSL fragments declared as a single
/// `toolchain { }` rule. Descriptor keys are not limited to a fixed set:
/// every declared key is published as `TOOLCHAIN_<KEY>` (and as a process
/// environment variable of the same name), while well known keys are also
/// exposed under the canonical Autark names (`SYSTEM_*`, `CC`, `AR`, ...).
struct toolchain {
  const char *path;        // Absolute path to the descriptor file
  const char *id;          // Target id (SYSTEM_TARGET). Defaults to the descriptor basename
  const char *system;      // Target OS name, lowercase (SYSTEM_NAME). e.g. `linux`, `android`
  const char *arch;        // Target CPU architecture (SYSTEM_ARCH)
  const char *arch_alt;    // Alternative architecture name (SYSTEM_ARCH_ALT). Defaults to `arch`
  const char *bits;        // Target bitness (SYSTEM_BITNESS_<bits>). e.g. `32`, `64`
  const char *endian;      // Target endianness: `little` or `big`
  const char *cc;          // Target C compiler (CC)
  const char *cxx;         // Target C++ compiler (CXX)
  const char *ar;          // Target archiver (AR)
  const char *ranlib;      // Target ranlib (RANLIB)
  const char *strip;       // Target strip (STRIP)
  const char *pkgconf;     // Target pkg-config tool (PKGCONF)
  const char *cflags;      // Target compiler flags (CFLAGS_TARGET)
  const char *ldflags;     // Target linker flags (LDFLAGS_TARGET)
  const char *host_cc;     // Host C compiler used to build build tools (HOST_CC)
  const char *host_cxx;    // Host C++ compiler used to build build tools (HOST_CXX)
  const char *host_ar;     // Host archiver used to build build tools (HOST_AR)
  const char *host_cflags; // Host C compiler flags (HOST_CFLAGS)
  const char *host_cxxflags; // Host C++ compiler flags (HOST_CXXFLAGS)
  const char *host_ldflags; // Host linker flags (HOST_LDFLAGS)
  const char *runner;      // Command prefix used to run target executables (emulator/adb)
  struct ulist vars;       // All declared key/value pairs (struct toolchain_var*)
};

/// Object tag of build tools compiled by host-cc/host-cxx
#define TOOLCHAIN_TAG_HOST "host"

/// Loads and resolves the given toolchain descriptor.
///
/// @param pool Pool used to allocate the toolchain and its values
/// @param path Path to the descriptor file
///
/// @return Resolved toolchain
struct toolchain* toolchain_load(struct pool *pool, const char *path);

/// Loads the toolchain descriptor selected by `--toolchain`/`AUTARK_TOOLCHAIN` (if any)
/// and publishes its facts into the root build unit.
void toolchain_activate(void);

/// Publishes resolved toolchain facts into the root build unit environment
/// and into the process environment, so check scripts and nested builds see them.
void toolchain_apply(struct toolchain *tc);

/// Looks up a raw descriptor value by key.
///
/// Key lookup is case insensitive and treats `-`, `.` and `_` as the same
/// separator, so `host-cc`, `host_cc` and `HOST.CC` are equivalent.
///
/// @return Declared value or zero if the key is not declared
const char* toolchain_var_get(struct toolchain *tc, const char *key);

/// @return Id of the active target toolchain or zero for a native build.
const char* toolchain_id(void);

/// @return Tag of objects compiled in the given scope.
///         0 for native target scope (no tagging is performed).
const char* toolchain_object_tag(bool host_scope);

/// Writes (or removes when `tag` is zero) the toolchain tag sidecar of an object file.
int toolchain_object_tag_write(const char *obj_path, const char *tag);

/// Checks that the object file belongs to the given compilation scope.
///
/// @return Pointer to `buf` holding the object tag on mismatch, zero if the
///         object is compatible with the scope or carries no tag.
const char* toolchain_object_check(const char *obj_path, bool host_scope, char *buf, size_t buflen);

#endif
