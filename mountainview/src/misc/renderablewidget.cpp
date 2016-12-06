/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 10/25/2016
*******************************************************/

#include "renderablewidget.h"
#include <QPainter>
#include <QtDebug>

class RenderableWidgetPrivate {
public:
    RenderableWidget* q;
    bool m_export_mode = false;
};

RenderableWidget::RenderableWidget(QWidget* parent)
    : QWidget(parent)
{
    d = new RenderableWidgetPrivate;
    d->q = this;
}

RenderableWidget::~RenderableWidget()
{
    delete d;
}

void RenderableWidget::setExportMode(bool val)
{
    d->m_export_mode = val;
}

bool RenderableWidget::exportMode() const
{
    return d->m_export_mode;
}

void RenderableWidget::renderView(QPainter *painter, const RenderOptionSet *options, const QRectF &rect)
{
    qDebug() << Q_FUNC_INFO << metaObject()->className();
    // fallback to rendering the image
    QImage img = renderImage(rect.width(), rect.height());
    painter->drawImage(rect, img);
}
