#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include "mediametadata.h"

struct Project {
    QString name;
    QString path;
    QString s3Bucket;
    QString categoryId;
    QString searchQuery;
    int minDuration = 30;
    QList<MediaMetadata> media;
    QSet<int> rejectedIds;

    QString rawDir() const;
    QString scaledDir() const;
};

class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);

    bool createProject(const QString& name, const QString& categoryId);
    bool loadProject(const QString& path);
    bool saveProject();
    void closeProject();
    bool deleteProject(const QString& path);

    bool hasProject() const { return !m_project.path.isEmpty(); }
    Project& project() { return m_project; }
    const Project& project() const { return m_project; }

    void addMedia(const QList<MediaMetadata>& items);
    void rejectMedia(int id);
    void updateMedia(const MediaMetadata& item);

    static QStringList availableProjects();

signals:
    void projectLoaded();
    void projectSaved();
    void projectClosed();
    void mediaChanged();

private:
    Project m_project;
};
