#include "uploadmanager.h"
#include "settings.h"
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

UploadManager::UploadManager(QObject* parent)
    : QObject(parent)
{
}

void UploadManager::scaleVideo(int videoId, const QString& inputPath, const QString& outputPath,
                                int targetWidth, int targetHeight, int crf, const QString& preset)
{
    Task task;
    task.type = Scale;
    task.videoId = videoId;
    task.inputPath = inputPath;
    task.outputPath = outputPath;
    task.targetWidth = targetWidth;
    task.targetHeight = targetHeight;
    task.crf = crf;
    task.preset = preset;

    m_scaleQueue.enqueue(task);
    startScaleTasks();
}

void UploadManager::uploadToS3(int videoId, const QString& localPath, const QString& bucket, const QString& key)
{
    Task task;
    task.type = Upload;
    task.videoId = videoId;
    task.inputPath = localPath;
    task.bucket = bucket;
    task.key = key;

    m_uploadQueue.enqueue(task);
    startUploadTasks();
}

void UploadManager::uploadIndexJson(const QString& bucket, const QString& projectName)
{
    // Create index.json content
    QJsonObject root;
    root["updated_utc"] = QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH.mm.ssZ");

    QJsonArray prefixes;

    QJsonObject scaledPrefix;
    scaledPrefix["prefix"] = QString("videos/");
    scaledPrefix["name"] = projectName;
    scaledPrefix["description"] = "1280x800 cropped, production-ready videos";
    scaledPrefix["catalog"] = QString("videos/catalog.json");
    prefixes.append(scaledPrefix);

    root["prefixes"] = prefixes;

    // Write to temp file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_tempIndexPath = tempDir + "/index.json";

    QFile file(m_tempIndexPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        // Queue upload task
        Task task;
        task.type = IndexUpload;
        task.videoId = -1;
        task.inputPath = m_tempIndexPath;
        task.bucket = bucket;
        task.key = "index.json";

        m_uploadQueue.enqueue(task);
        startUploadTasks();
    } else {
        emit indexUploadError("Failed to create temp index.json file");
    }
}

void UploadManager::cancelAll()
{
    m_scaleQueue.clear();
    m_uploadQueue.clear();

    // Kill all running scale processes
    for (auto process : m_runningScales.keys()) {
        process->kill();
        process->deleteLater();
    }
    m_runningScales.clear();

    // Kill all running upload processes
    for (auto process : m_runningUploads.keys()) {
        process->kill();
        process->deleteLater();
    }
    m_runningUploads.clear();

    // Clean up temp file
    if (!m_tempIndexPath.isEmpty()) {
        QFile::remove(m_tempIndexPath);
        m_tempIndexPath.clear();
    }
}

void UploadManager::startScaleTasks()
{
    while (m_runningScales.size() < MAX_CONCURRENT_SCALES && !m_scaleQueue.isEmpty()) {
        Task task = m_scaleQueue.dequeue();

        auto process = new QProcess(this);

        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &UploadManager::onScaleProcessFinished);
        connect(process, &QProcess::errorOccurred,
                this, &UploadManager::onScaleProcessError);

        m_runningScales[process] = task;

        emit scaleStarted(task.videoId);

        // Build ffmpeg command
        QString vf = QString("scale=%1:%2:force_original_aspect_ratio=increase,crop=%1:%2")
            .arg(task.targetWidth)
            .arg(task.targetHeight);

        QStringList args;
        args << "-y"
             << "-i" << task.inputPath
             << "-an"
             << "-vf" << vf
             << "-c:v" << "libx264"
             << "-preset" << task.preset
             << "-crf" << QString::number(task.crf)
             << "-pix_fmt" << "yuv420p"
             << "-movflags" << "+faststart"
             << task.outputPath;

        process->start("ffmpeg", args);
    }
}

void UploadManager::startUploadTasks()
{
    while (m_runningUploads.size() < MAX_CONCURRENT_UPLOADS && !m_uploadQueue.isEmpty()) {
        Task task = m_uploadQueue.dequeue();

        auto process = new QProcess(this);

        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &UploadManager::onUploadProcessFinished);
        connect(process, &QProcess::errorOccurred,
                this, &UploadManager::onUploadProcessError);

        m_runningUploads[process] = task;

        if (task.type == Upload) {
            emit uploadStarted(task.videoId);
        }

        QString s3Path = QString("s3://%1/%2").arg(task.bucket, task.key);

        QStringList args;
        args << "s3" << "cp"
             << task.inputPath
             << s3Path;

        QString profile = Settings::instance().awsProfile();
        if (!profile.isEmpty() && profile != "default") {
            args << "--profile" << profile;
        }

        process->start("aws", args);
    }
}

void UploadManager::onScaleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    auto process = qobject_cast<QProcess*>(sender());
    if (!process || !m_runningScales.contains(process)) return;

    Task task = m_runningScales.take(process);

    QString errorOutput = process->readAllStandardError();
    process->deleteLater();

    if (status != QProcess::NormalExit || exitCode != 0) {
        QString error = errorOutput.isEmpty() ? QString("Exit code: %1").arg(exitCode) : errorOutput;
        emit scaleError(task.videoId, error);
    } else {
        emit scaleCompleted(task.videoId, task.outputPath);
    }

    // Start more tasks if available
    startScaleTasks();

    // Check if all done
    if (m_runningScales.isEmpty() && m_scaleQueue.isEmpty() &&
        m_runningUploads.isEmpty() && m_uploadQueue.isEmpty()) {
        emit allTasksCompleted();
    }
}

void UploadManager::onScaleProcessError(QProcess::ProcessError error)
{
    auto process = qobject_cast<QProcess*>(sender());
    if (!process || !m_runningScales.contains(process)) return;

    Task task = m_runningScales.take(process);
    process->deleteLater();

    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "ffmpeg not found. Please install ffmpeg.";
            break;
        case QProcess::Crashed:
            errorMsg = "ffmpeg crashed";
            break;
        default:
            errorMsg = "Unknown process error";
            break;
    }

    emit scaleError(task.videoId, errorMsg);

    startScaleTasks();

    if (m_runningScales.isEmpty() && m_scaleQueue.isEmpty() &&
        m_runningUploads.isEmpty() && m_uploadQueue.isEmpty()) {
        emit allTasksCompleted();
    }
}

void UploadManager::onUploadProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    auto process = qobject_cast<QProcess*>(sender());
    if (!process || !m_runningUploads.contains(process)) return;

    Task task = m_runningUploads.take(process);

    QString errorOutput = process->readAllStandardError();
    process->deleteLater();

    if (status != QProcess::NormalExit || exitCode != 0) {
        QString error = errorOutput.isEmpty() ? QString("Exit code: %1").arg(exitCode) : errorOutput;

        if (task.type == Upload) {
            emit uploadError(task.videoId, error);
        } else {
            emit indexUploadError(error);
        }
    } else {
        if (task.type == Upload) {
            emit uploadCompleted(task.videoId);
        } else {
            // Clean up temp file after successful index upload
            if (!m_tempIndexPath.isEmpty()) {
                QFile::remove(m_tempIndexPath);
                m_tempIndexPath.clear();
            }
            emit indexUploadCompleted();
        }
    }

    // Start more tasks if available
    startUploadTasks();

    // Check if all done
    if (m_runningScales.isEmpty() && m_scaleQueue.isEmpty() &&
        m_runningUploads.isEmpty() && m_uploadQueue.isEmpty()) {
        emit allTasksCompleted();
    }
}

void UploadManager::onUploadProcessError(QProcess::ProcessError error)
{
    auto process = qobject_cast<QProcess*>(sender());
    if (!process || !m_runningUploads.contains(process)) return;

    Task task = m_runningUploads.take(process);
    process->deleteLater();

    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "aws cli not found. Please install AWS CLI.";
            break;
        case QProcess::Crashed:
            errorMsg = "aws cli crashed";
            break;
        default:
            errorMsg = "Unknown process error";
            break;
    }

    if (task.type == Upload) {
        emit uploadError(task.videoId, errorMsg);
    } else {
        emit indexUploadError(errorMsg);
    }

    startUploadTasks();

    if (m_runningScales.isEmpty() && m_scaleQueue.isEmpty() &&
        m_runningUploads.isEmpty() && m_uploadQueue.isEmpty()) {
        emit allTasksCompleted();
    }
}
