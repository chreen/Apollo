# Apollo
Lua 5.3 fork with added functionality to support other environments

## About
This was created for one of my old obfuscation projects, but since I have moved on from writing Lua obfuscators, I have decided to make this project public. It is mostly comprised of patches from the [lua-users wiki](http://lua-users.org/wiki/LuaPowerPatches) and some that I found on Google.

It adds features that aim to be compatible with scripts written for the following platforms
  * Luau (no typechecking or underscore number separators)
  * FiveM
  * Garry's Mod (no C style comments)
  
This project was created so the resulting bytecode could be embedded in a custom virtual machine written in Lua for the purpose of obfuscating scripts for multiple platforms with a single version of Lua.
  
## A Warning
This code has not been tested in production, and although the test suite passes, here (most likely) be dragons. This project will not be updated, and support will not be provided if you run into problems. Use at your own risk. The code is likely very messy and has a lot of room for optimizations, as this is my first C project (although I wouldn't call it that, as most of the work done here was applying patches written by others).
  
## Building
The build system has been modified to use CMake, based on [this project](https://github.com/walterschell/Lua). You can view the [`.github/workflows`](.github/workflows) directory to see how to build and test on your platform.

## Features

### Contextual Continue & Goto
For support with scripts written for Roblox, a `continue` keyword was added and both it and the `goto` keyword have been modified to be contextual. This means you can use them as variable names without any syntax errors.

[Relevant file: `continue` test](apollo-tests/continue.lua)

[Relevant file: `goto` test](apollo-tests/goto.lua)

### FiveM Hash Syntax
Apollo contains support for FiveM's backtick string literal for calculating Jenkins one-at-a-time hashes.

[Relevant file: hash test](apollo-tests/hash.lua)

### Garry's Mod Comparison Operators
Apollo allows the more traditional style comparison operators, `||` (or), `&&` (and), `!` (unary not), and `!=` (inequality)

[Relevant file: comparison test](apollo-tests/glua.lua)

### Compound Assignment Operators
Compound assignment operators have been added for addition, subtraction, multiplication, division, modulus, exponentiation, and concatenation. An increment operator (`++`) is also included.

[Relevant file: compound assignment test](apollo-tests/compound.lua)

### Generalized Iteration
Apollo allows for iteration of a table without calling a function (defaults to `next`). The `__iter` metamethod can be set to control which function is used.

[Relevant file: generalized iteration test](apollo-tests/gen_iter.lua)

### Binary Literals
Support for binary literals has been added, following the `0b` syntax. (example: `0b11001 == 25`)
