#include "videolistwidget.h"
#include <QKeyEvent>
#include <QPixmap>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>

VideoListWidget::VideoListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setIconSize(QSize(160, 90));
    setSpacing(4);
    setSelectionMode(QAbstractItemView::ExtendedSelection);  // Ctrl+click, Shift+click for multi-select

    connect(this, &QListWidget::currentItemChanged, this, &VideoListWidget::onCurrentItemChanged);
}

// === Search Results ===

void VideoListWidget::setSearchResults(const QList<VideoMetadata>& videos, const QSet<int>& rejectedIds, const QSet<int>& projectIds)
{
    m_searchResults.clear();
    addSearchResults(videos, rejectedIds, projectIds);
}

void VideoListWidget::addSearchResults(const QList<VideoMetadata>& videos, const QSet<int>& rejectedIds, const QSet<int>& projectIds)
{
    qDebug() << "addSearchResults: incoming=" << videos.size()
             << "rejected=" << rejectedIds.size()
             << "inProject=" << projectIds.size();

    int added = 0, skippedDupe = 0, skippedRejected = 0, skippedProject = 0;

    for (const auto& video : videos) {
        if (m_searchResults.contains(video.id)) { skippedDupe++; continue; }
        if (rejectedIds.contains(video.id)) { skippedRejected++; continue; }
        if (projectIds.contains(video.id)) { skippedProject++; continue; }
        m_searchResults[video.id] = video;
        added++;
    }

    qDebug() << "  added=" << added << "skippedDupe=" << skippedDupe
             << "skippedRejected=" << skippedRejected << "skippedProject=" << skippedProject;
    qDebug() << "  m_searchResults.size()=" << m_searchResults.size()
             << "m_viewMode=" << (m_viewMode == SearchResults ? "SearchResults" : "ProjectVideos");

    if (m_viewMode == SearchResults) {
        refreshList();
    }
}

void VideoListWidget::clearSearchResults()
{
    m_searchResults.clear();
    if (m_viewMode == SearchResults) {
        refreshList();
    }
}

QList<VideoMetadata> VideoListWidget::getSearchResults() const
{
    return m_searchResults.values();
}

// === Project Videos ===

void VideoListWidget::setProjectVideos(const QList<VideoMetadata>& videos)
{
    m_projectVideos.clear();
    for (const auto& video : videos) {
        if (!video.isRejected) {
            m_projectVideos[video.id] = video;
        }
    }

    if (m_viewMode == ProjectVideos) {
        refreshList();
    }
}

void VideoListWidget::clearProjectVideos()
{
    m_projectVideos.clear();
    if (m_viewMode == ProjectVideos) {
        refreshList();
    }
}

// === View Mode ===

void VideoListWidget::setViewMode(ViewMode mode)
{
    qDebug() << "setViewMode:" << (mode == SearchResults ? "SearchResults" : "ProjectVideos")
             << "current=" << (m_viewMode == SearchResults ? "SearchResults" : "ProjectVideos");

    if (m_viewMode != mode) {
        m_viewMode = mode;
        refreshList();
    }
}

void VideoListWidget::refreshList()
{
    qDebug() << "refreshList: viewMode=" << (m_viewMode == SearchResults ? "SearchResults" : "ProjectVideos")
             << "searchResults=" << m_searchResults.size()
             << "projectVideos=" << m_projectVideos.size();

    // Cancel pending thumbnails
    for (auto reply : m_pendingThumbnails.keys()) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingThumbnails.clear();

    QListWidget::clear();

    const QMap<int, VideoMetadata>& source =
        (m_viewMode == SearchResults) ? m_searchResults : m_projectVideos;

    qDebug() << "  source.size()=" << source.size();

    for (const auto& video : source) {
        auto item = new QListWidgetItem(this);
        item->setData(Qt::UserRole, video.id);
        updateItemAppearance(item, video);

        if (!video.thumbnailUrl.isEmpty()) {
            loadThumbnail(video.id, video.thumbnailUrl);
        }
    }
}

void VideoListWidget::clear()
{
    m_searchResults.clear();
    m_projectVideos.clear();

    for (auto reply : m_pendingThumbnails.keys()) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingThumbnails.clear();

    QListWidget::clear();
}

VideoMetadata* VideoListWidget::getVideo(int id)
{
    if (m_viewMode == SearchResults) {
        if (m_searchResults.contains(id)) {
            return &m_searchResults[id];
        }
    } else {
        if (m_projectVideos.contains(id)) {
            return &m_projectVideos[id];
        }
    }
    return nullptr;
}

VideoMetadata* VideoListWidget::currentVideo()
{
    auto item = currentItem();
    if (!item) return nullptr;
    int id = item->data(Qt::UserRole).toInt();
    return getVideo(id);
}

void VideoListWidget::markRejected(int id)
{
    bool removed = false;

    if (m_viewMode == SearchResults && m_searchResults.contains(id)) {
        m_searchResults.remove(id);
        removed = true;
    } else if (m_viewMode == ProjectVideos && m_projectVideos.contains(id)) {
        m_projectVideos.remove(id);
        removed = true;
    }

    if (removed) {
        auto item = findItem(id);
        if (item) {
            delete takeItem(row(item));
        }
        emit videoRejected(id);
    }
}

void VideoListWidget::updateVideoStatus(int id, const VideoMetadata* updatedVideo)
{
    // Update internal data if provided
    if (updatedVideo) {
        if (m_projectVideos.contains(id)) {
            m_projectVideos[id] = *updatedVideo;
        }
        if (m_searchResults.contains(id)) {
            m_searchResults[id] = *updatedVideo;
        }
    }

    auto item = findItem(id);
    if (!item) return;

    VideoMetadata* video = nullptr;
    if (m_viewMode == SearchResults && m_searchResults.contains(id)) {
        video = &m_searchResults[id];
    } else if (m_viewMode == ProjectVideos && m_projectVideos.contains(id)) {
        video = &m_projectVideos[id];
    }

    if (video) {
        updateItemAppearance(item, *video);
    }
}

void VideoListWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        auto selectedItems = this->selectedItems();
        if (!selectedItems.isEmpty()) {
            // Collect all IDs first (since we'll be modifying the list)
            QList<int> idsToReject;
            for (auto item : selectedItems) {
                idsToReject.append(item->data(Qt::UserRole).toInt());
            }

            // Find next item to select after deletion
            int lastRow = row(selectedItems.last());
            int nextRow = -1;
            if (lastRow < count() - selectedItems.size()) {
                nextRow = lastRow - selectedItems.size() + 1;
                if (nextRow < 0) nextRow = 0;
            }

            // Now reject all
            for (int id : idsToReject) {
                markRejected(id);
            }

            // Select next item if any remain
            if (count() > 0 && nextRow >= 0 && nextRow < count()) {
                setCurrentRow(nextRow);
            } else if (count() > 0) {
                setCurrentRow(count() - 1);
            }
        }
        event->accept();
        return;
    }

    // Ctrl+A to select all
    if (event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier) {
        selectAll();
        event->accept();
        return;
    }

    QListWidget::keyPressEvent(event);
}

void VideoListWidget::onCurrentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous);
    if (current) {
        int id = current->data(Qt::UserRole).toInt();
        VideoMetadata* video = getVideo(id);
        if (video) {
            qDebug() << "onCurrentItemChanged: video id=" << id;
            qDebug() << "  previewUrl=" << video->previewVideoUrl;
            emit videoSelected(*video);
        }
    }
}

void VideoListWidget::loadThumbnail(int videoId, const QUrl& url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "PexelManager/1.0");

    auto reply = m_thumbnailNetwork.get(request);
    m_pendingThumbnails[reply] = videoId;

    connect(reply, &QNetworkReply::finished, this, &VideoListWidget::onThumbnailLoaded);
}

void VideoListWidget::onThumbnailLoaded()
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int videoId = m_pendingThumbnails.take(reply);

    if (reply->error() == QNetworkReply::NoError) {
        QPixmap pixmap;
        if (pixmap.loadFromData(reply->readAll())) {
            auto item = findItem(videoId);
            if (item) {
                item->setIcon(QIcon(pixmap.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            }
        }
    }

    reply->deleteLater();
}

QListWidgetItem* VideoListWidget::findItem(int videoId)
{
    for (int i = 0; i < count(); ++i) {
        auto item = this->item(i);
        if (item->data(Qt::UserRole).toInt() == videoId) {
            return item;
        }
    }
    return nullptr;
}

void VideoListWidget::updateItemAppearance(QListWidgetItem* item, const VideoMetadata& video)
{
    QString status;
    if (video.isUploaded) {
        status = "[UPLOADED] ";
        item->setForeground(Qt::darkGreen);
    } else if (video.isScaled) {
        status = "[SCALED] ";
        item->setForeground(Qt::darkBlue);
    } else if (video.isDownloaded) {
        status = "[DOWNLOADED] ";
        item->setForeground(Qt::darkCyan);
    } else {
        item->setForeground(Qt::black);
    }

    QString text = QString("%1%2\n%3 - %4s")
        .arg(status)
        .arg(video.id)
        .arg(video.author)
        .arg(video.duration);

    item->setText(text);
}
