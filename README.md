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


Documentation will be available in the very near future.
