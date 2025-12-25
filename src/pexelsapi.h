#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "videometadata.h"

class PexelsApi : public QObject
{
    Q_OBJECT

public:
    explicit PexelsApi(QObject* parent = nullptr);

    void search(const QString& query, int page = 1, int perPage = 20, int minDuration = 0);
    void cancelSearch();

    bool isSearching() const { return m_currentReply != nullptr; }

signals:
    void searchCompleted(const QList<VideoMetadata>& videos, int totalResults, int page);
    void searchError(const QString& error);

private slots:
    void onSearchFinished();

private:
    QNetworkAccessManager m_network;
    QNetworkReply* m_currentReply = nullptr;
};
