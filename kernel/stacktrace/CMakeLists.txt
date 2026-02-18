# ============================================================================
# Stacktrace 模块 - 栈回溯等调试功能
# ============================================================================

# Symbol table generation paths (exported to parent scope)
# 将生成的符号文件放在构建目录中，避免污染源代码
set(SYMBOLS_PY ${CMAKE_SOURCE_DIR}/scripts/extract_symbols.py PARENT_SCOPE)
set(SYMBOLS_C ${CMAKE_CURRENT_BINARY_DIR}/symbols.c PARENT_SCOPE)

# 符号文件的时间戳，用于触发重新编译
set(SYMBOLS_TIMESTAMP ${CMAKE_CURRENT_BINARY_DIR}/symbols.c.timestamp PARENT_SCOPE)

# 首次构建时，如果符号文件不存在，创建一个空占位符
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/symbols.c)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/symbols.c
"/* Auto-generated placeholder - will be overwritten by extract_symbols.py */\n"
"#include \"stacktrace/symbol.h\"\n"
"\n"
"/* Empty symbol table */\n"
"const char _symbol_string_table[] = \"\";\n"
"const symbol_entry_t _symbol_table_start[] = { { 0, NULL } };\n"
"const symbol_entry_t _symbol_table_end[] = { { 0, NULL } };\n"
"const uintptr_t _symbol_text_start = 0x10000;\n"
"const uintptr_t _symbol_text_end = 0x20000;\n"
)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/symbols.c.timestamp "")
endif()

add_library(stacktrace STATIC
    stacktrace.c
    symbol.c
    symbols.c
)

target_include_directories(stacktrace PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(stacktrace PUBLIC ${CCOS_FREESTANDING_FLAGS})

set_target_properties(stacktrace PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
