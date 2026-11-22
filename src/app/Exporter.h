#pragma once

// Writes the open Canvas somewhere else: a copy of its .canvas file, or a PNG or SVG image.
// Both return the file written, or an empty URL when that failed (errorOccurred says why).

#include "Document.h"

#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantMap>

class Exporter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main.cpp")

public:
    explicit Exporter(const Document *document, QObject *parent = nullptr);

    Q_INVOKABLE QUrl exportCanvasFile(const QUrl &url);
    // Of the nodes among `ids` (with what is inside their Groups), or the whole Canvas when
    // there are none; `colors` is Colors.forPainter.
    Q_INVOKABLE QUrl exportImage(const QUrl &url, const QVariantMap &colors, const QStringList &ids);

signals:
    void errorOccurred(const QString &message);

private:
    const Document *m_document;
};
