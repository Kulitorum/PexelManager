#pragma once

#include <QListWidget>
#include <QNetworkAccessManager>
#include <QMap>
#include <QSet>
#include "videometadata.h"

class VideoListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit VideoListWidget(QWidget* parent = nullptr);

    // Search results (temporary, before adding to project)
    void setSearchResults(const QList<VideoMetadata>& videos, const QSet<int>& rejectedIds, const QSet<int>& projectIds);
    void addSearchResults(const QList<VideoMetadata>& videos, const QSet<int>& rejectedIds, const QSet<int>& projectIds);
    void clearSearchResults();
    QList<VideoMetadata> getSearchResults() const;
    int searchResultsCount() const { return m_searchResults.size(); }

    // Project videos
    void setProjectVideos(const QList<VideoMetadata>& videos);
    void clearProjectVideos();
    int projectVideosCount() const { return m_projectVideos.size(); }

    // View toggle
    enum ViewMode { SearchResults, ProjectVideos };
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_viewMode; }

    void clear();

    VideoMetadata* getVideo(int id);
    VideoMetadata* currentVideo();

    void markRejected(int id);
    void updateVideoStatus(int id, const VideoMetadata* updatedVideo = nullptr);

signals:
    void videoSelected(const VideoMetadata& video);
    void videoRejected(int id);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onCurrentItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void onThumbnailLoaded();

private:
    void loadThumbnail(int videoId, const QUrl& url);
    QListWidgetItem* findItem(int videoId);
    void updateItemAppearance(QListWidgetItem* item, const VideoMetadata& video);
    void refreshList();

    ViewMode m_viewMode = SearchResults;
    QMap<int, VideoMetadata> m_searchResults;
    QMap<int, VideoMetadata> m_projectVideos;
    QNetworkAccessManager m_thumbnailNetwork;
    QMap<QNetworkReply*, int> m_pendingThumbnails;
};
