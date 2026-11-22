# The design check (see DesignCheck.h). Plain QtCore, so it can run during the build.
qt_add_library(junction_design_check STATIC DesignCheck.h DesignCheck.cpp)
target_include_directories(junction_design_check PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(junction_design_check PUBLIC Qt6::Core)

qt_add_executable(design-check main.cpp)
target_link_libraries(design-check PRIVATE junction_design_check)
