#pragma once

#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QMap>

class UploadManager : public QObject
{
    Q_OBJECT

public:
    explicit UploadManager(QObject* parent = nullptr);

    void scaleVideo(int videoId, const QString& inputPath, const QString& outputPath,
                    int targetWidth, int targetHeight, int crf, const QString& preset);
    void uploadToS3(int videoId, const QString& localPath, const QString& bucket, const QString& key);
    void uploadIndexJson(const QString& bucket, const QString& projectName);
    void cancelAll();

    bool isBusy() const { return !m_runningScales.isEmpty() || !m_runningUploads.isEmpty() || !m_scaleQueue.isEmpty() || !m_uploadQueue.isEmpty(); }

    static const int MAX_CONCURRENT_SCALES = 8;
    static const int MAX_CONCURRENT_UPLOADS = 8;

signals:
    void scaleStarted(int videoId);
    void scaleCompleted(int videoId, const QString& outputPath);
    void scaleError(int videoId, const QString& error);

    void uploadStarted(int videoId);
    void uploadCompleted(int videoId);
    void uploadError(int videoId, const QString& error);

    void indexUploadCompleted();
    void indexUploadError(const QString& error);

    void allTasksCompleted();

private slots:
    void onScaleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onScaleProcessError(QProcess::ProcessError error);
    void onUploadProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onUploadProcessError(QProcess::ProcessError error);

private:
    void startScaleTasks();
    void startUploadTasks();

    enum TaskType { Scale, Upload, IndexUpload };

    struct Task {
        TaskType type;
        int videoId;
        QString inputPath;
        QString outputPath;
        QString bucket;
        QString key;
        QString projectName;
        int targetWidth;
        int targetHeight;
        int crf;
        QString preset;
    };

    // Scale processes (up to MAX_CONCURRENT_SCALES)
    QMap<QProcess*, Task> m_runningScales;
    QQueue<Task> m_scaleQueue;

    // Upload processes (up to MAX_CONCURRENT_UPLOADS)
    QMap<QProcess*, Task> m_runningUploads;
    QQueue<Task> m_uploadQueue;

    QString m_tempIndexPath;
};
