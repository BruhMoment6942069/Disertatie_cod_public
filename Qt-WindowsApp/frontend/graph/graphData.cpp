//
// Created by Andrei on 23.03.2026.
//

#include "graphData.h"

#include <QDebug>
#include <QTimer>

graphData::graphData(QWidget *parent) : QChartView(parent) {
    QList<QColor> colors = {QColor(0,120,215), QColor(220, 50, 50), QColor(50, 180, 50), QColor(180, 50,220)};
    QStringList names = {"Ch 1","Ch 2","Ch 3", "Ch 4"};
    chart = new QChart();
    chart->setTitle("Smooth Spline Visualization");
    chart->legend()->hide();
    chart->setAnimationOptions(QChart::NoAnimation);

    for (int i = 0; i < 4; i++) {
        series[i] = new QLineSeries(this);
        series[i]->setName(names[i]);
        QPen pen(colors[i]);
        pen.setWidth(2);
        series[i]->setPen(pen);
        chart->addSeries(series[i]);

    }


    // AXES
    m_axisX = new QValueAxis();
    m_axisX->setRange(0, 100 * 0.05 );
    m_axisX->setTitleText("Time (s)");
    m_axisX->setTickCount(11);
    chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis();
    m_axisY->setRange(0, 360);
    m_axisY->setTitleText("Grade");
    chart->addAxis(m_axisY, Qt::AlignLeft);

    for (int i = 0; i < 4; i++) {

        series[i]->attachAxis(m_axisX);
        series[i]->attachAxis(m_axisY);
    }
    //Chart View
    this->setChart(chart);
    this->setRenderHint(QPainter::Antialiasing);

    m_renderTimer = new QTimer(this);
    connect(m_renderTimer, &QTimer::timeout, this, &graphData::updateGraph);
    m_renderTimer->start(static_cast<int>(0.05 * 1000));

}

void graphData::pushDataToBuffer(const QList<qint32> &motorPositions) {
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < 4; i++) {
        m_buffer[i].append(QPointF(m_time, motorPositions.value(i)));
    }
    m_time += 0.05;
}

void graphData::clearData() {
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < 4; i++) {
        m_buffer[i].clear();
        series[i]->clear();
    }
    m_time = 0.0;
}

void graphData::updateGraph() {
    for (int i = 0; i < 4; i++) {

        QList<QPointF> dataToDraw;
        QMutexLocker locker(&m_mutex);
        dataToDraw = m_buffer[i];
        m_buffer[i].clear();
        series[i]->append(dataToDraw);
        while (series[i]->count() > 100) {
            series[i]->remove(0);
        }
    }
    double xMax = m_time;
    double xMin = xMax - 100 * 0.05;
    m_axisX->setRange(xMin, xMax);

}

