function(junction_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE junction_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

junction_test(tst_document)
junction_test(tst_geometry)
junction_test(tst_library)
