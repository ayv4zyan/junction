#pragma once

// The core's types QML uses, registered here: the core itself knows nothing about QML.

#include "CanvasItems.h"
#include "Library.h"
#include "CanvasSession.h"

#include <QQmlEngine>

struct LibraryForeign
{
    Q_GADGET
    QML_FOREIGN(Library)
    QML_NAMED_ELEMENT(Library)
    QML_UNCREATABLE("Created by main.cpp")
};

struct CanvasSessionForeign
{
    Q_GADGET
    QML_FOREIGN(CanvasSession)
    QML_NAMED_ELEMENT(CanvasSession)
    QML_UNCREATABLE("Created by the Editor")
};

// The view Library::currentView() returns.
struct ViewportForeign
{
    Q_GADGET
    QML_FOREIGN(Viewport)
    QML_VALUE_TYPE(viewport)
};

// Side, PathStyle and NodeKind: CanvasEnums.Side.Top, CanvasEnums.PathStyle.Curved, ...
namespace CanvasEnumsForeign {
Q_NAMESPACE
QML_FOREIGN_NAMESPACE(CanvasEnums)
QML_NAMED_ELEMENT(CanvasEnums)
} // namespace CanvasEnumsForeign
