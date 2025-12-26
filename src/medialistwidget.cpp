#include "medialistwidget.h"
#include <QKeyEvent>
#include <QPixmap>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>

MediaListWidget::MediaListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setIconSize(QSize(160, 90));
    setSpacing(4);
    setSelectionMode(QAbstractItemView::ExtendedSelection);  // Ctrl+click, Shift+click for multi-select

    connect(this, &QListWidget::currentItemChanged, this, &MediaListWidget::onCurrentItemChanged);
}

// === Search Results ===

void MediaListWidget::setSearchResults(const QList<MediaMetadata>& media, const QSet<int>& rejectedIds, const QSet<int>& projectIds)
{
    m_searchResults.clear();
    addSearchResults(media, rejectedIds, projectIds);
}

void MediaListWidget::addSearchResults(const QList<MediaMetadata>& media, const QSet<int>& rejectedIds, const QSet<int>& projectIds)
{
    qDebug() << "addSearchResults: incoming=" << media.size()
             << "rejected=" << rejectedIds.size()
             << "inProject=" << projectIds.size();

    int added = 0, skippedDupe = 0, skippedRejected = 0, skippedProject = 0;

    for (const auto& item : media) {
        if (m_searchResults.contains(item.id)) { skippedDupe++; continue; }
        if (rejectedIds.contains(item.id)) { skippedRejected++; continue; }
        if (projectIds.contains(item.id)) { skippedProject++; continue; }
        m_searchResults[item.id] = item;
        added++;
    }

    qDebug() << "  added=" << added << "skippedDupe=" << skippedDupe
             << "skippedRejected=" << skippedRejected << "skippedProject=" << skippedProject;
    qDebug() << "  m_searchResults.size()=" << m_searchResults.size()
             << "m_viewMode=" << (m_viewMode == SearchResults ? "SearchResults" : "ProjectMedia");

    if (m_viewMode == SearchResults) {
        refreshList();
    }
}

void MediaListWidget::clearSearchResults()
{
    m_searchResults.clear();
    if (m_viewMode == SearchResults) {
        refreshList();
    }
}

QList<MediaMetadata> MediaListWidget::getSearchResults() const
{
    return m_searchResults.values();
}

// === Project Media ===

void MediaListWidget::setProjectMedia(const QList<MediaMetadata>& media)
{
    m_projectMedia.clear();
    for (const auto& item : media) {
        if (!item.isRejected) {
            m_projectMedia[item.id] = item;
        }
    }

    if (m_viewMode == ProjectMedia) {
        refreshList();
    }
}

void MediaListWidget::clearProjectMedia()
{
    m_projectMedia.clear();
    if (m_viewMode == ProjectMedia) {
        refreshList();
    }
}

// === View Mode ===

void MediaListWidget::setViewMode(ViewMode mode)
{
    qDebug() << "setViewMode:" << (mode == SearchResults ? "SearchResults" : "ProjectMedia")
             << "current=" << (m_viewMode == SearchResults ? "SearchResults" : "ProjectMedia");

    if (m_viewMode != mode) {
        m_viewMode = mode;
        refreshList();
    }
}

void MediaListWidget::refreshList()
{
    qDebug() << "refreshList: viewMode=" << (m_viewMode == SearchResults ? "SearchResults" : "ProjectMedia")
             << "searchResults=" << m_searchResults.size()
             << "projectMedia=" << m_projectMedia.size();

    // Cancel pending thumbnails
    for (auto reply : m_pendingThumbnails.keys()) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingThumbnails.clear();

    QListWidget::clear();

    const QMap<int, MediaMetadata>& source =
        (m_viewMode == SearchResults) ? m_searchResults : m_projectMedia;

    qDebug() << "  source.size()=" << source.size();

    for (const auto& item : source) {
        auto listItem = new QListWidgetItem(this);
        listItem->setData(Qt::UserRole, item.id);
        updateItemAppearance(listItem, item);

        if (!item.thumbnailUrl.isEmpty()) {
            loadThumbnail(item.id, item.thumbnailUrl);
        }
    }
}

void MediaListWidget::clear()
{
    m_searchResults.clear();
    m_projectMedia.clear();

    for (auto reply : m_pendingThumbnails.keys()) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingThumbnails.clear();

    QListWidget::clear();
}

MediaMetadata* MediaListWidget::getMedia(int id)
{
    if (m_viewMode == SearchResults) {
        if (m_searchResults.contains(id)) {
            return &m_searchResults[id];
        }
    } else {
        if (m_projectMedia.contains(id)) {
            return &m_projectMedia[id];
        }
    }
    return nullptr;
}

MediaMetadata* MediaListWidget::currentMedia()
{
    auto item = currentItem();
    if (!item) return nullptr;
    int id = item->data(Qt::UserRole).toInt();
    return getMedia(id);
}

void MediaListWidget::markRejected(int id)
{
    bool removed = false;

    if (m_viewMode == SearchResults && m_searchResults.contains(id)) {
        m_searchResults.remove(id);
        removed = true;
    } else if (m_viewMode == ProjectMedia && m_projectMedia.contains(id)) {
        m_projectMedia.remove(id);
        removed = true;
    }

    if (removed) {
        auto item = findItem(id);
        if (item) {
            delete takeItem(row(item));
        }
        emit mediaRejected(id);
    }
}

void MediaListWidget::updateMediaStatus(int id, const MediaMetadata* updatedMedia)
{
    // Update internal data if provided
    if (updatedMedia) {
        if (m_projectMedia.contains(id)) {
            m_projectMedia[id] = *updatedMedia;
        }
        if (m_searchResults.contains(id)) {
            m_searchResults[id] = *updatedMedia;
        }
    }

    auto item = findItem(id);
    if (!item) return;

    MediaMetadata* media = nullptr;
    if (m_viewMode == SearchResults && m_searchResults.contains(id)) {
        media = &m_searchResults[id];
    } else if (m_viewMode == ProjectMedia && m_projectMedia.contains(id)) {
        media = &m_projectMedia[id];
    }

    if (media) {
        updateItemAppearance(item, *media);
    }
}

void MediaListWidget::keyPressEvent(QKeyEvent* event)
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

void MediaListWidget::onCurrentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous);
    if (current) {
        int id = current->data(Qt::UserRole).toInt();
        MediaMetadata* media = getMedia(id);
        if (media) {
            qDebug() << "onCurrentItemChanged: media id=" << id;
            qDebug() << "  type=" << (media->isVideo() ? "video" : "image");
            emit mediaSelected(*media);
        }
    }
}

void MediaListWidget::loadThumbnail(int mediaId, const QUrl& url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "PexelManager/1.0");

    auto reply = m_thumbnailNetwork.get(request);
    m_pendingThumbnails[reply] = mediaId;

    connect(reply, &QNetworkReply::finished, this, &MediaListWidget::onThumbnailLoaded);
}

void MediaListWidget::onThumbnailLoaded()
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int mediaId = m_pendingThumbnails.take(reply);

    if (reply->error() == QNetworkReply::NoError) {
        QPixmap pixmap;
        if (pixmap.loadFromData(reply->readAll())) {
            auto item = findItem(mediaId);
            if (item) {
                item->setIcon(QIcon(pixmap.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            }
        }
    }

    reply->deleteLater();
}

QListWidgetItem* MediaListWidget::findItem(int mediaId)
{
    for (int i = 0; i < count(); ++i) {
        auto item = this->item(i);
        if (item->data(Qt::UserRole).toInt() == mediaId) {
            return item;
        }
    }
    return nullptr;
}

void MediaListWidget::updateItemAppearance(QListWidgetItem* item, const MediaMetadata& media)
{
    QString status;
    if (media.isUploaded) {
        status = "[UPLOADED] ";
        item->setForeground(Qt::darkGreen);
    } else if (media.isScaled) {
        status = "[SCALED] ";
        item->setForeground(Qt::darkBlue);
    } else if (media.isDownloaded) {
        status = "[DOWNLOADED] ";
        item->setForeground(Qt::darkCyan);
    } else {
        item->setForeground(Qt::black);
    }

    QString typeIndicator = media.isImage() ? "[IMG] " : "";
    QString durationOrSize;
    if (media.isVideo()) {
        durationOrSize = QString("%1s").arg(media.duration);
    } else {
        durationOrSize = QString("%1x%2").arg(media.width).arg(media.height);
    }

    QString text = QString("%1%2%3\n%4 - %5")
        .arg(status)
        .arg(typeIndicator)
        .arg(media.id)
        .arg(media.author)
        .arg(durationOrSize);

    item->setText(text);
}
