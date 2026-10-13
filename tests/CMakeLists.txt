# Build one unit-test executable per module: <name>.c -> test_<name>.
# Every executable is linked against the library and registered with CTest.
function(http_add_unit_test name)
    add_executable(test_${name} unit.c test_${name}.c)
    target_link_libraries(test_${name} PRIVATE ${PROJECT_NAME})
    target_include_directories(test_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(test_${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

http_add_unit_test(headers)
http_add_unit_test(url)
http_add_unit_test(request)
http_add_unit_test(response)
