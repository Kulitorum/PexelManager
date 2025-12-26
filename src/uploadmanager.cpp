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

void UploadManager::scaleMedia(int mediaId, MediaType type, const QString& inputPath, const QString& outputPath,
                                int targetWidth, int targetHeight, int crf, const QString& preset)
{
    Task task;
    task.type = Scale;
    task.mediaType = type;
    task.mediaId = mediaId;
    task.inputPath = inputPath;
    task.outputPath = outputPath;
    task.targetWidth = targetWidth;
    task.targetHeight = targetHeight;
    task.crf = crf;
    task.preset = preset;

    m_scaleQueue.enqueue(task);
    startScaleTasks();
}

void UploadManager::uploadToS3(int mediaId, const QString& localPath, const QString& bucket, const QString& key)
{
    Task task;
    task.type = Upload;
    task.mediaId = mediaId;
    task.inputPath = localPath;
    task.bucket = bucket;
    task.key = key;

    m_uploadQueue.enqueue(task);
    startUploadTasks();
}

void UploadManager::uploadCategoriesJson(const QString& bucket, const QString& categoryId, const QString& projectName)
{
    // Read existing categories or create new array
    QString categoriesPath = Settings::instance().projectsDir() + "/../categories.json";
    QJsonArray categories;

    QFile existingFile(categoriesPath);
    if (existingFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(existingFile.readAll());
        existingFile.close();
        if (doc.isArray()) {
            categories = doc.array();
        }
    }

    // Check if this category already exists
    bool found = false;
    for (int i = 0; i < categories.size(); ++i) {
        QJsonObject cat = categories[i].toObject();
        if (cat["id"].toString() == categoryId) {
            // Update existing entry (only keep id and name)
            QJsonObject updated;
            updated["id"] = categoryId;
            updated["name"] = projectName;
            categories[i] = updated;
            found = true;
            break;
        }
    }

    // Add new entry if not found
    if (!found) {
        QJsonObject newCat;
        newCat["id"] = categoryId;
        newCat["name"] = projectName;
        categories.append(newCat);
    }

    // Write to local file
    if (existingFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(categories);
        existingFile.write(doc.toJson(QJsonDocument::Indented));
        existingFile.close();
    }

    // Write to temp file for upload
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_tempCategoriesPath = tempDir + "/categories.json";

    QFile tempFile(m_tempCategoriesPath);
    if (tempFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(categories);
        tempFile.write(doc.toJson(QJsonDocument::Indented));
        tempFile.close();

        // Queue upload task to same bucket (categories.json at root)
        Task task;
        task.type = CategoriesUpload;
        task.mediaId = -1;
        task.inputPath = m_tempCategoriesPath;
        task.bucket = bucket;
        task.key = "categories.json";

        m_uploadQueue.enqueue(task);
        startUploadTasks();
    } else {
        emit categoriesUploadError("Failed to create temp categories.json file");
    }
}

void UploadManager::uploadIndexJson(const QString& bucket, const QString& categoryId, const QString& projectName)
{
    // Create index.json content
    QJsonObject root;
    root["updated_utc"] = QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH.mm.ssZ");

    QJsonArray prefixes;

    QJsonObject mediaPrefix;
    mediaPrefix["prefix"] = QString("media/");
    mediaPrefix["name"] = projectName;
    mediaPrefix["description"] = "1280x800 cropped, production-ready media";
    mediaPrefix["catalog"] = QString("catalogs/%1.json").arg(categoryId);
    prefixes.append(mediaPrefix);

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
        task.mediaId = -1;
        task.inputPath = m_tempIndexPath;
        task.bucket = bucket;
        task.key = "index.json";

        m_uploadQueue.enqueue(task);
        startUploadTasks();
    } else {
        emit indexUploadError("Failed to create temp index.json file");
    }
}

void UploadManager::uploadCatalogJson(const QString& bucket, const QString& categoryId, const QList<MediaMetadata>& media)
{
    // Create catalog.json as simple array of media items with scaled files
    QJsonArray mediaArray;
    for (const auto& item : media) {
        if (item.isRejected) continue;

        QFileInfo fileInfo(item.localScaledPath);
        if (!fileInfo.exists()) continue;

        QJsonObject m;
        m["id"] = item.id;
        m["type"] = item.isVideo() ? "video" : "image";
        m["path"] = fileInfo.fileName();
        m["author"] = item.author;
        m["bytes"] = fileInfo.size();

        // Only include duration for videos
        if (item.isVideo()) {
            m["duration_s"] = item.duration;
        }

        mediaArray.append(m);
    }

    // Write to temp file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_tempCatalogPath = tempDir + "/catalog.json";

    QFile file(m_tempCatalogPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(mediaArray);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        // Queue upload task - new path: catalogs/{categoryId}.json
        Task task;
        task.type = CatalogUpload;
        task.mediaId = -1;
        task.inputPath = m_tempCatalogPath;
        task.bucket = bucket;
        task.key = QString("catalogs/%1.json").arg(categoryId);

        m_uploadQueue.enqueue(task);
        startUploadTasks();
    } else {
        emit indexUploadError("Failed to create temp catalog.json file");
    }
}

void UploadManager::deleteFromS3(const QString& bucket, const QString& categoryId)
{
    // Delete media files for this category
    // Note: In the new structure, media files are shared, so we need to be careful
    // This function now deletes the catalog file for this category
    Task task;
    task.type = S3Delete;
    task.mediaId = -1;
    task.bucket = bucket;
    task.categoryId = categoryId;

    m_uploadQueue.enqueue(task);
    startUploadTasks();
}

void UploadManager::removeCategoryAndUpload(const QString& bucket, const QString& categoryId)
{
    // Read existing categories
    QString categoriesPath = Settings::instance().projectsDir() + "/../categories.json";
    QJsonArray categories;

    QFile existingFile(categoriesPath);
    if (existingFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(existingFile.readAll());
        existingFile.close();
        if (doc.isArray()) {
            categories = doc.array();
        }
    }

    // Remove the category with matching id
    QJsonArray newCategories;
    for (int i = 0; i < categories.size(); ++i) {
        QJsonObject cat = categories[i].toObject();
        if (cat["id"].toString() != categoryId) {
            newCategories.append(cat);
        }
    }

    // Write to local file
    if (existingFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(newCategories);
        existingFile.write(doc.toJson(QJsonDocument::Indented));
        existingFile.close();
    }

    // Write to temp file for upload
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_tempCategoriesPath = tempDir + "/categories.json";

    QFile tempFile(m_tempCategoriesPath);
    if (tempFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(newCategories);
        tempFile.write(doc.toJson(QJsonDocument::Indented));
        tempFile.close();

        // Queue upload task to same bucket
        Task task;
        task.type = CategoriesUpload;
        task.mediaId = -1;
        task.inputPath = m_tempCategoriesPath;
        task.bucket = bucket;
        task.key = "categories.json";

        m_uploadQueue.enqueue(task);
        startUploadTasks();
    } else {
        emit categoriesUploadError("Failed to create temp categories.json file");
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

    // Clean up temp files
    if (!m_tempIndexPath.isEmpty()) {
        QFile::remove(m_tempIndexPath);
        m_tempIndexPath.clear();
    }
    if (!m_tempCatalogPath.isEmpty()) {
        QFile::remove(m_tempCatalogPath);
        m_tempCatalogPath.clear();
    }
    if (!m_tempCategoriesPath.isEmpty()) {
        QFile::remove(m_tempCategoriesPath);
        m_tempCategoriesPath.clear();
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

        emit scaleStarted(task.mediaId);

        QStringList args;

        if (task.mediaType == MediaType::Image) {
            // Image scaling with ffmpeg
            QString vf = QString("scale=%1:%2:force_original_aspect_ratio=increase,crop=%1:%2")
                .arg(task.targetWidth)
                .arg(task.targetHeight);

            args << "-y"
                 << "-i" << task.inputPath
                 << "-vf" << vf
                 << "-q:v" << "2"  // High quality JPEG
                 << task.outputPath;
        } else {
            // Video scaling with ffmpeg
            QString vf = QString("scale=%1:%2:force_original_aspect_ratio=increase,crop=%1:%2")
                .arg(task.targetWidth)
                .arg(task.targetHeight);

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
        }

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
            emit uploadStarted(task.mediaId);
        }

        QStringList args;
        QString profile = Settings::instance().awsProfile();

        if (task.type == S3Delete) {
            // Delete catalog file for this category
            QString s3Path = QString("s3://%1/catalogs/%2.json").arg(task.bucket, task.categoryId);
            args << "s3" << "rm" << s3Path;
        } else {
            // Upload file: aws s3 cp local s3://bucket/key
            QString s3Path = QString("s3://%1/%2").arg(task.bucket, task.key);
            args << "s3" << "cp" << task.inputPath << s3Path;
        }

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
        emit scaleError(task.mediaId, error);
    } else {
        emit scaleCompleted(task.mediaId, task.outputPath);
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

    emit scaleError(task.mediaId, errorMsg);

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
            emit uploadError(task.mediaId, error);
        } else if (task.type == IndexUpload || task.type == CatalogUpload) {
            emit indexUploadError(error);
        } else if (task.type == CategoriesUpload) {
            emit categoriesUploadError(error);
        } else if (task.type == S3Delete) {
            emit s3DeleteError(task.bucket, error);
        }
    } else {
        if (task.type == Upload) {
            emit uploadCompleted(task.mediaId);
        } else if (task.type == IndexUpload) {
            // Clean up temp file after successful index upload
            if (!m_tempIndexPath.isEmpty()) {
                QFile::remove(m_tempIndexPath);
                m_tempIndexPath.clear();
            }
            emit indexUploadCompleted();
        } else if (task.type == CatalogUpload) {
            // Clean up temp file after successful catalog upload
            if (!m_tempCatalogPath.isEmpty()) {
                QFile::remove(m_tempCatalogPath);
                m_tempCatalogPath.clear();
            }
            // No separate signal for catalog - it's part of the index upload flow
        } else if (task.type == CategoriesUpload) {
            // Clean up temp file after successful categories upload
            if (!m_tempCategoriesPath.isEmpty()) {
                QFile::remove(m_tempCategoriesPath);
                m_tempCategoriesPath.clear();
            }
            emit categoriesUploadCompleted();
        } else if (task.type == S3Delete) {
            emit s3DeleteCompleted(task.bucket);
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
        emit uploadError(task.mediaId, errorMsg);
    } else if (task.type == IndexUpload || task.type == CatalogUpload) {
        emit indexUploadError(errorMsg);
    } else if (task.type == CategoriesUpload) {
        emit categoriesUploadError(errorMsg);
    } else if (task.type == S3Delete) {
        emit s3DeleteError(task.bucket, errorMsg);
    }

    startUploadTasks();

    if (m_runningScales.isEmpty() && m_scaleQueue.isEmpty() &&
        m_runningUploads.isEmpty() && m_uploadQueue.isEmpty()) {
        emit allTasksCompleted();
    }
}
