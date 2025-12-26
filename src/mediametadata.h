#pragma once

#include <QString>
#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>

enum class MediaType {
    Video,
    Image
};

struct MediaFile {
    int width = 0;
    int height = 0;
    QString quality;
    QUrl link;

    static MediaFile fromJson(const QJsonObject& json) {
        MediaFile mf;
        mf.width = json["width"].toInt();
        mf.height = json["height"].toInt();
        mf.quality = json["quality"].toString();
        mf.link = QUrl(json["link"].toString());
        return mf;
    }
};

struct MediaMetadata {
    MediaType type = MediaType::Video;
    int id = 0;
    int duration = 0;  // 0 for images
    int width = 0;
    int height = 0;
    QString author;
    QUrl authorUrl;
    QUrl sourceUrl;
    QUrl thumbnailUrl;

    // Video-specific
    QUrl previewVideoUrl;
    QList<MediaFile> mediaFiles;

    // Image-specific
    QUrl originalImageUrl;
    QUrl largeImageUrl;

    // Local state
    QString localRawPath;
    QString localScaledPath;
    bool isRejected = false;
    bool isDownloaded = false;
    bool isScaled = false;
    bool isUploaded = false;

    bool isVideo() const { return type == MediaType::Video; }
    bool isImage() const { return type == MediaType::Image; }

    static MediaMetadata fromPexelsVideoJson(const QJsonObject& json) {
        MediaMetadata m;
        m.type = MediaType::Video;
        m.id = json["id"].toInt();
        m.duration = json["duration"].toInt();
        m.width = json["width"].toInt();
        m.height = json["height"].toInt();

        auto user = json["user"].toObject();
        m.author = user["name"].toString();
        m.authorUrl = QUrl(user["url"].toString());
        m.sourceUrl = QUrl(json["url"].toString());

        // Get thumbnail from image field
        m.thumbnailUrl = QUrl(json["image"].toString());

        // Parse video files
        auto files = json["video_files"].toArray();
        for (const auto& f : files) {
            auto mf = MediaFile::fromJson(f.toObject());
            if (!mf.link.isEmpty()) {
                m.mediaFiles.append(mf);
            }
        }

        // Find a good preview video (smaller resolution)
        for (const auto& mf : m.mediaFiles) {
            if (mf.quality == "sd" || mf.width <= 640) {
                m.previewVideoUrl = mf.link;
                break;
            }
        }
        if (m.previewVideoUrl.isEmpty() && !m.mediaFiles.isEmpty()) {
            m.previewVideoUrl = m.mediaFiles.first().link;
        }

        return m;
    }

    static MediaMetadata fromPexelsPhotoJson(const QJsonObject& json) {
        MediaMetadata m;
        m.type = MediaType::Image;
        m.id = json["id"].toInt();
        m.duration = 0;
        m.width = json["width"].toInt();
        m.height = json["height"].toInt();

        m.author = json["photographer"].toString();
        m.authorUrl = QUrl(json["photographer_url"].toString());
        m.sourceUrl = QUrl(json["url"].toString());

        // Parse image sources
        auto src = json["src"].toObject();
        m.thumbnailUrl = QUrl(src["medium"].toString());
        m.originalImageUrl = QUrl(src["original"].toString());
        m.largeImageUrl = QUrl(src["large2x"].toString());

        // Fallback to large if large2x not available
        if (m.largeImageUrl.isEmpty()) {
            m.largeImageUrl = QUrl(src["large"].toString());
        }

        return m;
    }

    // Get best video file for download based on max resolution preference
    MediaFile getBestMediaFile(int maxWidth = 1920) const {
        MediaFile best;
        int bestArea = 0;

        for (const auto& mf : mediaFiles) {
            if (mf.width <= maxWidth) {
                int area = mf.width * mf.height;
                if (area > bestArea) {
                    bestArea = area;
                    best = mf;
                }
            }
        }

        // Fallback to smallest if nothing fits
        if (best.link.isEmpty() && !mediaFiles.isEmpty()) {
            best = mediaFiles.first();
            for (const auto& mf : mediaFiles) {
                if (mf.width * mf.height < best.width * best.height) {
                    best = mf;
                }
            }
        }

        return best;
    }

    // Get download URL for this media item
    QUrl getDownloadUrl(int maxWidth = 1920) const {
        if (type == MediaType::Image) {
            // Prefer large image, fallback to original
            return largeImageUrl.isEmpty() ? originalImageUrl : largeImageUrl;
        } else {
            return getBestMediaFile(maxWidth).link;
        }
    }

    // Get file extension for this media type
    QString getFileExtension() const {
        return (type == MediaType::Image) ? ".jpg" : ".mp4";
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["type"] = (type == MediaType::Video) ? "video" : "image";
        obj["id"] = id;
        obj["duration"] = duration;
        obj["width"] = width;
        obj["height"] = height;
        obj["author"] = author;
        obj["author_url"] = authorUrl.toString();
        obj["source_url"] = sourceUrl.toString();
        obj["thumbnail_url"] = thumbnailUrl.toString();
        obj["local_raw_path"] = localRawPath;
        obj["local_scaled_path"] = localScaledPath;
        obj["is_rejected"] = isRejected;
        obj["is_downloaded"] = isDownloaded;
        obj["is_scaled"] = isScaled;
        obj["is_uploaded"] = isUploaded;

        if (type == MediaType::Video) {
            obj["preview_video_url"] = previewVideoUrl.toString();

            // Save media files for videos
            QJsonArray filesArray;
            for (const auto& mf : mediaFiles) {
                QJsonObject mfObj;
                mfObj["width"] = mf.width;
                mfObj["height"] = mf.height;
                mfObj["quality"] = mf.quality;
                mfObj["link"] = mf.link.toString();
                filesArray.append(mfObj);
            }
            obj["media_files"] = filesArray;
        } else {
            obj["original_image_url"] = originalImageUrl.toString();
            obj["large_image_url"] = largeImageUrl.toString();
        }

        return obj;
    }

    static MediaMetadata fromJson(const QJsonObject& json) {
        MediaMetadata m;

        // Determine type (default to video for backward compatibility)
        QString typeStr = json["type"].toString("video");
        m.type = (typeStr == "image") ? MediaType::Image : MediaType::Video;

        m.id = json["id"].toInt();
        m.duration = json["duration"].toInt();
        m.width = json["width"].toInt();
        m.height = json["height"].toInt();
        m.author = json["author"].toString();
        m.authorUrl = QUrl(json["author_url"].toString());
        m.sourceUrl = QUrl(json["source_url"].toString());
        m.thumbnailUrl = QUrl(json["thumbnail_url"].toString());
        m.localRawPath = json["local_raw_path"].toString();
        m.localScaledPath = json["local_scaled_path"].toString();
        m.isRejected = json["is_rejected"].toBool();
        m.isDownloaded = json["is_downloaded"].toBool();
        m.isScaled = json["is_scaled"].toBool();
        m.isUploaded = json["is_uploaded"].toBool();

        if (m.type == MediaType::Video) {
            m.previewVideoUrl = QUrl(json["preview_video_url"].toString());

            // Load media files (check both old and new key names for compatibility)
            QJsonArray filesArray = json["media_files"].toArray();
            if (filesArray.isEmpty()) {
                filesArray = json["video_files"].toArray();
            }
            for (const auto& f : filesArray) {
                m.mediaFiles.append(MediaFile::fromJson(f.toObject()));
            }
        } else {
            m.originalImageUrl = QUrl(json["original_image_url"].toString());
            m.largeImageUrl = QUrl(json["large_image_url"].toString());
        }

        return m;
    }
};
