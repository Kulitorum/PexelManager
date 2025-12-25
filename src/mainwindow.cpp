#include "mainwindow.h"
#include "settings.h"

#include <QDebug>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Pexel Manager");
    resize(1400, 900);

    m_api = new PexelsApi(this);
    m_projectManager = new ProjectManager(this);
    m_downloadManager = new DownloadManager(this);
    m_uploadManager = new UploadManager(this);

    setupUi();
    setupMenus();
    restoreState();

    // API connections
    connect(m_api, &PexelsApi::searchCompleted, this, &MainWindow::onSearchCompleted);
    connect(m_api, &PexelsApi::searchError, this, &MainWindow::onSearchError);

    // Video list connections
    connect(m_videoList, &VideoListWidget::videoSelected, this, &MainWindow::onVideoSelected);
    connect(m_videoList, &VideoListWidget::videoRejected, this, &MainWindow::onVideoRejected);

    // Download connections
    connect(m_downloadManager, &DownloadManager::downloadCompleted, this, &MainWindow::onDownloadCompleted);
    connect(m_downloadManager, &DownloadManager::downloadProgress, this, [this](int, qint64 recv, qint64 total) {
        if (total > 0) {
            m_progressBar->setValue(static_cast<int>(recv * 100 / total));
        }
    });
    connect(m_downloadManager, &DownloadManager::downloadError, this, [this](int id, const QString& error) {
        m_statusLabel->setText(QString("Download error for %1: %2").arg(id).arg(error));
    });

    // Scale/Upload connections
    connect(m_uploadManager, &UploadManager::scaleCompleted, this, &MainWindow::onScaleCompleted);
    connect(m_uploadManager, &UploadManager::uploadCompleted, this, &MainWindow::onUploadCompleted);
    connect(m_uploadManager, &UploadManager::scaleError, this, [this](int id, const QString& error) {
        m_statusLabel->setText(QString("Scale error for %1: %2").arg(id).arg(error));
    });
    connect(m_uploadManager, &UploadManager::uploadError, this, [this](int id, const QString& error) {
        m_statusLabel->setText(QString("Upload error for %1: %2").arg(id).arg(error));
    });
    connect(m_uploadManager, &UploadManager::indexUploadCompleted, this, [this]() {
        m_statusLabel->setText("Upload completed (including index.json)");
    });
    connect(m_uploadManager, &UploadManager::indexUploadError, this, [this](const QString& error) {
        m_statusLabel->setText(QString("Index upload error: %1").arg(error));
    });

    // Try to load last project
    QString lastProject = Settings::instance().lastProjectPath();
    if (!lastProject.isEmpty() && QDir(lastProject).exists()) {
        m_projectManager->loadProject(lastProject);
        m_videoList->setProjectVideos(m_projectManager->project().videos);
        m_videoList->setViewMode(VideoListWidget::ProjectVideos);
        m_viewModeLabel->setText("PROJECT VIDEOS");
        m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
        m_toggleViewBtn->setText(QString("Show Search Results (0)"));
        m_loadMoreBtn->setVisible(false);
        m_addToProjectBtn->setVisible(false);
        updateProjectUi();
    }
}

MainWindow::~MainWindow()
{
    saveState();
    m_projectManager->saveProject();
}

void MainWindow::setupUi()
{
    auto centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto mainLayout = new QVBoxLayout(centralWidget);

    // Main splitter
    m_splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_splitter, 1);

    // === Left Panel ===
    m_leftPanel = new QWidget(this);
    auto leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(4, 4, 4, 4);

    // Search bar
    auto searchLayout = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search Pexels videos...");
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearch);
    searchLayout->addWidget(m_searchEdit, 1);

    m_searchBtn = new QPushButton("Search", this);
    connect(m_searchBtn, &QPushButton::clicked, this, &MainWindow::onSearch);
    searchLayout->addWidget(m_searchBtn);

    leftLayout->addLayout(searchLayout);

    // Options row
    auto optionsLayout = new QHBoxLayout;

    optionsLayout->addWidget(new QLabel("Min duration:", this));
    m_minDurationSpin = new QSpinBox(this);
    m_minDurationSpin->setRange(0, 600);
    m_minDurationSpin->setValue(30);
    m_minDurationSpin->setSuffix("s");
    optionsLayout->addWidget(m_minDurationSpin);

    optionsLayout->addWidget(new QLabel("Max res:", this));
    m_resolutionCombo = new QComboBox(this);
    m_resolutionCombo->addItem("1080p", 1920);
    m_resolutionCombo->addItem("1440p", 2560);
    m_resolutionCombo->addItem("4K", 3840);
    optionsLayout->addWidget(m_resolutionCombo);

    optionsLayout->addStretch();

    leftLayout->addLayout(optionsLayout);

    // View mode label
    m_viewModeLabel = new QLabel("PROJECT VIDEOS", this);
    m_viewModeLabel->setAlignment(Qt::AlignCenter);
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    leftLayout->addWidget(m_viewModeLabel);

    // View toggle button
    m_toggleViewBtn = new QPushButton("Show Search Results (0)", this);
    connect(m_toggleViewBtn, &QPushButton::clicked, this, &MainWindow::onToggleView);
    leftLayout->addWidget(m_toggleViewBtn);

    // Video list
    m_videoList = new VideoListWidget(this);
    leftLayout->addWidget(m_videoList, 1);

    // Add to project button (only visible in search results view)
    m_addToProjectBtn = new QPushButton("Add to Project", this);
    connect(m_addToProjectBtn, &QPushButton::clicked, this, &MainWindow::onAddToProject);
    leftLayout->addWidget(m_addToProjectBtn);

    // Load more button
    m_loadMoreBtn = new QPushButton("Load More Results", this);
    m_loadMoreBtn->setEnabled(false);
    connect(m_loadMoreBtn, &QPushButton::clicked, this, &MainWindow::onLoadMore);
    leftLayout->addWidget(m_loadMoreBtn);

    m_splitter->addWidget(m_leftPanel);

    // === Right Panel ===
    m_rightPanel = new QWidget(this);
    auto rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setContentsMargins(4, 4, 4, 4);

    m_player = new VideoPlayerWidget(this);
    rightLayout->addWidget(m_player, 1);

    m_videoInfoLabel = new QLabel("Select a video to preview", this);
    m_videoInfoLabel->setWordWrap(true);
    rightLayout->addWidget(m_videoInfoLabel);

    m_splitter->addWidget(m_rightPanel);

    m_splitter->setSizes({400, 1000});

    // === Bottom Toolbar ===
    auto toolbarLayout = new QHBoxLayout;

    m_downloadBtn = new QPushButton("Download Selected", this);
    connect(m_downloadBtn, &QPushButton::clicked, this, &MainWindow::onDownloadSelected);
    toolbarLayout->addWidget(m_downloadBtn);

    m_scaleBtn = new QPushButton("Scale Downloaded", this);
    connect(m_scaleBtn, &QPushButton::clicked, this, &MainWindow::onScaleSelected);
    toolbarLayout->addWidget(m_scaleBtn);

    m_uploadBtn = new QPushButton("Upload to S3", this);
    connect(m_uploadBtn, &QPushButton::clicked, this, &MainWindow::onUploadSelected);
    toolbarLayout->addWidget(m_uploadBtn);

    toolbarLayout->addStretch();

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedWidth(200);
    m_progressBar->setVisible(false);
    toolbarLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Ready", this);
    toolbarLayout->addWidget(m_statusLabel);

    mainLayout->addLayout(toolbarLayout);
}

void MainWindow::setupMenus()
{
    // File menu
    auto fileMenu = menuBar()->addMenu("&File");

    fileMenu->addAction("&New Project...", this, &MainWindow::onNewProject, QKeySequence::New);
    fileMenu->addAction("&Open Project...", this, &MainWindow::onOpenProject, QKeySequence::Open);
    fileMenu->addAction("&Save Project", this, &MainWindow::onSaveProject, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("Open Project &Directory", this, &MainWindow::onOpenProjectDir);
    fileMenu->addAction("&Reset Project...", this, &MainWindow::onResetProject);
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", this, &MainWindow::onSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QMainWindow::close, QKeySequence::Quit);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveState();
    m_projectManager->saveProject();
    event->accept();
}

void MainWindow::restoreState()
{
    auto& settings = Settings::instance();

    QByteArray geometry = settings.windowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    QByteArray splitterState = settings.splitterState();
    if (!splitterState.isEmpty()) {
        m_splitter->restoreState(splitterState);
    }
}

void MainWindow::saveState()
{
    auto& settings = Settings::instance();
    settings.setWindowGeometry(saveGeometry());
    settings.setSplitterState(m_splitter->saveState());
}

void MainWindow::updateProjectUi()
{
    if (m_projectManager->hasProject()) {
        setWindowTitle(QString("Pexel Manager - %1").arg(m_projectManager->project().name));
        m_searchEdit->setText(m_projectManager->project().searchQuery);
        m_minDurationSpin->setValue(m_projectManager->project().minDuration);
    } else {
        setWindowTitle("Pexel Manager");
    }
}

void MainWindow::onNewProject()
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Project",
        "Project name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    QString bucket = QInputDialog::getText(this, "New Project",
        "S3 Bucket name:", QLineEdit::Normal,
        Settings::instance().s3Bucket(), &ok);
    if (!ok) return;

    if (!m_projectManager->createProject(name, bucket)) {
        QMessageBox::warning(this, "Error", "Failed to create project. Name may already exist.");
        return;
    }

    m_videoList->clear();
    m_videoList->setViewMode(VideoListWidget::ProjectVideos);
    m_viewModeLabel->setText("PROJECT VIDEOS");
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    m_toggleViewBtn->setText("Show Search Results (0)");
    m_loadMoreBtn->setVisible(false);
    m_addToProjectBtn->setVisible(false);
    updateProjectUi();
    m_statusLabel->setText("Created new project: " + name);
}

void MainWindow::onOpenProject()
{
    QStringList projects = ProjectManager::availableProjects();
    if (projects.isEmpty()) {
        QMessageBox::information(this, "Open Project", "No projects found.");
        return;
    }

    // Extract just the project names
    QStringList names;
    for (const auto& path : projects) {
        names.append(QDir(path).dirName());
    }

    bool ok;
    QString selected = QInputDialog::getItem(this, "Open Project",
        "Select project:", names, 0, false, &ok);
    if (!ok) return;

    int index = names.indexOf(selected);
    if (index >= 0 && m_projectManager->loadProject(projects[index])) {
        m_videoList->setProjectVideos(m_projectManager->project().videos);
        m_videoList->setViewMode(VideoListWidget::ProjectVideos);
        m_viewModeLabel->setText("PROJECT VIDEOS");
        m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
        m_toggleViewBtn->setText(QString("Show Search Results (0)"));
        m_loadMoreBtn->setVisible(false);
        m_addToProjectBtn->setVisible(false);
        updateProjectUi();
        m_statusLabel->setText("Loaded project: " + selected);
    }
}

void MainWindow::onSaveProject()
{
    if (m_projectManager->saveProject()) {
        m_statusLabel->setText("Project saved");
    }
}

void MainWindow::onResetProject()
{
    if (!m_projectManager->hasProject()) {
        QMessageBox::information(this, "Reset Project", "No project is currently open.");
        return;
    }

    auto& project = m_projectManager->project();
    int videoCount = project.videos.size();
    int rejectedCount = project.rejectedIds.size();

    auto result = QMessageBox::warning(this, "Reset Project",
        QString("This will permanently delete:\n\n"
                "- %1 videos from the project\n"
                "- %2 rejected video IDs\n\n"
                "Downloaded and scaled files will NOT be deleted.\n\n"
                "Are you sure you want to reset the project?")
            .arg(videoCount)
            .arg(rejectedCount),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    // Clear project data
    project.videos.clear();
    project.rejectedIds.clear();
    project.searchQuery.clear();

    // Clear UI
    m_videoList->clear();
    m_videoList->setViewMode(VideoListWidget::ProjectVideos);
    m_viewModeLabel->setText("PROJECT VIDEOS");
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    m_toggleViewBtn->setText("Show Search Results (0)");
    m_loadMoreBtn->setVisible(false);
    m_addToProjectBtn->setVisible(false);

    // Save the cleared project
    m_projectManager->saveProject();

    m_statusLabel->setText("Project reset - all videos and rejected IDs cleared");
}

void MainWindow::onOpenProjectDir()
{
    if (!m_projectManager->hasProject()) {
        QMessageBox::information(this, "No Project", "No project is currently open.");
        return;
    }

    QString path = m_projectManager->project().path;
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Settings");

    auto layout = new QFormLayout(&dialog);

    auto apiKeyEdit = new QLineEdit(&dialog);
    apiKeyEdit->setText(Settings::instance().pexelsApiKey());
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    layout->addRow("Pexels API Key:", apiKeyEdit);

    auto awsProfileEdit = new QLineEdit(&dialog);
    awsProfileEdit->setText(Settings::instance().awsProfile());
    layout->addRow("AWS Profile:", awsProfileEdit);

    auto bucketEdit = new QLineEdit(&dialog);
    bucketEdit->setText(Settings::instance().s3Bucket());
    layout->addRow("Default S3 Bucket:", bucketEdit);

    auto targetWidthSpin = new QSpinBox(&dialog);
    targetWidthSpin->setRange(640, 3840);
    targetWidthSpin->setValue(Settings::instance().targetWidth());
    layout->addRow("Target Width:", targetWidthSpin);

    auto targetHeightSpin = new QSpinBox(&dialog);
    targetHeightSpin->setRange(480, 2160);
    targetHeightSpin->setValue(Settings::instance().targetHeight());
    layout->addRow("Target Height:", targetHeightSpin);

    auto crfSpin = new QSpinBox(&dialog);
    crfSpin->setRange(15, 35);
    crfSpin->setValue(Settings::instance().ffmpegCrf());
    layout->addRow("FFmpeg CRF:", crfSpin);

    auto presetCombo = new QComboBox(&dialog);
    presetCombo->addItems({"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"});
    presetCombo->setCurrentText(Settings::instance().ffmpegPreset());
    layout->addRow("FFmpeg Preset:", presetCombo);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        auto& settings = Settings::instance();
        settings.setPexelsApiKey(apiKeyEdit->text());
        settings.setAwsProfile(awsProfileEdit->text());
        settings.setS3Bucket(bucketEdit->text());
        settings.setTargetWidth(targetWidthSpin->value());
        settings.setTargetHeight(targetHeightSpin->value());
        settings.setFfmpegCrf(crfSpin->value());
        settings.setFfmpegPreset(presetCombo->currentText());
        m_statusLabel->setText("Settings saved");
    }
}

void MainWindow::onSearch()
{
    if (!m_projectManager->hasProject()) {
        QMessageBox::warning(this, "No Project",
            "Please create or open a project first.");
        return;
    }

    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) return;

    m_currentQuery = query;
    m_currentPage = 1;

    m_projectManager->project().searchQuery = query;
    m_projectManager->project().minDuration = m_minDurationSpin->value();

    m_searchBtn->setEnabled(false);
    m_statusLabel->setText("Searching...");

    m_api->search(query, 1, 40, m_minDurationSpin->value());
}

void MainWindow::onSearchCompleted(const QList<VideoMetadata>& videos, int totalResults, int page)
{
    qDebug() << "onSearchCompleted: videos=" << videos.size() << "total=" << totalResults << "page=" << page;

    m_totalResults = totalResults;
    m_currentPage = page;

    // Switch to search results view
    m_videoList->setViewMode(VideoListWidget::SearchResults);
    m_viewModeLabel->setText("SEARCH RESULTS");
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #d9944a; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");

    const auto& rejectedIds = m_projectManager->project().rejectedIds;

    // Get project video IDs
    QSet<int> projectIds;
    for (const auto& v : m_projectManager->project().videos) {
        projectIds.insert(v.id);
    }

    // For new search, reset start count
    if (page == 1) {
        m_loadMoreStartCount = 0;
        m_videoList->setSearchResults(videos, rejectedIds, projectIds);
    } else {
        m_videoList->addSearchResults(videos, rejectedIds, projectIds);
    }

    int countAfter = m_videoList->searchResultsCount();
    int addedThisSession = countAfter - m_loadMoreStartCount;
    int totalFetched = page * 40;  // Approximate, assuming 40 per page

    qDebug() << "  startCount=" << m_loadMoreStartCount << "countAfter=" << countAfter
             << "addedThisSession=" << addedThisSession << "totalFetched=" << totalFetched;

    // Keep loading until we have 40 new results this session, or exhausted all results
    bool moreAvailable = totalFetched < totalResults;
    bool needMore = addedThisSession < 40;

    if (needMore && moreAvailable) {
        // Auto-load next page
        m_statusLabel->setText(QString("Loading... found %1 new so far (page %2)").arg(addedThisSession).arg(page));
        m_api->search(m_currentQuery, page + 1, 40, m_minDurationSpin->value());
        return;  // Don't enable buttons yet
    }

    // Done loading
    m_searchBtn->setEnabled(true);
    m_loadMoreBtn->setEnabled(moreAvailable);
    m_loadMoreBtn->setVisible(true);
    m_addToProjectBtn->setVisible(true);
    m_toggleViewBtn->setText(QString("Show Project (%1)").arg(m_videoList->projectVideosCount()));

    if (countAfter == 0) {
        m_statusLabel->setText(QString("No new videos found (all %1 results already in project or rejected)").arg(totalResults));
    } else if (page == 1) {
        m_statusLabel->setText(QString("Found %1 new videos").arg(countAfter));
    } else {
        m_statusLabel->setText(QString("Added %1 new videos (%2 total)").arg(addedThisSession).arg(countAfter));
    }
}

void MainWindow::onSearchError(const QString& error)
{
    m_searchBtn->setEnabled(true);
    m_statusLabel->setText("Search error: " + error);
    QMessageBox::warning(this, "Search Error", error);
}

void MainWindow::onLoadMore()
{
    if (m_currentQuery.isEmpty()) return;

    m_loadMoreBtn->setEnabled(false);
    m_searchBtn->setEnabled(false);
    m_loadMoreStartCount = m_videoList->searchResultsCount();
    m_statusLabel->setText("Loading more...");

    m_api->search(m_currentQuery, m_currentPage + 1, 40, m_minDurationSpin->value());
}

void MainWindow::onAddToProject()
{
    if (!m_projectManager->hasProject()) {
        QMessageBox::warning(this, "No Project", "Please create or open a project first.");
        return;
    }

    auto searchResults = m_videoList->getSearchResults();
    if (searchResults.isEmpty()) {
        m_statusLabel->setText("No videos to add");
        return;
    }

    // Add search results to project
    m_projectManager->addVideos(searchResults);

    // Update the project videos in the list widget
    m_videoList->setProjectVideos(m_projectManager->project().videos);

    // Clear search results
    m_videoList->clearSearchResults();

    // Switch to project view
    m_videoList->setViewMode(VideoListWidget::ProjectVideos);
    m_viewModeLabel->setText("PROJECT VIDEOS");
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    m_toggleViewBtn->setText(QString("Show Search Results (%1)").arg(m_videoList->searchResultsCount()));
    m_loadMoreBtn->setVisible(false);
    m_addToProjectBtn->setVisible(false);

    m_statusLabel->setText(QString("Added %1 videos to project. Project now has %2 videos.")
        .arg(searchResults.size())
        .arg(m_videoList->projectVideosCount()));

    m_projectManager->saveProject();
}

void MainWindow::onToggleView()
{
    if (m_videoList->viewMode() == VideoListWidget::SearchResults) {
        // Switch to project view
        m_videoList->setViewMode(VideoListWidget::ProjectVideos);
        m_viewModeLabel->setText("PROJECT VIDEOS");
        m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
        m_toggleViewBtn->setText(QString("Show Search Results (%1)").arg(m_videoList->searchResultsCount()));
        m_loadMoreBtn->setVisible(false);
        m_addToProjectBtn->setVisible(false);
        m_statusLabel->setText(QString("Project: %1 videos").arg(m_videoList->projectVideosCount()));
    } else {
        // Switch to search results view
        m_videoList->setViewMode(VideoListWidget::SearchResults);
        m_viewModeLabel->setText("SEARCH RESULTS");
        m_viewModeLabel->setStyleSheet("QLabel { background-color: #d9944a; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
        m_toggleViewBtn->setText(QString("Show Project (%1)").arg(m_videoList->projectVideosCount()));
        m_loadMoreBtn->setVisible(true);
        m_addToProjectBtn->setVisible(true);
        m_statusLabel->setText(QString("Search results: %1 videos").arg(m_videoList->searchResultsCount()));
    }
}

void MainWindow::onVideoSelected(const VideoMetadata& video)
{
    qDebug() << "MainWindow::onVideoSelected id=" << video.id;
    qDebug() << "  previewUrl=" << video.previewVideoUrl;
    qDebug() << "  localRawPath=" << video.localRawPath << " exists=" << QFile::exists(video.localRawPath);
    qDebug() << "  localScaledPath=" << video.localScaledPath << " exists=" << QFile::exists(video.localScaledPath);

    // Prefer local file if downloaded
    if (!video.localScaledPath.isEmpty() && QFile::exists(video.localScaledPath)) {
        qDebug() << "  -> playing scaled file";
        m_player->playFile(video.localScaledPath);
    } else if (!video.localRawPath.isEmpty() && QFile::exists(video.localRawPath)) {
        qDebug() << "  -> playing raw file";
        m_player->playFile(video.localRawPath);
    } else if (!video.previewVideoUrl.isEmpty()) {
        qDebug() << "  -> playing preview URL";
        m_player->playUrl(video.previewVideoUrl);
    } else {
        qDebug() << "  -> NO VIDEO SOURCE AVAILABLE";
    }

    QString info = QString("ID: %1\nAuthor: %2\nDuration: %3s\nSize: %4x%5")
        .arg(video.id)
        .arg(video.author)
        .arg(video.duration)
        .arg(video.width)
        .arg(video.height);

    if (video.isRejected) info += "\n[REJECTED]";
    if (video.isDownloaded) info += "\n[Downloaded]";
    if (video.isScaled) info += "\n[Scaled]";
    if (video.isUploaded) info += "\n[Uploaded]";

    m_videoInfoLabel->setText(info);
}

void MainWindow::onVideoRejected(int id)
{
    m_projectManager->rejectVideo(id);

    // Update button counts
    if (m_videoList->viewMode() == VideoListWidget::SearchResults) {
        m_toggleViewBtn->setText(QString("Show Project (%1)").arg(m_videoList->projectVideosCount()));
    } else {
        m_toggleViewBtn->setText(QString("Show Search (%1)").arg(m_videoList->searchResultsCount()));
    }

    m_statusLabel->setText(QString("Rejected video %1").arg(id));
}

void MainWindow::onDownloadSelected()
{
    if (!m_projectManager->hasProject()) return;

    auto& project = m_projectManager->project();
    int maxWidth = m_resolutionCombo->currentData().toInt();
    int count = 0;

    for (auto& video : project.videos) {
        if (video.isRejected || video.isDownloaded) continue;

        auto best = video.getBestVideoFile(maxWidth);
        if (best.link.isEmpty()) continue;

        QString filename = QString("%1_%2_%3s.mp4")
            .arg(video.id)
            .arg(video.author.left(20).replace(' ', '_'))
            .arg(video.duration);

        // Remove invalid characters
        filename.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");

        QString destPath = project.rawDir() + "/" + filename;
        video.localRawPath = destPath;

        m_downloadManager->downloadVideo(video.id, best.link, destPath);
        count++;
    }

    if (count > 0) {
        m_downloadTotal = count;
        m_downloadCompleted = 0;
        m_progressBar->setVisible(true);
        m_statusLabel->setText(QString("Downloading 1/%1 videos...").arg(count));
    } else {
        m_statusLabel->setText("No videos to download");
    }
}

void MainWindow::onScaleSelected()
{
    if (!m_projectManager->hasProject()) return;

    auto& project = m_projectManager->project();
    auto& settings = Settings::instance();
    int count = 0;

    for (auto& video : project.videos) {
        if (video.isRejected || !video.isDownloaded || video.isScaled) continue;
        if (!QFile::exists(video.localRawPath)) continue;

        QString filename = QFileInfo(video.localRawPath).fileName();
        QString destPath = project.scaledDir() + "/" + filename;
        video.localScaledPath = destPath;

        m_uploadManager->scaleVideo(
            video.id,
            video.localRawPath,
            destPath,
            settings.targetWidth(),
            settings.targetHeight(),
            settings.ffmpegCrf(),
            settings.ffmpegPreset()
        );
        count++;
    }

    if (count > 0) {
        m_scaleTotal = count;
        m_scaleCompleted = 0;
        m_statusLabel->setText(QString("Scaling 1/%1 videos...").arg(count));
    } else {
        m_statusLabel->setText("No videos to scale");
    }
}

void MainWindow::onUploadSelected()
{
    if (!m_projectManager->hasProject()) return;

    auto& project = m_projectManager->project();
    if (project.s3Bucket.isEmpty()) {
        QMessageBox::warning(this, "No Bucket",
            "Please set an S3 bucket in project settings.");
        return;
    }

    int count = 0;

    for (auto& video : project.videos) {
        if (video.isRejected || !video.isScaled || video.isUploaded) continue;
        if (!QFile::exists(video.localScaledPath)) continue;

        QString key = "videos/" + QFileInfo(video.localScaledPath).fileName();

        m_uploadManager->uploadToS3(video.id, video.localScaledPath, project.s3Bucket, key);
        count++;
    }

    if (count > 0) {
        m_uploadTotal = count;
        m_uploadCompleted = 0;
        // Also upload index.json after all videos
        m_uploadManager->uploadIndexJson(project.s3Bucket, project.name);
        m_statusLabel->setText(QString("Uploading 1/%1 videos...").arg(count));
    } else {
        m_statusLabel->setText("No videos to upload");
    }
}

void MainWindow::onDownloadCompleted(int videoId, const QString& path)
{
    for (auto& video : m_projectManager->project().videos) {
        if (video.id == videoId) {
            video.localRawPath = path;
            video.isDownloaded = true;
            m_projectManager->updateVideo(video);
            m_videoList->updateVideoStatus(videoId, &video);
            break;
        }
    }

    m_downloadCompleted++;

    if (!m_downloadManager->isDownloading()) {
        m_progressBar->setVisible(false);
        m_statusLabel->setText(QString("Downloaded %1 videos").arg(m_downloadCompleted));
    } else {
        m_statusLabel->setText(QString("Downloading %1/%2 videos...").arg(m_downloadCompleted + 1).arg(m_downloadTotal));
    }
}

void MainWindow::onScaleCompleted(int videoId, const QString& path)
{
    for (auto& video : m_projectManager->project().videos) {
        if (video.id == videoId) {
            video.localScaledPath = path;
            video.isScaled = true;
            m_projectManager->updateVideo(video);
            m_videoList->updateVideoStatus(videoId, &video);
            break;
        }
    }

    m_scaleCompleted++;

    if (!m_uploadManager->isBusy()) {
        m_statusLabel->setText(QString("Scaled %1 videos").arg(m_scaleCompleted));
    } else {
        m_statusLabel->setText(QString("Scaling %1/%2 videos...").arg(m_scaleCompleted + 1).arg(m_scaleTotal));
    }
}

void MainWindow::onUploadCompleted(int videoId)
{
    for (auto& video : m_projectManager->project().videos) {
        if (video.id == videoId) {
            video.isUploaded = true;
            m_projectManager->updateVideo(video);
            m_videoList->updateVideoStatus(videoId, &video);
            break;
        }
    }

    m_uploadCompleted++;

    if (!m_uploadManager->isBusy()) {
        m_statusLabel->setText(QString("Uploaded %1 videos").arg(m_uploadCompleted));
    } else {
        m_statusLabel->setText(QString("Uploading %1/%2 videos...").arg(m_uploadCompleted + 1).arg(m_uploadTotal));
    }
}

void MainWindow::updateStatus()
{
    // Could add more detailed status updates here
}
