#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include "videometadata.h"

struct Project {
    QString name;
    QString path;
    QString s3Bucket;
    QString searchQuery;
    int minDuration = 30;
    QList<VideoMetadata> videos;
    QSet<int> rejectedIds;

    QString rawDir() const;
    QString scaledDir() const;
};

class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);

    bool createProject(const QString& name, const QString& s3Bucket);
    bool loadProject(const QString& path);
    bool saveProject();
    void closeProject();
    bool deleteProject(const QString& path);

    bool hasProject() const { return !m_project.path.isEmpty(); }
    Project& project() { return m_project; }
    const Project& project() const { return m_project; }

    void addVideos(const QList<VideoMetadata>& videos);
    void rejectVideo(int id);
    void updateVideo(const VideoMetadata& video);

    static QStringList availableProjects();

signals:
    void projectLoaded();
    void projectSaved();
    void projectClosed();
    void videosChanged();

private:
    Project m_project;
};
