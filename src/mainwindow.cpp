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

    // Media list connections
    connect(m_mediaList, &MediaListWidget::mediaSelected, this, &MainWindow::onMediaSelected);
    connect(m_mediaList, &MediaListWidget::mediaRejected, this, &MainWindow::onMediaRejected);

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
    connect(m_downloadManager, &DownloadManager::allDownloadsCompleted, this, [this]() {
        m_projectManager->saveProject();
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
    connect(m_uploadManager, &UploadManager::categoriesUploadCompleted, this, [this]() {
        m_statusLabel->setText("Upload completed (including categories.json)");
    });
    connect(m_uploadManager, &UploadManager::categoriesUploadError, this, [this](const QString& error) {
        m_statusLabel->setText(QString("Categories upload error: %1").arg(error));
    });
    connect(m_uploadManager, &UploadManager::s3DeleteCompleted, this, [this](const QString& bucket) {
        m_statusLabel->setText(QString("S3 content deleted from '%1'").arg(bucket));
    });
    connect(m_uploadManager, &UploadManager::s3DeleteError, this, [this](const QString& bucket, const QString& error) {
        m_statusLabel->setText(QString("S3 delete error for '%1': %2").arg(bucket, error));
    });
    connect(m_uploadManager, &UploadManager::allTasksCompleted, this, [this]() {
        m_projectManager->saveProject();
    });

    // Try to load last project
    QString lastProject = Settings::instance().lastProjectPath();
    if (!lastProject.isEmpty() && QDir(lastProject).exists()) {
        m_projectManager->loadProject(lastProject);
        m_mediaList->setProjectMedia(m_projectManager->project().media);
        m_mediaList->setViewMode(MediaListWidget::ProjectMedia);
        m_viewModeLabel->setText("PROJECT MEDIA");
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
    m_searchEdit->setPlaceholderText("Search Pexels media...");
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearch);
    searchLayout->addWidget(m_searchEdit, 1);

    m_searchBtn = new QPushButton("Search", this);
    connect(m_searchBtn, &QPushButton::clicked, this, &MainWindow::onSearch);
    searchLayout->addWidget(m_searchBtn);

    leftLayout->addLayout(searchLayout);

    // Options row
    auto optionsLayout = new QHBoxLayout;

    // Media type selector
    optionsLayout->addWidget(new QLabel("Type:", this));
    m_mediaTypeCombo = new QComboBox(this);
    m_mediaTypeCombo->addItem("Videos", static_cast<int>(SearchType::Videos));
    m_mediaTypeCombo->addItem("Photos", static_cast<int>(SearchType::Photos));
    optionsLayout->addWidget(m_mediaTypeCombo);

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
    m_viewModeLabel = new QLabel("PROJECT MEDIA", this);
    m_viewModeLabel->setAlignment(Qt::AlignCenter);
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    leftLayout->addWidget(m_viewModeLabel);

    // View toggle button
    m_toggleViewBtn = new QPushButton("Show Search Results (0)", this);
    connect(m_toggleViewBtn, &QPushButton::clicked, this, &MainWindow::onToggleView);
    leftLayout->addWidget(m_toggleViewBtn);

    // Media list
    m_mediaList = new MediaListWidget(this);
    leftLayout->addWidget(m_mediaList, 1);

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

    m_mediaInfoLabel = new QLabel("Select media to preview", this);
    m_mediaInfoLabel->setWordWrap(true);
    rightLayout->addWidget(m_mediaInfoLabel);

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
    fileMenu->addAction("&Delete Project...", this, &MainWindow::onDeleteProject);
    fileMenu->addSeparator();
    fileMenu->addAction("Upload &Catalog JSON...", this, &MainWindow::onUploadCatalog);
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

    QString categoryId = QInputDialog::getText(this, "New Project",
        "Category ID (e.g., espresso, landscapes):", QLineEdit::Normal,
        name.toLower().replace(' ', '-'), &ok);
    if (!ok || categoryId.isEmpty()) return;

    if (!m_projectManager->createProject(name, categoryId)) {
        QMessageBox::warning(this, "Error", "Failed to create project. Name may already exist.");
        return;
    }

    m_mediaList->clear();
    m_mediaList->setViewMode(MediaListWidget::ProjectMedia);
    m_viewModeLabel->setText("PROJECT MEDIA");
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
        m_mediaList->setProjectMedia(m_projectManager->project().media);
        m_mediaList->setViewMode(MediaListWidget::ProjectMedia);
        m_viewModeLabel->setText("PROJECT MEDIA");
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
    int mediaCount = project.media.size();
    int rejectedCount = project.rejectedIds.size();

    auto result = QMessageBox::warning(this, "Reset Project",
        QString("This will permanently delete:\n\n"
                "- %1 media items from the project\n"
                "- %2 rejected IDs\n\n"
                "Downloaded and scaled files will NOT be deleted.\n\n"
                "Are you sure you want to reset the project?")
            .arg(mediaCount)
            .arg(rejectedCount),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    // Clear project data
    project.media.clear();
    project.rejectedIds.clear();
    project.searchQuery.clear();

    // Clear UI
    m_mediaList->clear();
    m_mediaList->setViewMode(MediaListWidget::ProjectMedia);
    m_viewModeLabel->setText("PROJECT MEDIA");
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    m_toggleViewBtn->setText("Show Search Results (0)");
    m_loadMoreBtn->setVisible(false);
    m_addToProjectBtn->setVisible(false);

    m_projectManager->saveProject();

    m_statusLabel->setText("Project reset - all media and rejected IDs cleared");
}

void MainWindow::onDeleteProject()
{
    if (!m_projectManager->hasProject()) {
        QMessageBox::information(this, "Delete Project", "No project is currently open.");
        return;
    }

    auto& project = m_projectManager->project();
    QString bucket = project.s3Bucket;
    QString categoryId = project.categoryId;
    QString projectPath = project.path;
    QString projectName = project.name;

    auto result = QMessageBox::warning(this, "Delete Project",
        QString("This will PERMANENTLY delete:\n\n"
                "LOCAL:\n"
                "  - All project files in: %1\n\n"
                "S3:\n"
                "  - Catalog file: catalogs/%2.json\n\n"
                "The category will also be removed from categories.json.\n\n"
                "This action CANNOT be undone. Are you sure?")
            .arg(projectPath)
            .arg(categoryId),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    m_statusLabel->setText("Deleting project from S3...");

    // Delete catalog from S3
    m_uploadManager->deleteFromS3(bucket, categoryId);

    // Remove from categories.json and re-upload
    m_uploadManager->removeCategoryAndUpload(bucket, categoryId);

    // Delete local project files
    if (m_projectManager->deleteProject(projectPath)) {
        m_mediaList->clear();
        m_viewModeLabel->setText("NO PROJECT");
        m_viewModeLabel->setStyleSheet("QLabel { background-color: #888; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
        updateProjectUi();
        m_statusLabel->setText(QString("Project '%1' deleted").arg(projectName));
    } else {
        QMessageBox::warning(this, "Error", "Failed to delete local project files.");
    }
}

void MainWindow::onUploadCatalog()
{
    if (!m_projectManager->hasProject()) {
        QMessageBox::information(this, "No Project", "No project is currently open.");
        return;
    }

    auto& project = m_projectManager->project();

    if (project.s3Bucket.isEmpty()) {
        QMessageBox::warning(this, "No S3 Bucket",
            "Please set an S3 bucket in settings.");
        return;
    }

    // Count media with scaled files
    int uploadedCount = 0;
    for (const auto& item : project.media) {
        if (!item.isRejected && !item.localScaledPath.isEmpty() && QFile::exists(item.localScaledPath)) {
            uploadedCount++;
        }
    }

    if (uploadedCount == 0) {
        QMessageBox::information(this, "No Media",
            "No scaled media found in this project.");
        return;
    }

    auto result = QMessageBox::question(this, "Upload Catalog",
        QString("This will upload catalog with %1 items to:\n\n"
                "s3://%2/catalogs/%3.json\n\n"
                "Continue?")
            .arg(uploadedCount)
            .arg(project.s3Bucket)
            .arg(project.categoryId),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (result == QMessageBox::Yes) {
        m_uploadManager->uploadCatalogJson(project.s3Bucket, project.categoryId, project.media);
        m_statusLabel->setText("Uploading catalog...");
    }
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
    layout->addRow("S3 Bucket:", bucketEdit);

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
    m_currentSearchType = static_cast<SearchType>(m_mediaTypeCombo->currentData().toInt());

    m_projectManager->project().searchQuery = query;
    m_projectManager->project().minDuration = m_minDurationSpin->value();

    m_searchBtn->setEnabled(false);
    m_statusLabel->setText("Searching...");

    m_api->search(query, m_currentSearchType, 1, 40, m_minDurationSpin->value());
}

void MainWindow::onSearchCompleted(const QList<MediaMetadata>& media, int totalResults, int page)
{
    qDebug() << "onSearchCompleted: media=" << media.size() << "total=" << totalResults << "page=" << page;

    m_totalResults = totalResults;
    m_currentPage = page;

    // Switch to search results view
    m_mediaList->setViewMode(MediaListWidget::SearchResults);
    m_viewModeLabel->setText("SEARCH RESULTS");
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #d9944a; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");

    const auto& rejectedIds = m_projectManager->project().rejectedIds;

    // Get project media IDs
    QSet<int> projectIds;
    for (const auto& m : m_projectManager->project().media) {
        projectIds.insert(m.id);
    }

    // For new search, reset start count
    if (page == 1) {
        m_loadMoreStartCount = 0;
        m_mediaList->setSearchResults(media, rejectedIds, projectIds);
    } else {
        m_mediaList->addSearchResults(media, rejectedIds, projectIds);
    }

    int countAfter = m_mediaList->searchResultsCount();
    int addedThisSession = countAfter - m_loadMoreStartCount;
    int totalFetched = page * 40;

    qDebug() << "  startCount=" << m_loadMoreStartCount << "countAfter=" << countAfter
             << "addedThisSession=" << addedThisSession << "totalFetched=" << totalFetched;

    // Keep loading until we have 40 new results this session, or exhausted all results
    bool moreAvailable = totalFetched < totalResults;
    bool needMore = addedThisSession < 40;

    if (needMore && moreAvailable) {
        m_statusLabel->setText(QString("Loading... found %1 new so far (page %2)").arg(addedThisSession).arg(page));
        m_api->search(m_currentQuery, m_currentSearchType, page + 1, 40, m_minDurationSpin->value());
        return;
    }

    // Done loading
    m_searchBtn->setEnabled(true);
    m_loadMoreBtn->setEnabled(moreAvailable);
    m_loadMoreBtn->setVisible(true);
    m_addToProjectBtn->setVisible(true);
    m_toggleViewBtn->setText(QString("Show Project (%1)").arg(m_mediaList->projectMediaCount()));

    if (countAfter == 0) {
        m_statusLabel->setText(QString("No new media found (all %1 results already in project or rejected)").arg(totalResults));
    } else if (page == 1) {
        m_statusLabel->setText(QString("Found %1 new media items").arg(countAfter));
    } else {
        m_statusLabel->setText(QString("Added %1 new items (%2 total)").arg(addedThisSession).arg(countAfter));
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
    m_loadMoreStartCount = m_mediaList->searchResultsCount();
    m_statusLabel->setText("Loading more...");

    m_api->search(m_currentQuery, m_currentSearchType, m_currentPage + 1, 40, m_minDurationSpin->value());
}

void MainWindow::onAddToProject()
{
    if (!m_projectManager->hasProject()) {
        QMessageBox::warning(this, "No Project", "Please create or open a project first.");
        return;
    }

    auto searchResults = m_mediaList->getSearchResults();
    if (searchResults.isEmpty()) {
        m_statusLabel->setText("No media to add");
        return;
    }

    // Add search results to project
    m_projectManager->addMedia(searchResults);

    // Update the project media in the list widget
    m_mediaList->setProjectMedia(m_projectManager->project().media);

    // Clear search results
    m_mediaList->clearSearchResults();

    // Switch to project view
    m_mediaList->setViewMode(MediaListWidget::ProjectMedia);
    m_viewModeLabel->setText("PROJECT MEDIA");
    m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
    m_toggleViewBtn->setText(QString("Show Search Results (%1)").arg(m_mediaList->searchResultsCount()));
    m_loadMoreBtn->setVisible(false);
    m_addToProjectBtn->setVisible(false);

    m_statusLabel->setText(QString("Added %1 items to project. Project now has %2 items.")
        .arg(searchResults.size())
        .arg(m_mediaList->projectMediaCount()));

    m_projectManager->saveProject();
}

void MainWindow::onToggleView()
{
    if (m_mediaList->viewMode() == MediaListWidget::SearchResults) {
        // Switch to project view
        m_mediaList->setViewMode(MediaListWidget::ProjectMedia);
        m_viewModeLabel->setText("PROJECT MEDIA");
        m_viewModeLabel->setStyleSheet("QLabel { background-color: #4a90d9; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
        m_toggleViewBtn->setText(QString("Show Search Results (%1)").arg(m_mediaList->searchResultsCount()));
        m_loadMoreBtn->setVisible(false);
        m_addToProjectBtn->setVisible(false);
        m_statusLabel->setText(QString("Project: %1 items").arg(m_mediaList->projectMediaCount()));
    } else {
        // Switch to search results view
        m_mediaList->setViewMode(MediaListWidget::SearchResults);
        m_viewModeLabel->setText("SEARCH RESULTS");
        m_viewModeLabel->setStyleSheet("QLabel { background-color: #d9944a; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }");
        m_toggleViewBtn->setText(QString("Show Project (%1)").arg(m_mediaList->projectMediaCount()));
        m_loadMoreBtn->setVisible(true);
        m_addToProjectBtn->setVisible(true);
        m_statusLabel->setText(QString("Search results: %1 items").arg(m_mediaList->searchResultsCount()));
    }
}

void MainWindow::onMediaSelected(const MediaMetadata& media)
{
    qDebug() << "MainWindow::onMediaSelected id=" << media.id << "type=" << (media.isVideo() ? "video" : "image");

    // Prefer local file if downloaded
    if (!media.localScaledPath.isEmpty() && QFile::exists(media.localScaledPath)) {
        qDebug() << "  -> showing scaled file";
        if (media.isVideo()) {
            m_player->playFile(media.localScaledPath);
        } else {
            m_player->showImageFile(media.localScaledPath);
        }
    } else if (!media.localRawPath.isEmpty() && QFile::exists(media.localRawPath)) {
        qDebug() << "  -> showing raw file";
        if (media.isVideo()) {
            m_player->playFile(media.localRawPath);
        } else {
            m_player->showImageFile(media.localRawPath);
        }
    } else if (media.isVideo() && !media.previewVideoUrl.isEmpty()) {
        qDebug() << "  -> playing preview URL";
        m_player->playUrl(media.previewVideoUrl);
    } else if (media.isImage() && !media.largeImageUrl.isEmpty()) {
        qDebug() << "  -> showing large image URL";
        m_player->showImageUrl(media.largeImageUrl);
    } else if (media.isImage() && !media.originalImageUrl.isEmpty()) {
        qDebug() << "  -> showing original image URL";
        m_player->showImageUrl(media.originalImageUrl);
    } else {
        qDebug() << "  -> NO MEDIA SOURCE AVAILABLE";
    }

    QString typeStr = media.isVideo() ? "Video" : "Image";
    QString info = QString("ID: %1 [%2]\nAuthor: %3\n")
        .arg(media.id)
        .arg(typeStr)
        .arg(media.author);

    if (media.isVideo()) {
        info += QString("Duration: %1s\n").arg(media.duration);
    }
    info += QString("Size: %1x%2").arg(media.width).arg(media.height);

    if (media.isRejected) info += "\n[REJECTED]";
    if (media.isDownloaded) info += "\n[Downloaded]";
    if (media.isScaled) info += "\n[Scaled]";
    if (media.isUploaded) info += "\n[Uploaded]";

    m_mediaInfoLabel->setText(info);
}

void MainWindow::onMediaRejected(int id)
{
    m_projectManager->rejectMedia(id);

    // Update button counts
    if (m_mediaList->viewMode() == MediaListWidget::SearchResults) {
        m_toggleViewBtn->setText(QString("Show Project (%1)").arg(m_mediaList->projectMediaCount()));
    } else {
        m_toggleViewBtn->setText(QString("Show Search (%1)").arg(m_mediaList->searchResultsCount()));
    }

    m_statusLabel->setText(QString("Rejected media %1").arg(id));
}

void MainWindow::onDownloadSelected()
{
    if (!m_projectManager->hasProject()) return;

    auto& project = m_projectManager->project();
    int maxWidth = m_resolutionCombo->currentData().toInt();
    int count = 0;

    for (auto& item : project.media) {
        if (item.isRejected || item.isDownloaded) continue;

        QUrl downloadUrl = item.getDownloadUrl(maxWidth);
        if (downloadUrl.isEmpty()) continue;

        QString ext = item.getFileExtension();
        QString filename;

        if (item.isVideo()) {
            filename = QString("%1_%2_%3s%4")
                .arg(item.id)
                .arg(item.author.left(20).replace(' ', '_'))
                .arg(item.duration)
                .arg(ext);
        } else {
            filename = QString("%1_%2%3")
                .arg(item.id)
                .arg(item.author.left(20).replace(' ', '_'))
                .arg(ext);
        }

        // Remove invalid characters
        filename.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");

        QString destPath = project.rawDir() + "/" + filename;
        item.localRawPath = destPath;

        m_downloadManager->downloadMedia(item.id, downloadUrl, destPath);
        count++;
    }

    if (count > 0) {
        m_downloadTotal = count;
        m_downloadCompleted = 0;
        m_progressBar->setVisible(true);
        m_statusLabel->setText(QString("Downloading 1/%1 items...").arg(count));
    } else {
        m_statusLabel->setText("No media to download");
    }
}

void MainWindow::onScaleSelected()
{
    if (!m_projectManager->hasProject()) return;

    auto& project = m_projectManager->project();
    auto& settings = Settings::instance();
    int count = 0;

    for (auto& item : project.media) {
        if (item.isRejected || !item.isDownloaded || item.isScaled) continue;
        if (!QFile::exists(item.localRawPath)) continue;

        QString inputFilename = QFileInfo(item.localRawPath).fileName();
        QString outputExt = item.getFileExtension();

        // Change extension if needed (e.g., downloaded as .png, output as .jpg)
        QString baseName = QFileInfo(inputFilename).completeBaseName();
        QString destPath = project.scaledDir() + "/" + baseName + outputExt;
        item.localScaledPath = destPath;

        m_uploadManager->scaleMedia(
            item.id,
            item.type,
            item.localRawPath,
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
        m_statusLabel->setText(QString("Scaling 1/%1 items...").arg(count));
    } else {
        m_statusLabel->setText("No media to scale");
    }
}

void MainWindow::onUploadSelected()
{
    if (!m_projectManager->hasProject()) return;

    auto& project = m_projectManager->project();
    if (project.s3Bucket.isEmpty()) {
        QMessageBox::warning(this, "No Bucket",
            "Please set an S3 bucket in settings.");
        return;
    }

    int count = 0;

    for (auto& item : project.media) {
        if (item.isRejected || !item.isScaled || item.isUploaded) continue;
        if (!QFile::exists(item.localScaledPath)) continue;

        QString key = "media/" + QFileInfo(item.localScaledPath).fileName();

        m_uploadManager->uploadToS3(item.id, item.localScaledPath, project.s3Bucket, key);
        count++;
    }

    if (count > 0) {
        m_uploadTotal = count;
        m_uploadCompleted = 0;
        // Also upload index.json, catalog.json, and categories.json
        m_uploadManager->uploadIndexJson(project.s3Bucket, project.categoryId, project.name);
        m_uploadManager->uploadCatalogJson(project.s3Bucket, project.categoryId, project.media);
        m_uploadManager->uploadCategoriesJson(project.s3Bucket, project.categoryId, project.name);
        m_statusLabel->setText(QString("Uploading 1/%1 items...").arg(count));
    } else {
        m_statusLabel->setText("No media to upload");
    }
}

void MainWindow::onDownloadCompleted(int mediaId, const QString& path)
{
    for (auto& item : m_projectManager->project().media) {
        if (item.id == mediaId) {
            item.localRawPath = path;
            item.isDownloaded = true;
            m_projectManager->updateMedia(item);
            m_mediaList->updateMediaStatus(mediaId, &item);
            break;
        }
    }

    m_downloadCompleted++;

    if (!m_downloadManager->isDownloading()) {
        m_progressBar->setVisible(false);
        m_statusLabel->setText(QString("Downloaded %1 items").arg(m_downloadCompleted));
    } else {
        m_statusLabel->setText(QString("Downloading %1/%2 items...").arg(m_downloadCompleted + 1).arg(m_downloadTotal));
    }
}

void MainWindow::onScaleCompleted(int mediaId, const QString& path)
{
    for (auto& item : m_projectManager->project().media) {
        if (item.id == mediaId) {
            item.localScaledPath = path;
            item.isScaled = true;
            m_projectManager->updateMedia(item);
            m_mediaList->updateMediaStatus(mediaId, &item);
            break;
        }
    }

    m_scaleCompleted++;

    if (!m_uploadManager->isBusy()) {
        m_statusLabel->setText(QString("Scaled %1 items").arg(m_scaleCompleted));
    } else {
        m_statusLabel->setText(QString("Scaling %1/%2 items...").arg(m_scaleCompleted + 1).arg(m_scaleTotal));
    }
}

void MainWindow::onUploadCompleted(int mediaId)
{
    for (auto& item : m_projectManager->project().media) {
        if (item.id == mediaId) {
            item.isUploaded = true;
            m_projectManager->updateMedia(item);
            m_mediaList->updateMediaStatus(mediaId, &item);
            break;
        }
    }

    m_uploadCompleted++;

    if (!m_uploadManager->isBusy()) {
        m_statusLabel->setText(QString("Uploaded %1 items").arg(m_uploadCompleted));
    } else {
        m_statusLabel->setText(QString("Uploading %1/%2 items...").arg(m_uploadCompleted + 1).arg(m_uploadTotal));
    }
}

void MainWindow::updateStatus()
{
    // Could add more detailed status updates here
}
