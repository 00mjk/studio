#include "wplabel.h"
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include "mainwindow.h"

namespace gams {
namespace studio {

WpLabel::WpLabel(QWidget *parent) : QLabel(parent)
{
}

WpLabel::WpLabel(const QString &content, const QString &link, QWidget *parent)
    : QLabel(parent), mContent(content), mLink(link)
{
    QLabel::setText(mContent);
}

void WpLabel::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
    setFrameShape(QFrame::Box);
}

void WpLabel::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    setFrameShape(QFrame::StyledPanel);
}

void WpLabel::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!mLink.isNull()) { // file history
        QLabel::linkActivated(mLink);

    // added via designer from here on
    } else if (!this->property("link").isNull()) { // web- or file links, open directly
        QString link = this->property("link").toString();
        QDesktopServices::openUrl(QUrl(link, QUrl::TolerantMode));

    } else if (!this->property("action").isNull()) { // actions
        QString action = this->property("action").toString();
        emit relayActionLab(action);

    } else if (!this->property("modlib").isNull()) { // load item from model library

    }
}


}
}
