#pragma once

// The few design values the C++ side needs without QML: for painting and export, Label hit
// boxes and where the selection toolbar goes. They copy tokens from the design system
// (src/design); tst_design_values fails if they drift apart, so change both together.

#include <QtGlobal>

namespace DesignValues {

constexpr int LabelFontSize = 13;      // Fonts.body: Connection Labels (LabelEditor.qml too)
constexpr int CardFontSize = 15;       // Fonts.canvas: Card text and Group labels
constexpr qreal CardPadding = 12;      // Space.md: around a Card's text
constexpr qreal LabelPaddingX = 8;     // Space.sm: a Connection Label's box, sideways
constexpr qreal LabelPaddingY = 4;     // Space.xs: ... and above and below
constexpr qreal GroupLabelHeight = 28; // Sizes.control: GroupItem's label bar...
constexpr qreal GroupLabelSpace = 32;  // Sizes.control + Space.xs: ...and how far above the Group it starts
constexpr qreal GroupTint = 0.08;      // Colors.groupTint: how strongly a Group tints the Canvas
constexpr qreal CardTint = 0.12;       // Colors.cardTint: how strongly a color tints a Card

} // namespace DesignValues
