#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "mediametadata.h"

enum class SearchType {
    Videos,
    Photos
};

class PexelsApi : public QObject
{
    Q_OBJECT

public:
    explicit PexelsApi(QObject* parent = nullptr);

    void search(const QString& query, SearchType type, int page = 1, int perPage = 20, int minDuration = 0);
    void searchVideos(const QString& query, int page = 1, int perPage = 20, int minDuration = 0);
    void searchPhotos(const QString& query, int page = 1, int perPage = 20);
    void cancelSearch();

    bool isSearching() const { return m_currentReply != nullptr; }

signals:
    void searchCompleted(const QList<MediaMetadata>& media, int totalResults, int page);
    void searchError(const QString& error);

private slots:
    void onSearchFinished();

private:
    QNetworkAccessManager m_network;
    QNetworkReply* m_currentReply = nullptr;
    SearchType m_currentSearchType = SearchType::Videos;
};
