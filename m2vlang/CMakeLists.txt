add_library(M2VLang STATIC
    parser.cpp
    vm.cpp
    vm_object.cpp
)
target_compile_features(M2VLang PRIVATE cxx_std_17)
target_link_libraries(M2VLang PRIVATE dcparse)
target_include_directories(M2VLang PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(M2VLang PRIVATE $<$<CONFIG:Debug>:DEBUG>)

add_subdirectory(test)
