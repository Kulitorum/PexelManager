#pragma once

#include <QListWidget>
#include <QNetworkAccessManager>
#include <QMap>
#include <QSet>
#include "mediametadata.h"

class MediaListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit MediaListWidget(QWidget* parent = nullptr);

    // Search results (temporary, before adding to project)
    void setSearchResults(const QList<MediaMetadata>& media, const QSet<int>& rejectedIds, const QSet<int>& projectIds);
    void addSearchResults(const QList<MediaMetadata>& media, const QSet<int>& rejectedIds, const QSet<int>& projectIds);
    void clearSearchResults();
    QList<MediaMetadata> getSearchResults() const;
    int searchResultsCount() const { return m_searchResults.size(); }

    // Project media
    void setProjectMedia(const QList<MediaMetadata>& media);
    void clearProjectMedia();
    int projectMediaCount() const { return m_projectMedia.size(); }

    // View toggle
    enum ViewMode { SearchResults, ProjectMedia };
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_viewMode; }

    void clear();

    MediaMetadata* getMedia(int id);
    MediaMetadata* currentMedia();

    void markRejected(int id);
    void updateMediaStatus(int id, const MediaMetadata* updatedMedia = nullptr);

signals:
    void mediaSelected(const MediaMetadata& media);
    void mediaRejected(int id);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onCurrentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void onThumbnailLoaded();

private:
    void loadThumbnail(int mediaId, const QUrl& url);
    QListWidgetItem* findItem(int mediaId);
    void updateItemAppearance(QListWidgetItem* item, const MediaMetadata& media);
    void refreshList();

    ViewMode m_viewMode = SearchResults;
    QMap<int, MediaMetadata> m_searchResults;
    QMap<int, MediaMetadata> m_projectMedia;
    QNetworkAccessManager m_thumbnailNetwork;
    QMap<QNetworkReply*, int> m_pendingThumbnails;
};
