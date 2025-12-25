#include "downloadmanager.h"
#include <QFileInfo>
#include <QDir>

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent)
{
}

void DownloadManager::downloadVideo(int videoId, const QUrl& url, const QString& destPath)
{
    DownloadTask task;
    task.videoId = videoId;
    task.url = url;
    task.destPath = destPath;

    m_queue.enqueue(task);
    startDownloads();
}

void DownloadManager::cancelAll()
{
    m_queue.clear();

    for (auto reply : m_activeDownloads.keys()) {
        ActiveDownload& download = m_activeDownloads[reply];
        reply->abort();
        reply->deleteLater();
        if (download.file) {
            download.file->close();
            download.file->remove();
            delete download.file;
        }
    }
    m_activeDownloads.clear();
}

void DownloadManager::startDownloads()
{
    while (m_activeDownloads.size() < MAX_CONCURRENT_DOWNLOADS && !m_queue.isEmpty()) {
        DownloadTask task = m_queue.dequeue();

        // Ensure directory exists
        QFileInfo info(task.destPath);
        QDir dir = info.absoluteDir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // Check if file already exists
        if (QFile::exists(task.destPath)) {
            emit downloadCompleted(task.videoId, task.destPath);
            continue;  // Try next in queue
        }

        // Create temp file
        QString tempPath = task.destPath + ".part";
        QFile* file = new QFile(tempPath);
        if (!file->open(QIODevice::WriteOnly)) {
            emit downloadError(task.videoId,
                QString("Cannot create file: %1").arg(tempPath));
            delete file;
            continue;  // Try next in queue
        }

        emit downloadStarted(task.videoId);

        QNetworkRequest request(task.url);
        request.setRawHeader("User-Agent", "PexelManager/1.0");

        QNetworkReply* reply = m_network.get(request);

        ActiveDownload download;
        download.task = task;
        download.file = file;
        m_activeDownloads[reply] = download;

        connect(reply, &QNetworkReply::downloadProgress,
                this, &DownloadManager::onDownloadProgress);
        connect(reply, &QNetworkReply::finished,
                this, &DownloadManager::onDownloadFinished);
        connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
            if (m_activeDownloads.contains(reply)) {
                ActiveDownload& download = m_activeDownloads[reply];
                if (download.file) {
                    download.file->write(reply->readAll());
                }
            }
        });
    }

    // Check if all done
    if (m_activeDownloads.isEmpty() && m_queue.isEmpty()) {
        emit allDownloadsCompleted();
    }
}

void DownloadManager::onDownloadProgress(qint64 received, qint64 total)
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_activeDownloads.contains(reply)) return;

    emit downloadProgress(m_activeDownloads[reply].task.videoId, received, total);
}

void DownloadManager::onDownloadFinished()
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_activeDownloads.contains(reply)) return;

    ActiveDownload download = m_activeDownloads.take(reply);

    // Write any remaining data
    if (download.file) {
        download.file->write(reply->readAll());
        download.file->close();
    }

    QString tempPath = download.file ? download.file->fileName() : QString();
    delete download.file;

    if (reply->error() != QNetworkReply::NoError) {
        if (!tempPath.isEmpty()) {
            QFile::remove(tempPath);
        }
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            emit downloadError(download.task.videoId,
                QString("Download failed: %1").arg(reply->errorString()));
        }
    } else {
        // Rename temp file to final
        QFile::remove(download.task.destPath);  // Remove if exists
        if (QFile::rename(tempPath, download.task.destPath)) {
            emit downloadCompleted(download.task.videoId, download.task.destPath);
        } else {
            emit downloadError(download.task.videoId, "Failed to rename downloaded file");
        }
    }

    reply->deleteLater();

    // Start more downloads if available
    startDownloads();
}
