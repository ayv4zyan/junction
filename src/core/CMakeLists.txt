# The core knows nothing about QML or how things look: the model, changes and undo, the file
# format, the Library and geometry.
add_library(junction_core STATIC
    CanvasItems.h CanvasItems.cpp
    Document.h Document.cpp
    OpenCanvas.h OpenCanvas.cpp
    Fragment.h Fragment.cpp
    Library.h Library.cpp
    TransactionCommand.h
    Geometry.h Geometry.cpp
)

target_include_directories(junction_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(junction_core PUBLIC Qt6::Core Qt6::Gui)
