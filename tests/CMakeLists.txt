function(junction_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE junction_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

junction_test(tst_document)
junction_test(tst_geometry)
junction_test(tst_library)
junction_test(tst_open_canvas)
junction_test(tst_fragment)

# The Editor lives in the app, so its test builds the Editor's sources too.
qt_add_executable(tst_editor tst_editor.cpp
    ../src/app/Editor.cpp ../src/app/LibraryRows.cpp ../src/app/NodeModel.cpp)
target_include_directories(tst_editor PRIVATE ../src/app)
target_link_libraries(tst_editor PRIVATE junction_core junction_render Qt6::Test Qt6::Gui Qt6::Qml)
add_test(NAME tst_editor COMMAND tst_editor)
set_tests_properties(tst_editor PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The design check's rules (src/design/check): what goes around the design system is rejected.
qt_add_executable(tst_design_check tst_design_check.cpp)
target_link_libraries(tst_design_check PRIVATE junction_design_check Qt6::Test)
add_test(NAME tst_design_check COMMAND tst_design_check)

# The design values the C++ side copies (src/render/DesignValues.h) match the tokens.
qt_add_executable(tst_design_values tst_design_values.cpp)
target_link_libraries(tst_design_values PRIVATE junction_render junction_design junction_designplugin Qt6::Test Qt6::Qml Qt6::Gui)
add_test(NAME tst_design_values COMMAND tst_design_values)
set_tests_properties(tst_design_values PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The real app starts on a small Library without any QML error (see app_starts_cleanly.cmake).
add_test(NAME app_starts_cleanly
    COMMAND ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:junction> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/app_starts_cleanly
            -P ${CMAKE_CURRENT_SOURCE_DIR}/app_starts_cleanly.cmake)
