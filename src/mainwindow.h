#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QComboBox>

#include "pexelsapi.h"
#include "medialistwidget.h"
#include "videoplayerwidget.h"
#include "projectmanager.h"
#include "downloadmanager.h"
#include "uploadmanager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Menu actions
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onResetProject();
    void onDeleteProject();
    void onOpenProjectDir();
    void onUploadCatalog();
    void onSettings();

    // Search
    void onSearch();
    void onSearchCompleted(const QList<MediaMetadata>& media, int totalResults, int page);
    void onSearchError(const QString& error);
    void onLoadMore();
    void onAddToProject();
    void onToggleView();

    // Media selection
    void onMediaSelected(const MediaMetadata& media);
    void onMediaRejected(int id);

    // Download/Scale/Upload
    void onDownloadSelected();
    void onScaleSelected();
    void onUploadSelected();

    void onDownloadCompleted(int mediaId, const QString& path);
    void onScaleCompleted(int mediaId, const QString& path);
    void onUploadCompleted(int mediaId);

    // Status updates
    void updateStatus();

private:
    void setupUi();
    void setupMenus();
    void restoreState();
    void saveState();
    void updateProjectUi();

    // UI components
    QSplitter* m_splitter;

    // Left panel
    QWidget* m_leftPanel;
    QLineEdit* m_searchEdit;
    QPushButton* m_searchBtn;
    QSpinBox* m_minDurationSpin;
    QComboBox* m_resolutionCombo;
    QComboBox* m_mediaTypeCombo;
    QPushButton* m_toggleViewBtn;
    QLabel* m_viewModeLabel;
    MediaListWidget* m_mediaList;
    QPushButton* m_addToProjectBtn;
    QPushButton* m_loadMoreBtn;

    // Right panel
    QWidget* m_rightPanel;
    VideoPlayerWidget* m_player;
    QLabel* m_mediaInfoLabel;

    // Bottom toolbar
    QPushButton* m_downloadBtn;
    QPushButton* m_scaleBtn;
    QPushButton* m_uploadBtn;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // Core components
    PexelsApi* m_api;
    ProjectManager* m_projectManager;
    DownloadManager* m_downloadManager;
    UploadManager* m_uploadManager;

    // Search state
    QString m_currentQuery;
    SearchType m_currentSearchType = SearchType::Videos;
    int m_currentPage = 1;
    int m_totalResults = 0;
    int m_loadMoreStartCount = 0;

    // Download/Scale/Upload progress
    int m_downloadTotal = 0;
    int m_downloadCompleted = 0;
    int m_scaleTotal = 0;
    int m_scaleCompleted = 0;
    int m_uploadTotal = 0;
    int m_uploadCompleted = 0;
};
