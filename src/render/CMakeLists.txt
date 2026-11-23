# Drawing a Canvas with QPainter: the live view's grid and Connections, and PNG/SVG export.
# Uses the core's model and geometry, and copies of a few design tokens (DesignValues.h).
add_library(junction_render STATIC
	CanvasPainter.h CanvasPainter.cpp
	DesignValues.h
)

target_include_directories(junction_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(junction_render PUBLIC junction_core Qt6::Gui Qt6::Svg)
