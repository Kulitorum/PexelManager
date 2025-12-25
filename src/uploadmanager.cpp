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

    if (!m_currentProcess) {
        startNextTask();
    }
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

    if (!m_currentProcess) {
        startNextTask();
    }
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

        if (!m_currentProcess) {
            startNextTask();
        }
    } else {
        emit indexUploadError("Failed to create temp index.json file");
    }
}

void UploadManager::cancelAll()
{
    m_scaleQueue.clear();
    m_uploadQueue.clear();

    if (m_currentProcess) {
        m_currentProcess->kill();
        m_currentProcess->deleteLater();
        m_currentProcess = nullptr;
    }

    // Clean up temp file
    if (!m_tempIndexPath.isEmpty()) {
        QFile::remove(m_tempIndexPath);
        m_tempIndexPath.clear();
    }
}

void UploadManager::startNextTask()
{
    // Prioritize scaling over uploading
    if (!m_scaleQueue.isEmpty()) {
        m_currentTask = m_scaleQueue.dequeue();
    } else if (!m_uploadQueue.isEmpty()) {
        m_currentTask = m_uploadQueue.dequeue();
    } else {
        emit allTasksCompleted();
        return;
    }

    m_currentProcess = new QProcess(this);

    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &UploadManager::onProcessFinished);
    connect(m_currentProcess, &QProcess::errorOccurred,
            this, &UploadManager::onProcessError);

    if (m_currentTask.type == Scale) {
        emit scaleStarted(m_currentTask.videoId);

        // Build ffmpeg command
        // scale=W:H:force_original_aspect_ratio=increase,crop=W:H
        QString vf = QString("scale=%1:%2:force_original_aspect_ratio=increase,crop=%1:%2")
            .arg(m_currentTask.targetWidth)
            .arg(m_currentTask.targetHeight);

        QStringList args;
        args << "-y"
             << "-i" << m_currentTask.inputPath
             << "-an"
             << "-vf" << vf
             << "-c:v" << "libx264"
             << "-preset" << m_currentTask.preset
             << "-crf" << QString::number(m_currentTask.crf)
             << "-pix_fmt" << "yuv420p"
             << "-movflags" << "+faststart"
             << m_currentTask.outputPath;

        m_currentProcess->start("ffmpeg", args);

    } else {  // Upload or IndexUpload
        if (m_currentTask.type == Upload) {
            emit uploadStarted(m_currentTask.videoId);
        }

        QString s3Path = QString("s3://%1/%2").arg(m_currentTask.bucket, m_currentTask.key);

        QStringList args;
        args << "s3" << "cp"
             << m_currentTask.inputPath
             << s3Path;

        QString profile = Settings::instance().awsProfile();
        if (!profile.isEmpty() && profile != "default") {
            args << "--profile" << profile;
        }

        m_currentProcess->start("aws", args);
    }
}

void UploadManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_currentProcess) return;

    QString output = m_currentProcess->readAllStandardOutput();
    QString errorOutput = m_currentProcess->readAllStandardError();

    m_currentProcess->deleteLater();
    m_currentProcess = nullptr;

    if (status != QProcess::NormalExit || exitCode != 0) {
        QString error = errorOutput.isEmpty() ? QString("Exit code: %1").arg(exitCode) : errorOutput;

        if (m_currentTask.type == Scale) {
            emit scaleError(m_currentTask.videoId, error);
        } else if (m_currentTask.type == Upload) {
            emit uploadError(m_currentTask.videoId, error);
        } else {
            emit indexUploadError(error);
        }
    } else {
        if (m_currentTask.type == Scale) {
            emit scaleCompleted(m_currentTask.videoId, m_currentTask.outputPath);
        } else if (m_currentTask.type == Upload) {
            emit uploadCompleted(m_currentTask.videoId);
        } else {
            // Clean up temp file after successful upload
            if (!m_tempIndexPath.isEmpty()) {
                QFile::remove(m_tempIndexPath);
                m_tempIndexPath.clear();
            }
            emit indexUploadCompleted();
        }
    }

    startNextTask();
}

void UploadManager::onProcessError(QProcess::ProcessError error)
{
    if (!m_currentProcess) return;

    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = m_currentTask.type == Scale
                ? "ffmpeg not found. Please install ffmpeg."
                : "aws cli not found. Please install AWS CLI.";
            break;
        case QProcess::Crashed:
            errorMsg = "Process crashed";
            break;
        default:
            errorMsg = "Unknown process error";
            break;
    }

    m_currentProcess->deleteLater();
    m_currentProcess = nullptr;

    if (m_currentTask.type == Scale) {
        emit scaleError(m_currentTask.videoId, errorMsg);
    } else if (m_currentTask.type == Upload) {
        emit uploadError(m_currentTask.videoId, errorMsg);
    } else {
        emit indexUploadError(errorMsg);
    }

    startNextTask();
}
