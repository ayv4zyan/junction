qt_add_executable(junction main.cpp)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(junction
    URI Junction
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/CanvasView.qml
        qml/CardItem.qml
        qml/GroupItem.qml
        qml/ConnectionOverlay.qml
        qml/SelectionBar.qml
        qml/DragArea.qml
        qml/Dot.qml
        qml/ResizeEdges.qml
        qml/IconButton.qml
    RESOURCES
        icons/style-straight.svg icons/style-curved.svg icons/style-elbow.svg
        icons/arrows-none.svg icons/arrows-end.svg icons/arrows-start.svg icons/arrows-both.svg
        icons/label.svg
        icons/menu.svg icons/undo.svg icons/redo.svg
    SOURCES
        Editor.h Editor.cpp
        NodeModel.h NodeModel.cpp
        CanvasLayers.h CanvasLayers.cpp
)

target_link_libraries(junction PRIVATE junction_core Qt6::Quick Qt6::QuickControls2)
