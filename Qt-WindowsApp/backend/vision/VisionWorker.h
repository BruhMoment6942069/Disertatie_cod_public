//
// Created by Andrei on 08.07.2026.
//

#ifndef QT_WINDOWSAPP_VISIONWORKER_H
#define QT_WINDOWSAPP_VISIONWORKER_H
#include <QObject>
#include <opencv2/videoio.hpp>
#include <opencv2/core/base.hpp>
#include <QTimer>
#include <QImage>
#include <QElapsedTimer>
#include <vector>
#include <algorithm>
#include <QSettings>

class VisionWorker : public QObject{
    Q_OBJECT

public:
    explicit VisionWorker(int camId, QObject* parent = nullptr);

    signals:
    void frameReady(int id, const QImage& frame);
    void cameraFailed(const QString& reason);
    void fpsUpdated(int id, double fps);
    void autoCalibrateFinished(int id);
    void ballTracked(int id, bool locked, double errX, double errY);

public slots:
    void startCapture(int camId, int cameraIndex);
    void stopCapture(int camId);
    void setExposureLock(bool locked);
    void setRecording(bool recording);
    void startPlayback(int camId, const QString& path);
    void setDebugView(int view);
    void sampleAt(int camId, QPoint p);
    void resetSamples();
    void setAutoCalibrate(bool on);

private slots:
    void grabFrame();


private:
    void samplePixels(const cv::Mat& hsv, const cv::Mat& mask);
    cv::Mat buildMask(const cv::Mat& frame);
    void computeBounds();
    void detectBall(const cv::Mat& frame, const cv::Rect& roi);
    void updateTracker(const cv::Mat& frame);

    std::vector<uchar> _poolH, _poolS, _poolV;
    cv::VideoCapture _cap;
    QTimer* _grabTimer = nullptr;
    int _frameCount = 0;
    QElapsedTimer _elapsed;
    cv::VideoWriter _writer;
    bool _isFileSource = false;
    int _debugView = 0;
    cv::Mat _lastRaw;
    cv::Scalar _lower, _upper;
    int _camId = 0;
    QSettings _settings;
    bool _autoCalib = false;
    int _stableCount = 0;
    cv::Vec3f _lastCircle;
    cv::Point2f _detCenter;
    float _detRadius = 0.f;
    bool _detected = false;
    enum class TrackState {
        Searching, Locked
    };
    TrackState _trackState = TrackState::Searching;
    cv::Point2f _trackCenter;
    float _trackRadius = 0.f;
    int _missCount = 0;
    cv::Rect _searchRoi;


};


#endif //QT_WINDOWSAPP_VISIONWORKER_H

