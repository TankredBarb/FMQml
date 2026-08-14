#pragma once

#include <QByteArray>
#include <QColor>
#include <QString>
#include <QVector>
#include <QtPlugin>

inline constexpr int FM_TEXT_PREVIEW_DECORATION_PLUGIN_API_VERSION = 2;

struct TextDecorationSpan {
    int start = 0;
    int length = 0;
    int role = 0;
    bool bold = false;
    bool italic = false;
};

struct TextDecorationRequest {
    QString fileName;
    QString mimeName;
    QString content;
    QByteArray initialState;
};

struct TextDecorationResult {
    bool supported = false;
    QString languageId;
    QString languageLabel;
    bool defaultWrap = false;
    bool defaultLineNumbers = true;
    QString fontFamily;
    QColor role1Color;
    QColor role2Color;
    QColor role3Color;
    QColor role4Color;
    QVector<TextDecorationSpan> spans;
    QByteArray finalState;
};

class TextPreviewDecorationPlugin
{
public:
    virtual ~TextPreviewDecorationPlugin() = default;
    virtual int textDecorationApiVersion() const = 0;
    virtual QString textDecorationPluginId() const = 0;
    virtual QString textDecorationDisplayName() const = 0;
    virtual TextDecorationResult decorateText(const TextDecorationRequest &request) const = 0;
};

#define FM_TEXT_PREVIEW_DECORATION_PLUGIN_IID "FM.TextPreviewDecorationPlugin/2.0"
Q_DECLARE_INTERFACE(TextPreviewDecorationPlugin, FM_TEXT_PREVIEW_DECORATION_PLUGIN_IID)
