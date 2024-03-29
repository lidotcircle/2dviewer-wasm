add_library(HTMLCanvas canvas.c window.c)
target_include_directories(HTMLCanvas PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
