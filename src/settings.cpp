#include "settings.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>

Settings& Settings::instance()
{
    static Settings instance;
    return instance;
}

Settings::Settings()
    : m_settings(QSettings::IniFormat, QSettings::UserScope, "PexelManager", "PexelManager")
{
    // Ensure projects directory exists
    QDir dir(projectsDir());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QString Settings::pexelsApiKey() const
{
    // Check settings first, then fall back to environment variable
    QString key = m_settings.value("api/pexels_key").toString();
    if (key.isEmpty()) {
        key = qEnvironmentVariable("PEXELS_API_KEY");
    }
    return key;
}

void Settings::setPexelsApiKey(const QString& key)
{
    m_settings.setValue("api/pexels_key", key);
    emit settingsChanged();
}

QString Settings::awsProfile() const
{
    return m_settings.value("aws/profile", "default").toString();
}

void Settings::setAwsProfile(const QString& profile)
{
    m_settings.setValue("aws/profile", profile);
    emit settingsChanged();
}

QString Settings::s3Bucket() const
{
    return m_settings.value("aws/bucket", "decent-de1-media").toString();
}

void Settings::setS3Bucket(const QString& bucket)
{
    m_settings.setValue("aws/bucket", bucket);
    emit settingsChanged();
}

int Settings::maxDownloadWidth() const
{
    return m_settings.value("media/max_download_width", 1920).toInt();
}

void Settings::setMaxDownloadWidth(int width)
{
    m_settings.setValue("media/max_download_width", width);
    emit settingsChanged();
}

int Settings::targetWidth() const
{
    return m_settings.value("media/target_width", 1280).toInt();
}

void Settings::setTargetWidth(int width)
{
    m_settings.setValue("media/target_width", width);
    emit settingsChanged();
}

int Settings::targetHeight() const
{
    return m_settings.value("media/target_height", 800).toInt();
}

void Settings::setTargetHeight(int height)
{
    m_settings.setValue("media/target_height", height);
    emit settingsChanged();
}

int Settings::ffmpegCrf() const
{
    return m_settings.value("media/crf", 22).toInt();
}

void Settings::setFfmpegCrf(int crf)
{
    m_settings.setValue("media/crf", crf);
    emit settingsChanged();
}

QString Settings::ffmpegPreset() const
{
    return m_settings.value("media/preset", "slow").toString();
}

void Settings::setFfmpegPreset(const QString& preset)
{
    m_settings.setValue("media/preset", preset);
    emit settingsChanged();
}

QString Settings::projectsDir() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataPath + "/projects";
}

QString Settings::lastProjectPath() const
{
    return m_settings.value("app/last_project").toString();
}

void Settings::setLastProjectPath(const QString& path)
{
    m_settings.setValue("app/last_project", path);
}

QByteArray Settings::windowGeometry() const
{
    return m_settings.value("window/geometry").toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& geometry)
{
    m_settings.setValue("window/geometry", geometry);
}

QByteArray Settings::splitterState() const
{
    return m_settings.value("window/splitter").toByteArray();
}

void Settings::setSplitterState(const QByteArray& state)
{
    m_settings.setValue("window/splitter", state);
}
