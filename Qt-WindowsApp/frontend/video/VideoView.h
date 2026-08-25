//
// Created by Andrei on 08.07.2026.
//

#ifndef QT_WINDOWSAPP_VIDEOVIEW_H
#define QT_WINDOWSAPP_VIDEOVIEW_H
#include <QWidget>
#include <QImage>



class VideoView : public QWidget{
    Q_OBJECT
public:
    explicit VideoView(QWidget* parent = nullptr);
public slots:
    void setFrame(const QImage& frame);
    void setFPS(double fps);
signals:
    void frameClicked(QPoint framePos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    QImage _frame;
    QRect  _targetRect;
    double _fps = 0;

};


#endif //QT_WINDOWSAPP_VIDEOVIEW_H