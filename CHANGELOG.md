# [v0.9.13]
- Added autark script `prepare` keyword as alias of `check`.
- Fixed incorrect behaviour of `path_relativize_cwd()` for some edge cases.
- Fixes ./check_scripts/changelog_md.sh changelog parser allowing optional timestamps.
- Maked changelog parsers awk rules to be more portable.

# [v0.9.12]
- Added changelog processing check scripts.
  GNU and Markdown styles are supported.

# [v0.9.11]
- Fixed several UB and memory corruption cases.
- Fixed incorrect behavior of tests in `if` condition.
- Process spawn fixes and optimisation.
- Fixed incorrect DEPS_TYPE_FILE_NOT_EXISTS condition.
- Fixed incorrect fseek() in fetchreg_register()
- Fixed incorrect JSON escaping for compile_commands.json
- Fixed wrong compiler headers dependency parsing.

# [v0.9.10]
- Better handling of spread (..${}) values

# [v0.9.9]
- Fix. Not all program output is handled by spawn. (spawn.c)
- Added `in` test condition for `if` clause.

# [v0.9.8]
- Parallel compilation of C/C++ source files.
- Support of an external project dependencies.
- Automatic generation of `compile_commands.json` compilation database.
- Generation of project source distributions with all required source dependencies included, allowing builds in isolated environments.
- Script syntax highlighting support for Vim8+, Neovim, VSCode, TextMate with [Softmotions/tree-sitter-autark](https://github.com/Softmotions/tree-sitter-autark)