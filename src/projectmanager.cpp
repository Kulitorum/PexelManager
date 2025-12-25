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

bool ProjectManager::createProject(const QString& name, const QString& s3Bucket)
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
    m_project.s3Bucket = s3Bucket;

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
    m_project.s3Bucket = root["s3_bucket"].toString();
    m_project.searchQuery = root["search_query"].toString();
    m_project.minDuration = root["min_duration"].toInt(30);

    // Load rejected IDs
    QJsonArray rejectedArray = root["rejected_ids"].toArray();
    for (const auto& id : rejectedArray) {
        m_project.rejectedIds.insert(id.toInt());
    }

    // Load videos
    QJsonArray videosArray = root["videos"].toArray();
    for (const auto& v : videosArray) {
        auto video = VideoMetadata::fromJson(v.toObject());
        video.isRejected = m_project.rejectedIds.contains(video.id);
        m_project.videos.append(video);
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
    root["name"] = m_project.name;
    root["s3_bucket"] = m_project.s3Bucket;
    root["search_query"] = m_project.searchQuery;
    root["min_duration"] = m_project.minDuration;

    // Save rejected IDs
    QJsonArray rejectedArray;
    for (int id : m_project.rejectedIds) {
        rejectedArray.append(id);
    }
    root["rejected_ids"] = rejectedArray;

    // Save videos
    QJsonArray videosArray;
    for (const auto& video : m_project.videos) {
        videosArray.append(video.toJson());
    }
    root["videos"] = videosArray;

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

void ProjectManager::addVideos(const QList<VideoMetadata>& videos)
{
    for (auto video : videos) {
        // Skip if already exists
        bool exists = false;
        for (const auto& v : m_project.videos) {
            if (v.id == video.id) {
                exists = true;
                break;
            }
        }
        if (exists) continue;

        // Check if previously rejected
        video.isRejected = m_project.rejectedIds.contains(video.id);
        m_project.videos.append(video);
    }

    emit videosChanged();
}

void ProjectManager::rejectVideo(int id)
{
    m_project.rejectedIds.insert(id);

    for (auto& video : m_project.videos) {
        if (video.id == id) {
            video.isRejected = true;
            break;
        }
    }

    emit videosChanged();
}

void ProjectManager::updateVideo(const VideoMetadata& video)
{
    for (int i = 0; i < m_project.videos.size(); ++i) {
        if (m_project.videos[i].id == video.id) {
            m_project.videos[i] = video;
            break;
        }
    }

    emit videosChanged();
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
