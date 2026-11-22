#include "JunctionDesign.h"

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>

void JunctionDesign::installFonts()
{
    for (const char *weight : {"Regular", "SemiBold", "Bold"})
        QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/JunctionDesign/fonts/Geist-%1.ttf").arg(QLatin1String(weight)));

    QFont font(QStringLiteral("Geist"));
    font.setPixelSize(13);   // Fonts.body
    QGuiApplication::setFont(font);
}
