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

    if (!m_currentReply) {
        startNextDownload();
    }
}

void DownloadManager::cancelAll()
{
    m_queue.clear();

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_currentFile) {
        m_currentFile->close();
        m_currentFile->remove();
        delete m_currentFile;
        m_currentFile = nullptr;
    }
}

void DownloadManager::startNextDownload()
{
    if (m_queue.isEmpty()) {
        emit allDownloadsCompleted();
        return;
    }

    m_currentTask = m_queue.dequeue();

    // Ensure directory exists
    QFileInfo info(m_currentTask.destPath);
    QDir dir = info.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Check if file already exists
    if (QFile::exists(m_currentTask.destPath)) {
        emit downloadCompleted(m_currentTask.videoId, m_currentTask.destPath);
        startNextDownload();
        return;
    }

    // Create temp file
    QString tempPath = m_currentTask.destPath + ".part";
    m_currentFile = new QFile(tempPath);
    if (!m_currentFile->open(QIODevice::WriteOnly)) {
        emit downloadError(m_currentTask.videoId,
            QString("Cannot create file: %1").arg(tempPath));
        delete m_currentFile;
        m_currentFile = nullptr;
        startNextDownload();
        return;
    }

    emit downloadStarted(m_currentTask.videoId);

    QNetworkRequest request(m_currentTask.url);
    request.setRawHeader("User-Agent", "PexelManager/1.0");

    m_currentReply = m_network.get(request);

    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &DownloadManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &DownloadManager::onDownloadFinished);
    connect(m_currentReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_currentFile && m_currentReply) {
            m_currentFile->write(m_currentReply->readAll());
        }
    });
}

void DownloadManager::onDownloadProgress(qint64 received, qint64 total)
{
    emit downloadProgress(m_currentTask.videoId, received, total);
}

void DownloadManager::onDownloadFinished()
{
    if (!m_currentReply || !m_currentFile) return;

    auto reply = m_currentReply;
    m_currentReply = nullptr;

    // Write any remaining data
    m_currentFile->write(reply->readAll());
    m_currentFile->close();

    QString tempPath = m_currentFile->fileName();
    delete m_currentFile;
    m_currentFile = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        QFile::remove(tempPath);
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            emit downloadError(m_currentTask.videoId,
                QString("Download failed: %1").arg(reply->errorString()));
        }
    } else {
        // Rename temp file to final
        QFile::remove(m_currentTask.destPath);  // Remove if exists
        if (QFile::rename(tempPath, m_currentTask.destPath)) {
            emit downloadCompleted(m_currentTask.videoId, m_currentTask.destPath);
        } else {
            emit downloadError(m_currentTask.videoId, "Failed to rename downloaded file");
        }
    }

    reply->deleteLater();
    startNextDownload();
}
