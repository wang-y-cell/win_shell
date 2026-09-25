cmake_minimum_required(VERSION 3.16)
project(win_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)

if(MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

add_library(utils STATIC
    src/utils/parser.cpp
    src/utils/color.cpp
    src/utils/width.cpp
    src/utils/output.cpp
    src/utils/sys.cpp
    src/utils/theme.cpp
    src/utils/fsutil.cpp
)
target_include_directories(utils PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(WIN32)
    target_link_libraries(utils PUBLIC shell32 advapi32)
endif()

function(add_cmd name)
    add_executable(${name} src/${name}/main.cpp)
    target_link_libraries(${name} PRIVATE utils)
endfunction()

set(WIN_SHELL_CMDS
    pwd ls cat head tail wc tee sort uniq
    basename dirname mkdir touch rm cp mv ln
    clear which tree du df diff find grep
)

foreach(cmd IN LISTS WIN_SHELL_CMDS)
    add_cmd(${cmd})
endforeach()

add_executable(ll src/ls/main.cpp)
target_link_libraries(ll PRIVATE utils)
target_compile_definitions(ll PRIVATE LS_AS_LL)

install(TARGETS ${WIN_SHELL_CMDS} ll RUNTIME DESTINATION bin)
