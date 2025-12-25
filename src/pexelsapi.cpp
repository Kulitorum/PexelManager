#include "pexelsapi.h"
#include "settings.h"
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

PexelsApi::PexelsApi(QObject* parent)
    : QObject(parent)
{
}

void PexelsApi::search(const QString& query, int page, int perPage, int minDuration)
{
    cancelSearch();

    QString apiKey = Settings::instance().pexelsApiKey();
    if (apiKey.isEmpty()) {
        emit searchError("Pexels API key not set. Please configure it in Settings.");
        return;
    }

    QUrl url("https://api.pexels.com/videos/search");
    QUrlQuery params;
    params.addQueryItem("query", query);
    params.addQueryItem("page", QString::number(page));
    params.addQueryItem("per_page", QString::number(perPage));
    if (minDuration > 0) {
        params.addQueryItem("min_duration", QString::number(minDuration));
    }
    // Prefer landscape videos
    params.addQueryItem("orientation", "landscape");
    url.setQuery(params);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", apiKey.toUtf8());
    request.setRawHeader("User-Agent", "PexelManager/1.0");

    m_currentReply = m_network.get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &PexelsApi::onSearchFinished);
}

void PexelsApi::cancelSearch()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void PexelsApi::onSearchFinished()
{
    if (!m_currentReply) return;

    auto reply = m_currentReply;
    m_currentReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            emit searchError(QString("Network error: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit searchError(QString("JSON parse error: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject root = doc.object();
    int totalResults = root["total_results"].toInt();
    int page = root["page"].toInt();

    QList<VideoMetadata> videos;
    QJsonArray videosArray = root["videos"].toArray();
    for (const auto& v : videosArray) {
        videos.append(VideoMetadata::fromPexelsJson(v.toObject()));
    }

    emit searchCompleted(videos, totalResults, page);
}
