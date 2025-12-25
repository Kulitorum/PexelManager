#pragma once

#include <QString>
#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>

struct VideoFile {
    int width = 0;
    int height = 0;
    QString quality;
    QUrl link;

    static VideoFile fromJson(const QJsonObject& json) {
        VideoFile vf;
        vf.width = json["width"].toInt();
        vf.height = json["height"].toInt();
        vf.quality = json["quality"].toString();
        vf.link = QUrl(json["link"].toString());
        return vf;
    }
};

struct VideoMetadata {
    int id = 0;
    int duration = 0;
    int width = 0;
    int height = 0;
    QString author;
    QUrl authorUrl;
    QUrl sourceUrl;
    QUrl thumbnailUrl;
    QUrl previewVideoUrl;
    QList<VideoFile> videoFiles;

    // Local state
    QString localRawPath;
    QString localScaledPath;
    bool isRejected = false;
    bool isDownloaded = false;
    bool isScaled = false;
    bool isUploaded = false;

    static VideoMetadata fromPexelsJson(const QJsonObject& json) {
        VideoMetadata vm;
        vm.id = json["id"].toInt();
        vm.duration = json["duration"].toInt();
        vm.width = json["width"].toInt();
        vm.height = json["height"].toInt();

        auto user = json["user"].toObject();
        vm.author = user["name"].toString();
        vm.authorUrl = QUrl(user["url"].toString());
        vm.sourceUrl = QUrl(json["url"].toString());

        // Get thumbnail from image field
        vm.thumbnailUrl = QUrl(json["image"].toString());

        // Parse video files
        auto files = json["video_files"].toArray();
        for (const auto& f : files) {
            auto vf = VideoFile::fromJson(f.toObject());
            if (!vf.link.isEmpty()) {
                vm.videoFiles.append(vf);
            }
        }

        // Find a good preview video (smaller resolution)
        for (const auto& vf : vm.videoFiles) {
            if (vf.quality == "sd" || vf.width <= 640) {
                vm.previewVideoUrl = vf.link;
                break;
            }
        }
        if (vm.previewVideoUrl.isEmpty() && !vm.videoFiles.isEmpty()) {
            vm.previewVideoUrl = vm.videoFiles.first().link;
        }

        return vm;
    }

    // Get best video file for download based on max resolution preference
    VideoFile getBestVideoFile(int maxWidth = 1920) const {
        VideoFile best;
        int bestArea = 0;

        for (const auto& vf : videoFiles) {
            if (vf.width <= maxWidth) {
                int area = vf.width * vf.height;
                if (area > bestArea) {
                    bestArea = area;
                    best = vf;
                }
            }
        }

        // Fallback to smallest if nothing fits
        if (best.link.isEmpty() && !videoFiles.isEmpty()) {
            best = videoFiles.first();
            for (const auto& vf : videoFiles) {
                if (vf.width * vf.height < best.width * best.height) {
                    best = vf;
                }
            }
        }

        return best;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["duration"] = duration;
        obj["width"] = width;
        obj["height"] = height;
        obj["author"] = author;
        obj["author_url"] = authorUrl.toString();
        obj["source_url"] = sourceUrl.toString();
        obj["thumbnail_url"] = thumbnailUrl.toString();
        obj["preview_video_url"] = previewVideoUrl.toString();
        obj["local_raw_path"] = localRawPath;
        obj["local_scaled_path"] = localScaledPath;
        obj["is_rejected"] = isRejected;
        obj["is_downloaded"] = isDownloaded;
        obj["is_scaled"] = isScaled;
        obj["is_uploaded"] = isUploaded;

        // Save video files for future use
        QJsonArray filesArray;
        for (const auto& vf : videoFiles) {
            QJsonObject vfObj;
            vfObj["width"] = vf.width;
            vfObj["height"] = vf.height;
            vfObj["quality"] = vf.quality;
            vfObj["link"] = vf.link.toString();
            filesArray.append(vfObj);
        }
        obj["video_files"] = filesArray;

        return obj;
    }

    static VideoMetadata fromJson(const QJsonObject& json) {
        VideoMetadata vm;
        vm.id = json["id"].toInt();
        vm.duration = json["duration"].toInt();
        vm.width = json["width"].toInt();
        vm.height = json["height"].toInt();
        vm.author = json["author"].toString();
        vm.authorUrl = QUrl(json["author_url"].toString());
        vm.sourceUrl = QUrl(json["source_url"].toString());
        vm.thumbnailUrl = QUrl(json["thumbnail_url"].toString());
        vm.previewVideoUrl = QUrl(json["preview_video_url"].toString());
        vm.localRawPath = json["local_raw_path"].toString();
        vm.localScaledPath = json["local_scaled_path"].toString();
        vm.isRejected = json["is_rejected"].toBool();
        vm.isDownloaded = json["is_downloaded"].toBool();
        vm.isScaled = json["is_scaled"].toBool();
        vm.isUploaded = json["is_uploaded"].toBool();

        // Load video files
        QJsonArray filesArray = json["video_files"].toArray();
        for (const auto& f : filesArray) {
            vm.videoFiles.append(VideoFile::fromJson(f.toObject()));
        }

        return vm;
    }
};
