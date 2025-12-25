#pragma once

#include <QObject>
#include <QProcess>
#include <QQueue>

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

    bool isBusy() const { return m_currentProcess != nullptr || !m_scaleQueue.isEmpty() || !m_uploadQueue.isEmpty(); }

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
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    void startNextTask();

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

    QProcess* m_currentProcess = nullptr;
    Task m_currentTask;
    QQueue<Task> m_scaleQueue;
    QQueue<Task> m_uploadQueue;
    QString m_tempIndexPath;
};
