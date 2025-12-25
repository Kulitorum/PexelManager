#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QFile>

class DownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit DownloadManager(QObject* parent = nullptr);

    void downloadVideo(int videoId, const QUrl& url, const QString& destPath);
    void cancelAll();

    bool isDownloading() const { return m_currentReply != nullptr || !m_queue.isEmpty(); }
    int pendingCount() const { return m_queue.size() + (m_currentReply ? 1 : 0); }

signals:
    void downloadStarted(int videoId);
    void downloadProgress(int videoId, qint64 received, qint64 total);
    void downloadCompleted(int videoId, const QString& path);
    void downloadError(int videoId, const QString& error);
    void allDownloadsCompleted();

private slots:
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

private:
    void startNextDownload();

    struct DownloadTask {
        int videoId;
        QUrl url;
        QString destPath;
    };

    QNetworkAccessManager m_network;
    QNetworkReply* m_currentReply = nullptr;
    QFile* m_currentFile = nullptr;
    DownloadTask m_currentTask;
    QQueue<DownloadTask> m_queue;
};
