//
// Created by Andrei on 23.03.2026.
//

#ifndef DISERTATIE_GRAPHDATA_H
#define DISERTATIE_GRAPHDATA_H

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QLegend>
#include <QMutex>
#include <QTimer>
#include <QList>
#include <QPointF>
#include <QDateTime>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

class graphData : public QChartView {
    Q_OBJECT

public:
    explicit graphData(QWidget *parent = nullptr);

    void pushDataToBuffer(const QList<qint32> &motorPositions);
    void clearData();

private slots:
    void updateGraph();

private:
    QLineSeries* series[4];
    QDateTimeAxis* datetime;
    QChart* chart;
    QValueAxis* m_axisX;
    QValueAxis* m_axisY;
    QList<QPointF> m_buffer[4];
    QMutex m_mutex;
    QTimer* m_renderTimer;

    double m_time = 0.0 ;
};


#endif //DISERTATIE_GRAPHDATA_H