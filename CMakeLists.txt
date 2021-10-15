
cmake_minimum_required(VERSION 3.12)
project(Apollo VERSION 1.0.0 DESCRIPTION "Lua 5.3 fork with support for multiple environments" LANGUAGES C)

option(BUILD_SHARED_LIBS "Build as shared library" ON)

set(srcDir src)

set(srcFiles lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c ldebug.c ldo.c
    ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c
    loslib.c lparser.c lstate.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c
    lutf8lib.c lvm.c lzio.c)

set(publicHeaderFiles lauxlib.h lua.h luaconf.h lualib.h)

list(TRANSFORM srcFiles PREPEND ${srcDir}/)
list(TRANSFORM publicHeaderFiles PREPEND ${srcDir}/)

add_library(luadll
    ${srcFiles})

set_target_properties(luadll PROPERTIES
    OUTPUT_NAME "lua")

target_compile_definitions(luadll
    PRIVATE
    $<$<PLATFORM_ID:Linux>:LUA_USE_LINUX LUA_COMPAT_5_2>)

target_compile_options(luadll
    PRIVATE
    $<$<OR:$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:GNU>>:
    -Wextra -Wshadow -Wsign-compare -Wundef -Wwrite-strings -Wredundant-decls
    -Wdisabled-optimization -Waggregate-return -Wdouble-promotion -Wdeclaration-after-statement
    -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes -Wc++-compat -Wold-style-definition>)

add_library(lualib
        STATIC
        ${srcFiles})

set_target_properties(lualib PROPERTIES
        OUTPUT_NAME "lua")

target_compile_definitions(lualib
        PRIVATE
        $<$<PLATFORM_ID:Linux>:LUA_USE_LINUX LUA_COMPAT_5_2>)

target_compile_options(lualib
        PRIVATE
        $<$<OR:$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:GNU>>:
        -Wextra -Wshadow -Wsign-compare -Wundef -Wwrite-strings -Wredundant-decls
        -Wdisabled-optimization -Waggregate-return -Wdouble-promotion -Wdeclaration-after-statement
        -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes -Wc++-compat -Wold-style-definition>)

add_executable(lua src/lua.c)

target_link_libraries(lua PUBLIC
    lualib)

set_target_properties(lua PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

add_executable(luac src/luac.c)

target_link_libraries(luac PUBLIC
    lualib)

set_target_properties(luac PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

set(includeDir ${CMAKE_CURRENT_BINARY_DIR}/include)
file(COPY ${publicHeaderFiles} DESTINATION ${includeDir})

add_library(lua-header
    INTERFACE)

target_include_directories(lua-header
    INTERFACE
    ${includeDir})

target_link_libraries(luadll
    INTERFACE
    lua-header)

target_link_libraries(lualib
    INTERFACE
    lua-header)


add_library(lua::lib ALIAS luadll)
add_library(lua::header ALIAS lua-header)

