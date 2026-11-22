qt_add_executable(junction main.cpp)

qt_add_qml_module(junction
    URI Junction
    QML_FILES
        qml/Main.qml
        qml/CanvasToolbar.qml
        qml/LoadErrorPanel.qml
        qml/ExportDialog.qml
        qml/CanvasView.qml
        qml/CanvasMenus.qml
        qml/BackgroundInput.qml
        qml/NodeItem.qml
        qml/CardItem.qml
        qml/GroupItem.qml
        qml/ConnectionOverlay.qml
        qml/LabelEditor.qml
        qml/SelectionBar.qml
        qml/PropertyPicker.qml
        qml/DragArea.qml
        qml/SelectMoveArea.qml
        qml/PointMarker.qml
        qml/ResizeEdges.qml
        qml/IconButton.qml
        qml/InlineName.qml
        qml/TextEditSession.qml
        qml/Sidebar.qml
        qml/SidebarRow.qml
        qml/SidebarDrag.qml
        qml/CanvasColors.js
        qml/Placement.js
    RESOURCES
        icons/style-straight.svg icons/style-curved.svg icons/style-elbow.svg
        icons/arrows-none.svg icons/arrows-end.svg icons/arrows-start.svg icons/arrows-both.svg
        icons/label.svg
        icons/menu.svg icons/undo.svg icons/redo.svg
        icons/sidebar.svg icons/plus.svg icons/chevron.svg icons/more.svg icons/search.svg
        icons/close.svg icons/canvas.svg icons/project.svg
    SOURCES
        Editor.h Editor.cpp
        Selection.h Selection.cpp
        HitTest.h HitTest.cpp
        Exporter.h Exporter.cpp
        LibraryRows.h LibraryRows.cpp
        QmlForeignTypes.h
        SingleInstance.h SingleInstance.cpp
        NodeModel.h NodeModel.cpp
        CanvasLayers.h CanvasLayers.cpp
)

# Every build first checks that the app's QML uses the design system (src/design/check);
# code that goes around it fails the build.
add_custom_target(junction_design_check_app ALL
    COMMAND design-check ${CMAKE_CURRENT_SOURCE_DIR}/qml
    COMMENT "Checking that the app's QML uses the design system"
    VERBATIM)
add_dependencies(junction junction_design_check_app)

# The look comes from JunctionDesign (src/design): its tokens, controls style and components.
target_link_libraries(junction PRIVATE junction_core junction_render junction_design junction_designplugin
    Qt6::Network Qt6::Quick Qt6::QuickControls2)
