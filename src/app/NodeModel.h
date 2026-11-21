#pragma once

#include "Document.h"

#include <QAbstractListModel>
#include <QQmlEngine>

// Exposes the Document's Cards or its Groups to a QML Repeater.
class NodeModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by Editor")

public:
    enum Filter { Cards, Groups };
    enum Role { IdRole = Qt::UserRole + 1, KindRole, XRole, YRole, WidthRole, HeightRole, TextRole, ColorRole };

    NodeModel(Document *document, Filter filter, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    bool accepts(const QString &id) const;
    void rebuild();

    Document *m_document;
    Filter m_filter;
    QStringList m_ids;   // the ids shown, in drawing order
};
