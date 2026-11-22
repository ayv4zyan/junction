function(junction_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE junction_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

junction_test(tst_document)
junction_test(tst_geometry)
junction_test(tst_library)

# The Editor lives in the app, so its test builds the Editor's sources too.
qt_add_executable(tst_editor tst_editor.cpp
    ../src/app/Editor.cpp ../src/app/LibraryModel.cpp ../src/app/NodeModel.cpp)
target_include_directories(tst_editor PRIVATE ../src/app)
target_link_libraries(tst_editor PRIVATE junction_core Qt6::Test Qt6::Gui Qt6::Qml)
add_test(NAME tst_editor COMMAND tst_editor)
set_tests_properties(tst_editor PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
