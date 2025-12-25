#pragma once

#include <QObject>
#include <QString>
#include <QSettings>

class Settings : public QObject
{
    Q_OBJECT

public:
    static Settings& instance();

    // API settings
    QString pexelsApiKey() const;
    void setPexelsApiKey(const QString& key);

    // AWS settings
    QString awsProfile() const;
    void setAwsProfile(const QString& profile);

    QString s3Bucket() const;
    void setS3Bucket(const QString& bucket);

    QString categoriesBucket() const;
    void setCategoriesBucket(const QString& bucket);

    // Video settings
    int maxDownloadWidth() const;
    void setMaxDownloadWidth(int width);

    int targetWidth() const;
    void setTargetWidth(int width);

    int targetHeight() const;
    void setTargetHeight(int height);

    int ffmpegCrf() const;
    void setFfmpegCrf(int crf);

    QString ffmpegPreset() const;
    void setFfmpegPreset(const QString& preset);

    // Paths
    QString projectsDir() const;
    QString lastProjectPath() const;
    void setLastProjectPath(const QString& path);

    // Window state
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

    QByteArray splitterState() const;
    void setSplitterState(const QByteArray& state);

signals:
    void settingsChanged();

private:
    Settings();
    ~Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    QSettings m_settings;
};
