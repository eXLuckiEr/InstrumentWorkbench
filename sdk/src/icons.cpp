#include "iwp/icons.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>

namespace iwp {

namespace {

QIcon render(const QString &resource, const QColor &tint)
{
    QSvgRenderer renderer(resource);
    if (!renderer.isValid())
        return {};

    QIcon icon;
    for (int size : {16, 20, 24, 32, 48, 64, 96, 128, 256}) {   // large ones serve HiDPI screens
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        if (tint.isValid()) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(pixmap.rect(), tint);   // keeps the shape (alpha), replaces the colour
        }
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

QIcon svgIcon(const QString &resource) { return render(resource, QColor()); }

QIcon themedIcon(const QString &resource)
{
    return render(resource, QGuiApplication::palette().color(QPalette::Text));
}

} // namespace iwp
