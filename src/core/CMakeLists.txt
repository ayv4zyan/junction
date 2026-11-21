# The core knows nothing about QML: model, changes/undo, file format, geometry, painting.
add_library(junction_core STATIC
    CanvasItems.h CanvasItems.cpp
    Document.h Document.cpp
    TransactionCommand.h
    Geometry.h Geometry.cpp
    CanvasPainter.h CanvasPainter.cpp
)

target_include_directories(junction_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(junction_core PUBLIC Qt6::Core Qt6::Gui Qt6::Svg)
