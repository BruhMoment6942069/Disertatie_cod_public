//
// Created by Andrei on 08.07.2026.
//

#include "VideoView.h"
#include <QPainter>
#include <QVBoxLayout>
#include  <QMouseEvent>

VideoView::VideoView(QWidget* parent) : QWidget(parent) {


}

void VideoView::setFrame(const QImage &frame) {
    _frame = frame;
    update();
}

void VideoView::setFPS(double fps) {
    _fps = fps;
}

void VideoView::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (_frame.isNull()) {
        return;
    }

    QImage scaled = _frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = (width() - scaled.width()) / 2;
    int y = (height() - scaled.height()) / 2;

    _targetRect = QRect(x, y, scaled.width(), scaled.height());
    painter.drawImage(_targetRect, scaled);
    painter.setPen(Qt::green);
    painter.setFont(QFont("Consolas", 12, QFont::Bold));
    painter.drawText(_targetRect.topLeft() + QPoint(8,20), QString::number(_fps, 'f', 1) + " FPS");


}

void VideoView::mousePressEvent(QMouseEvent *event) {
    QPointF pos = event->position();
    if (_frame.isNull() || _targetRect.isEmpty()) {
        return;
    }
    if (!_targetRect.contains(pos.toPoint())) {
        return;
    }
    double scale = double(_frame.width()) / _targetRect.width();
    int fx = qRound((pos.x() - _targetRect.x()) * scale);
    int fy = qRound((pos.y() - _targetRect.y()) * scale);
    fx = qBound(0, fx, _frame.width() - 1);
    fy = qBound(0, fy, _frame.height() - 1);
    emit frameClicked(QPoint(fx, fy));

}
