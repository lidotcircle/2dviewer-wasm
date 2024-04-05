project(dcparse VERSION 0.1.0)

file(GLOB_RECURSE dcparse_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/lib/**.cpp)
add_library(dcparse STATIC ${dcparse_SOURCES})
set_property(TARGET dcparse PROPERTY CXX_STANDARD 17)
target_include_directories(dcparse PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
