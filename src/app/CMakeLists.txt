qt_add_executable(junction main.cpp)

qt_add_qml_module(junction
    URI Junction
    QML_FILES
        qml/Main.qml
        qml/CanvasView.qml
        qml/CardItem.qml
        qml/GroupItem.qml
        qml/ConnectionOverlay.qml
        qml/LabelEditor.qml
        qml/SelectionBar.qml
        qml/DragArea.qml
        qml/Dot.qml
        qml/ResizeEdges.qml
        qml/IconButton.qml
        qml/InlineName.qml
        qml/Sidebar.qml
        qml/SidebarRow.qml
        qml/CanvasColors.js
    RESOURCES
        icons/style-straight.svg icons/style-curved.svg icons/style-elbow.svg
        icons/arrows-none.svg icons/arrows-end.svg icons/arrows-start.svg icons/arrows-both.svg
        icons/label.svg
        icons/menu.svg icons/undo.svg icons/redo.svg
        icons/sidebar.svg icons/plus.svg icons/chevron.svg icons/more.svg icons/search.svg
        icons/close.svg icons/canvas.svg icons/project.svg
    SOURCES
        Editor.h Editor.cpp
        LibraryModel.h LibraryModel.cpp
        NodeModel.h NodeModel.cpp
        CanvasLayers.h CanvasLayers.cpp
)

# The look comes from JunctionDesign (src/design): its tokens, controls style and components.
target_link_libraries(junction PRIVATE junction_core junction_design junction_designplugin
    Qt6::Network Qt6::Quick Qt6::QuickControls2)
