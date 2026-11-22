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

# The app's QML uses the design tokens, never raw colors, font sizes or durations.
qt_add_executable(tst_design_tokens tst_design_tokens.cpp)
target_compile_definitions(tst_design_tokens PRIVATE JUNCTION_APP_QML_DIR="${CMAKE_SOURCE_DIR}/src/app/qml")
target_link_libraries(tst_design_tokens PRIVATE Qt6::Test)
add_test(NAME tst_design_tokens COMMAND tst_design_tokens)
