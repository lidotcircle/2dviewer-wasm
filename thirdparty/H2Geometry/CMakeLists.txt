add_library(H2Geometry INTERFACE ${sffs_SOURCES})
target_include_directories(H2Geometry INTERFACE "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(H2Geometry INTERFACE cxx_std_17)
