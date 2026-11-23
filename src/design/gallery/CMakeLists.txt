# junction-gallery: every token and component of JunctionDesign, in every state, in both
# themes. It links JunctionDesign and nothing from the app, so if the design system ever
# starts depending on app code, this stops building.
qt_add_executable(junction_gallery main.cpp)
set_target_properties(junction_gallery PROPERTIES OUTPUT_NAME junction-gallery)

qt_add_qml_module(junction_gallery
	URI JunctionGallery
	QML_FILES Gallery.qml
)

target_link_libraries(junction_gallery PRIVATE junction_design junction_designplugin Qt6::Quick Qt6::QuickControls2)
