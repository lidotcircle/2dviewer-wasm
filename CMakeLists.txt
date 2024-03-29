cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(Magic2DViewer VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR CMAKE_BUILD_TYPE STREQUAL "Debug") 
    get_filename_component(COMPILER_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    set(EM_SYS_HEADER ${COMPILER_DIR}/cache/sysroot/include)
    set(CMAKE_CXX_FLAGS "-I${EM_SYS_HEADER}")
    set(CMAKE_C_FLAGS "-I${EM_SYS_HEADER}")
endif()


add_subdirectory(wasm-canvas)

add_library(MainEntry src/main.c)
target_link_libraries(MainEntry PRIVATE HTMLCanvas)


set(HTMLTemplate ${CMAKE_CURRENT_LIST_DIR}/src/index.html)

add_custom_target(indexhtml ALL)
add_custom_command(
    TARGET indexhtml
    DEPENDS HTMLCanvas MainEntry ${HTMLTemplate}
    POST_BUILD
    COMMAND ${CMAKE_CXX_COMPILER}
    -Oz
	--closure 1
    --shell-file ${HTMLTemplate}
	-s ENVIRONMENT=web
	-s ALLOW_MEMORY_GROWTH=1
	-s AGGRESSIVE_VARIABLE_ELIMINATION=1
	-s ABORTING_MALLOC=1
	-s "EXPORTED_FUNCTIONS=['_main', '_malloc']"
	-s EXIT_RUNTIME=0
	-s NO_FILESYSTEM=1
	-s DISABLE_EXCEPTION_CATCHING=2
	--bind
	-s 'EXPORTED_RUNTIME_METHODS=["UTF8ToString"]'
    $<TARGET_FILE:HTMLCanvas>
    $<TARGET_FILE:MainEntry>
    -o ${CMAKE_CURRENT_BINARY_DIR}/index.html)

add_custom_target(CopyResource ALL)
add_custom_command(
    TARGET CopyResource
    DEPENDS indexhtml
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/css ${CMAKE_CURRENT_BINARY_DIR}/css
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/fonts ${CMAKE_CURRENT_BINARY_DIR}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/scripts ${CMAKE_CURRENT_BINARY_DIR}/scripts
)
add_custom_command(
    TARGET CopyResource
    DEPENDS indexhtml
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/serve.py ${CMAKE_CURRENT_BINARY_DIR}/serve.py
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/testdata.txt ${CMAKE_CURRENT_BINARY_DIR}/testdata.txt
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/favicon.ico ${CMAKE_CURRENT_BINARY_DIR}/favicon.ico
)

add_custom_target(distArchive ALL)
add_custom_command(
    TARGET distArchive
    DEPENDS CopyResource
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cfzv" "dist.tar.gz"
    ${CMAKE_CURRENT_BINARY_DIR}/fonts
    ${CMAKE_CURRENT_BINARY_DIR}/css
    ${CMAKE_CURRENT_BINARY_DIR}/scripts
    ${CMAKE_CURRENT_BINARY_DIR}/index.html
    ${CMAKE_CURRENT_BINARY_DIR}/index.js
    ${CMAKE_CURRENT_BINARY_DIR}/index.wasm
    ${CMAKE_CURRENT_BINARY_DIR}/serve.py
    ${CMAKE_CURRENT_BINARY_DIR}/testdata.txt
    ${CMAKE_CURRENT_BINARY_DIR}/favicon.ico
)
