add_executable(test_headers unit.c test_headers.c)

target_link_libraries(test_headers PRIVATE ${PROJECT_NAME})
target_include_directories(test_headers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(test_headers PRIVATE -Wall -Wextra)

add_test(NAME headers COMMAND test_headers)
