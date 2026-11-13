#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class PropertiesLayout : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;
    Q_INVOKABLE QVariantList generalSections(const QVariantList &rows) const;
    Q_INVOKABLE QVariantList detailsSections(const QVariantList &rows) const;
};
