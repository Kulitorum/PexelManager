#pragma once

#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QMap>
#include "mediametadata.h"

class UploadManager : public QObject
{
    Q_OBJECT

public:
    explicit UploadManager(QObject* parent = nullptr);

    void scaleMedia(int mediaId, MediaType type, const QString& inputPath, const QString& outputPath,
                    int targetWidth, int targetHeight, int crf, const QString& preset);
    void uploadToS3(int mediaId, const QString& localPath, const QString& bucket, const QString& key);
    void uploadIndexJson(const QString& bucket, const QString& categoryId, const QString& projectName);
    void uploadCatalogJson(const QString& bucket, const QString& categoryId, const QList<MediaMetadata>& media);
    void uploadCategoriesJson(const QString& bucket, const QString& categoryId, const QString& projectName);
    void deleteFromS3(const QString& bucket, const QString& categoryId);
    void removeCategoryAndUpload(const QString& bucket, const QString& categoryId);
    void cancelAll();

    bool isBusy() const { return !m_runningScales.isEmpty() || !m_runningUploads.isEmpty() || !m_scaleQueue.isEmpty() || !m_uploadQueue.isEmpty(); }

    static const int MAX_CONCURRENT_SCALES = 8;
    static const int MAX_CONCURRENT_UPLOADS = 8;

signals:
    void scaleStarted(int mediaId);
    void scaleCompleted(int mediaId, const QString& outputPath);
    void scaleError(int mediaId, const QString& error);

    void uploadStarted(int mediaId);
    void uploadCompleted(int mediaId);
    void uploadError(int mediaId, const QString& error);

    void indexUploadCompleted();
    void indexUploadError(const QString& error);

    void categoriesUploadCompleted();
    void categoriesUploadError(const QString& error);

    void s3DeleteCompleted(const QString& bucket);
    void s3DeleteError(const QString& bucket, const QString& error);

    void allTasksCompleted();

private slots:
    void onScaleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onScaleProcessError(QProcess::ProcessError error);
    void onUploadProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onUploadProcessError(QProcess::ProcessError error);

private:
    void startScaleTasks();
    void startUploadTasks();

    enum TaskType { Scale, Upload, IndexUpload, CatalogUpload, CategoriesUpload, S3Delete };

    struct Task {
        TaskType type;
        MediaType mediaType = MediaType::Video;
        int mediaId;
        QString inputPath;
        QString outputPath;
        QString bucket;
        QString key;
        QString categoryId;
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
    QString m_tempCatalogPath;
    QString m_tempCategoriesPath;
};
