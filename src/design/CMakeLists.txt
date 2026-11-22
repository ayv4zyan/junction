# JunctionDesign: the look of the app. Tokens (Colors, Space, Sizes, Fonts, Motion), a Qt Quick
# Controls style (Button.qml, Menu.qml, ...) and a few shared components. It knows nothing
# about Canvases or the Library: the app depends on it, never the other way round.
# See README.md for the rules.
qt_add_library(junction_design STATIC)

set_source_files_properties(Colors.qml Space.qml Sizes.qml Fonts.qml Motion.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(junction_design
    URI JunctionDesign
    # Controls this style doesn't define come from Fusion.
    IMPORTS QtQuick.Controls.Fusion
    QML_FILES
        # Tokens
        Colors.qml Space.qml Sizes.qml Fonts.qml Motion.qml
        # The controls style: what plain `Button {}`, `Menu {}` ... look like
        ApplicationWindow.qml Label.qml Button.qml ToolButton.qml TextField.qml CheckBox.qml
        Menu.qml MenuItem.qml MenuSeparator.qml ToolTip.qml Dialog.qml ScrollBar.qml ToolBar.qml
        # Components
        Floating.qml RisingFloating.qml FocusRing.qml Toast.qml Swatch.qml Separator.qml AskDialog.qml
        MenuEntry.qml TextMenu.qml
    RESOURCES
        fonts/Geist-Regular.ttf fonts/Geist-SemiBold.ttf fonts/Geist-Bold.ttf
        icons/check.svg icons/chevron.svg
    SOURCES
        JunctionDesign.h JunctionDesign.cpp
)

target_include_directories(junction_design PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(junction_design PRIVATE Qt6::Gui Qt6::Quick Qt6::QuickControls2)

add_subdirectory(check)
add_subdirectory(gallery)
