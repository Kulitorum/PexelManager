#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QFile>
#include <QMap>

class DownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit DownloadManager(QObject* parent = nullptr);

    void downloadVideo(int videoId, const QUrl& url, const QString& destPath);
    void cancelAll();

    bool isDownloading() const { return !m_activeDownloads.isEmpty() || !m_queue.isEmpty(); }
    int pendingCount() const { return m_queue.size() + m_activeDownloads.size(); }

    static const int MAX_CONCURRENT_DOWNLOADS = 8;

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
    void startDownloads();

    struct DownloadTask {
        int videoId;
        QUrl url;
        QString destPath;
    };

    struct ActiveDownload {
        DownloadTask task;
        QFile* file;
    };

    QNetworkAccessManager m_network;
    QMap<QNetworkReply*, ActiveDownload> m_activeDownloads;
    QQueue<DownloadTask> m_queue;
};
