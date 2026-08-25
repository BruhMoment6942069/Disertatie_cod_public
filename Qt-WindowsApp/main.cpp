#include <complex>
#include <QApplication>
#include "frontend/MainWindow.h"
#include "controllers/MainController.h"
#include <QFile>
#include <QString>
#include <opencv2/opencv.hpp>


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QFile file(":/styles.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        app.setStyleSheet(styleSheet);
    } else {
        qWarning() << "Unable to load style.qss";
    }

    MainWindow window;
    MainController controller(&window);


    window.show();
    cv::Mat test(2,2, CV_8UC1);
    qDebug() << "OpenCV" << CV_VERSION << "mat:" << test.rows << "x" << test.cols;

    return app.exec();
}