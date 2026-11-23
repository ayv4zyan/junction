pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import JunctionDesign

// Every token and component of the design system, in its states. Build new components here
// first, and judge them here in both themes. Command-line arguments: "light", "dark", "menu", "toast", "dialog".
ApplicationWindow {
	id: window
	width: 1100
	height: 900
	visible: true
	title: "Junction design system"

	readonly property var args: Qt.application.arguments
	Component.onCompleted: {
		if (args.indexOf("light") >= 0)
			Colors.dark = false
		else if (args.indexOf("dark") >= 0)
			Colors.dark = true
		if (args.indexOf("menu") >= 0)
			Qt.callLater(() => sampleMenu.popup(menuButton, 0, menuButton.height))
		if (args.indexOf("toast") >= 0)
			Qt.callLater(() => toast.show("Exported “Auth flow.png”", { action: "Show in folder", onAction: () => {} }))
		if (args.indexOf("dialog") >= 0)
			Qt.callLater(() => ask.confirm({ title: "Delete “Auth flow”?", message: "It will be moved to the trash.",
											 action: "Delete", destructive: true }))
	}

	header: ToolBar {
		RowLayout {
			anchors.fill: parent
			spacing: Space.sm
			Label { text: "JunctionDesign"; font.weight: Font.DemiBold }
			Label { text: "tokens, controls style and components"; color: Colors.mutedText }
			Item { Layout.fillWidth: true }
			CheckBox { text: "Dark"; checked: Colors.dark; onToggled: Colors.dark = checked }
		}
	}

	ScrollView {
		anchors.fill: parent
		contentWidth: availableWidth

		ColumnLayout {
			width: parent.width
			spacing: Space.xxl

			// ---------------------------------------------------------------- tokens
			Section {
				title: "Colors"
				Flow {
					Layout.fillWidth: true
					spacing: Space.md
					Repeater {
						model: ["background", "sidebar", "panel", "cardFill", "grid", "divider", "cardBorder", "line",
								"text", "mutedText", "selection", "accentFill", "accentText", "danger", "dangerFill",
								"hover", "pressed", "current"]
						delegate: ColumnLayout {
							required property string modelData
							spacing: Space.xs
							Rectangle {
								implicitWidth: 96
								implicitHeight: 40
								color: Colors[parent.modelData]
								border.color: Colors.cardBorder
							}
							Label { text: parent.modelData; font.pixelSize: Fonts.small; color: Colors.mutedText }
						}
					}
				}
				Row {
					spacing: Space.xs
					Repeater {
						model: Colors.presets
						delegate: Rectangle { required property color modelData; width: 40; height: 24; color: modelData }
					}
				}
			}

			Section {
				title: "Type (Geist)"
				Label { text: "Title 15 — Delete “Auth flow”?"; font.pixelSize: Fonts.title; font.weight: Font.DemiBold }
				Label { text: "Body 13 — every label, menu item and button in the app" }
				Label { text: "Muted 13 — secondary text, shortcut hints"; color: Colors.mutedText }
				Label { text: "Small 11 — counts: 3 · 12 · 128"; font.pixelSize: Fonts.small; font.features: { "tnum": 1 } }
				Text { text: "Canvas 15 — the text on a Card"; font.pixelSize: Fonts.canvas; color: Colors.text }
				Label { text: "Кириллица тоже: «Аутентификация»"; color: Colors.mutedText }
			}

			Section {
				title: "Space (4px grid) and sizes"
				Repeater {
					model: [["xs", Space.xs], ["sm", Space.sm], ["md", Space.md], ["lg", Space.lg], ["xl", Space.xl], ["xxl", Space.xxl],
							["control", Sizes.control], ["toolbar", Sizes.toolbar]]
					delegate: RowLayout {
						required property var modelData
						spacing: Space.md
						Label { text: parent.modelData[0] + " " + parent.modelData[1]; Layout.preferredWidth: 96; color: Colors.mutedText }
						Rectangle { implicitWidth: parent.modelData[1]; implicitHeight: Space.sm; color: Colors.selection }
					}
				}
			}

			Section {
				title: "Motion — click the box (fast 100 · normal 150 · slow 220, crisp)"
				Rectangle {
					id: mover
					property bool moved: false
					Layout.preferredWidth: 400
					Layout.preferredHeight: Sizes.control
					color: "transparent"
					border.color: Colors.cardBorder
					Repeater {
						model: [Motion.fast, Motion.normal, Motion.slow]
						delegate: Rectangle {
							id: runner
							required property int modelData
							required property int index
							y: index * (Sizes.control / 3)
							width: Sizes.control
							height: Sizes.control / 3
							x: mover.moved ? mover.width - width : 0
							color: Colors.accentFill
							Behavior on x { NumberAnimation { duration: runner.modelData; easing.type: mover.moved ? Motion.enter : Motion.exit } }
						}
					}
					TapHandler { onTapped: mover.moved = !mover.moved }
				}
			}

			// ---------------------------------------------------------------- controls
			Section {
				title: "Buttons"
				RowLayout {
					spacing: Space.sm
					Button { text: "Cancel" }
					Button { text: "Save"; highlighted: true }
					Button { text: "Delete"; highlighted: true; palette.highlight: Colors.dangerFill }
					Button { text: "Flat"; flat: true }
					Button { text: "With icon"; icon.source: "qrc:/qt/qml/JunctionDesign/icons/check.svg" }
					Button { text: "Disabled"; enabled: false }
					Button { text: "Focused"; Component.onCompleted: forceActiveFocus(Qt.TabFocusReason) }
				}
				RowLayout {
					spacing: Space.xs
					ToolButton { icon.source: "qrc:/qt/qml/JunctionDesign/icons/check.svg"; ToolTip.visible: hovered; ToolTip.delay: Motion.tooltipDelay; ToolTip.text: "A tool  Ctrl+T" }
					ToolButton { icon.source: "qrc:/qt/qml/JunctionDesign/icons/chevron.svg"; checked: true }
					ToolButton { icon.source: "qrc:/qt/qml/JunctionDesign/icons/check.svg"; enabled: false }
					Separator {}
					ToolButton { text: "100%" }
					ToolButton { text: "Fit" }
				}
			}

			Section {
				title: "Fields and checks"
				RowLayout {
					spacing: Space.md
					TextField { placeholderText: "Search"; Layout.preferredWidth: 220 }
					TextField { text: "Auth flow"; Layout.preferredWidth: 220 }
					CheckBox { text: "Snap to grid"; checked: true }
					CheckBox { text: "Unchecked" }
					CheckBox { text: "Disabled"; enabled: false }
				}
			}

			Section {
				title: "Menus and tooltips"
				RowLayout {
					spacing: Space.sm
					Button {
						id: menuButton
						text: "Open menu"
						onClicked: sampleMenu.popup(menuButton, 0, menuButton.height)
					}
					Label { text: "Hover the tool buttons above for a tooltip"; color: Colors.mutedText }
				}
				Menu {
					id: sampleMenu
					MenuEntry { text: "Cut"; hint: "Ctrl+X" }
					MenuEntry { text: "Copy"; hint: "Ctrl+C" }
					MenuEntry { text: "Paste"; hint: "Ctrl+V"; enabled: false }
					MenuSeparator {}
					Menu {
						title: "Move to"
						MenuItem { text: "Web app" }
						MenuItem { text: "Mobile" }
					}
					MenuItem { text: "Snap to grid"; checkable: true; checked: true }
					MenuSeparator {}
					MenuEntry { text: "Delete"; hint: "Del"; destructive: true }
				}
			}

			// ---------------------------------------------------------------- components
			Section {
				title: "Swatches, separator, floating"
				Floating {
					implicitWidth: swatches.implicitWidth + 2 * Space.xs
					implicitHeight: Sizes.toolbar
					Row {
						id: swatches
						anchors.centerIn: parent
						Swatch { fill: Colors.cardFill; outlined: true; tip: "Default" }
						Repeater {
							model: Colors.presets
							delegate: Swatch {
								required property color modelData
								required property int index
								fill: modelData
								chosen: index === 4
							}
						}
						Separator { anchors.verticalCenter: parent.verticalCenter }
						ToolButton { icon.source: "qrc:/qt/qml/JunctionDesign/icons/check.svg" }
					}
				}
			}

			Section {
				title: "Toasts and dialogs"
				RowLayout {
					spacing: Space.sm
					Button { text: "Toast"; onClicked: toast.show("Saved “Auth flow”") }
					Button { text: "Error toast"; onClicked: toast.show("Could not save: the disk is full", { error: true }) }
					Button {
						text: "Toast with action"
						onClicked: toast.show("Exported “Auth flow.png”", { action: "Show in folder", onAction: () => {} })
					}
					Separator {}
					Button {
						text: "Confirm"
						onClicked: ask.confirm({ title: "Move 3 canvases?", message: "They will be moved to “Mobile”.", action: "Move" })
					}
					Button {
						text: "Destructive"
						onClicked: ask.confirm({ title: "Delete “Auth flow”?", message: "It will be moved to the trash.",
												 action: "Delete", destructive: true })
					}
					Button {
						text: "Ask for text"
						onClicked: ask.forText({ title: "Edit 3 labels", text: "calls", placeholder: "Label", action: "Save",
											  onAccepted: (text) => toast.show("Label: " + text) })
					}
				}
			}

			Item { implicitHeight: Space.xxl * 2 }
		}
	}

	Toast {
		id: toast
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottom: parent.bottom
		anchors.bottomMargin: Space.xl
	}

	AskDialog { id: ask }

	component Section: ColumnLayout {
		property string title
		Layout.leftMargin: Space.xl
		Layout.rightMargin: Space.xl
		Layout.topMargin: Space.lg
		Layout.fillWidth: true
		spacing: Space.md
		Label { text: parent.title; font.pixelSize: Fonts.title; font.weight: Font.DemiBold }
	}
}
