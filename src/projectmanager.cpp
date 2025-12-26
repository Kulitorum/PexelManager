#include "projectmanager.h"
#include "settings.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

QString Project::rawDir() const
{
    return path + "/raw";
}

QString Project::scaledDir() const
{
    return path + "/scaled";
}

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent)
{
}

bool ProjectManager::createProject(const QString& name, const QString& categoryId)
{
    QString projectsDir = Settings::instance().projectsDir();
    QString projectPath = projectsDir + "/" + name;

    QDir dir(projectPath);
    if (dir.exists()) {
        return false;
    }

    if (!dir.mkpath(".")) {
        return false;
    }

    // Create subdirectories
    dir.mkpath("raw");
    dir.mkpath("scaled");

    m_project = Project();
    m_project.name = name;
    m_project.path = projectPath;
    m_project.s3Bucket = Settings::instance().s3Bucket();
    m_project.categoryId = categoryId;

    saveProject();

    Settings::instance().setLastProjectPath(projectPath);
    emit projectLoaded();

    return true;
}

bool ProjectManager::loadProject(const QString& path)
{
    QString projectFile = path + "/project.json";
    QFile file(projectFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();

    m_project = Project();
    m_project.name = root["name"].toString();
    m_project.path = path;
    m_project.searchQuery = root["search_query"].toString();
    m_project.minDuration = root["min_duration"].toInt(30);

    // Check format version (v2 = new format, v1/missing = old format)
    int version = root["version"].toInt(1);

    if (version >= 2) {
        // New format
        m_project.s3Bucket = root["s3_bucket"].toString();
        if (m_project.s3Bucket.isEmpty()) {
            m_project.s3Bucket = Settings::instance().s3Bucket();
        }
        m_project.categoryId = root["category_id"].toString();

        // Load media
        QJsonArray mediaArray = root["media"].toArray();
        for (const auto& m : mediaArray) {
            auto item = MediaMetadata::fromJson(m.toObject());
            m_project.media.append(item);
        }
    } else {
        // Old format - migrate
        QString oldBucket = root["s3_bucket"].toString();

        // Extract categoryId from old bucket name (e.g., "decent-de1-espresso" -> "espresso")
        if (oldBucket.startsWith("decent-de1-")) {
            m_project.categoryId = oldBucket.mid(11);
        } else {
            m_project.categoryId = oldBucket;
        }

        // Use new single bucket
        m_project.s3Bucket = Settings::instance().s3Bucket();

        // Load videos array (old format) and set type to Video
        QJsonArray videosArray = root["videos"].toArray();
        for (const auto& v : videosArray) {
            auto item = MediaMetadata::fromJson(v.toObject());
            item.type = MediaType::Video;  // Old projects only had videos
            m_project.media.append(item);
        }
    }

    // Load rejected IDs
    QJsonArray rejectedArray = root["rejected_ids"].toArray();
    for (const auto& id : rejectedArray) {
        m_project.rejectedIds.insert(id.toInt());
    }

    // Apply rejection status to media items
    for (auto& item : m_project.media) {
        item.isRejected = m_project.rejectedIds.contains(item.id);
    }

    // If migrated from old format, save in new format
    if (version < 2) {
        saveProject();
    }

    Settings::instance().setLastProjectPath(path);
    emit projectLoaded();

    return true;
}

bool ProjectManager::saveProject()
{
    if (m_project.path.isEmpty()) {
        return false;
    }

    QJsonObject root;
    root["version"] = 2;
    root["name"] = m_project.name;
    root["s3_bucket"] = m_project.s3Bucket;
    root["category_id"] = m_project.categoryId;
    root["search_query"] = m_project.searchQuery;
    root["min_duration"] = m_project.minDuration;

    // Save rejected IDs
    QJsonArray rejectedArray;
    for (int id : m_project.rejectedIds) {
        rejectedArray.append(id);
    }
    root["rejected_ids"] = rejectedArray;

    // Save media
    QJsonArray mediaArray;
    for (const auto& item : m_project.media) {
        mediaArray.append(item.toJson());
    }
    root["media"] = mediaArray;

    QString projectFile = m_project.path + "/project.json";
    QFile file(projectFile);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    emit projectSaved();
    return true;
}

void ProjectManager::closeProject()
{
    if (hasProject()) {
        saveProject();
    }
    m_project = Project();
    emit projectClosed();
}

bool ProjectManager::deleteProject(const QString& path)
{
    // Close project if it's the current one
    if (m_project.path == path) {
        m_project = Project();
        emit projectClosed();
    }

    // Clear last project path if it matches
    if (Settings::instance().lastProjectPath() == path) {
        Settings::instance().setLastProjectPath("");
    }

    // Recursively delete project directory
    QDir dir(path);
    return dir.removeRecursively();
}

void ProjectManager::addMedia(const QList<MediaMetadata>& items)
{
    for (auto item : items) {
        // Skip if already exists
        bool exists = false;
        for (const auto& m : m_project.media) {
            if (m.id == item.id) {
                exists = true;
                break;
            }
        }
        if (exists) continue;

        // Check if previously rejected
        item.isRejected = m_project.rejectedIds.contains(item.id);
        m_project.media.append(item);
    }

    emit mediaChanged();
}

void ProjectManager::rejectMedia(int id)
{
    m_project.rejectedIds.insert(id);

    for (auto& item : m_project.media) {
        if (item.id == id) {
            item.isRejected = true;
            break;
        }
    }

    emit mediaChanged();
}

void ProjectManager::updateMedia(const MediaMetadata& item)
{
    for (int i = 0; i < m_project.media.size(); ++i) {
        if (m_project.media[i].id == item.id) {
            m_project.media[i] = item;
            break;
        }
    }

    emit mediaChanged();
}

QStringList ProjectManager::availableProjects()
{
    QStringList projects;
    QString projectsDir = Settings::instance().projectsDir();

    QDir dir(projectsDir);
    if (!dir.exists()) {
        return projects;
    }

    for (const auto& entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString projectFile = entry.absoluteFilePath() + "/project.json";
        if (QFile::exists(projectFile)) {
            projects.append(entry.absoluteFilePath());
        }
    }

    return projects;
}
