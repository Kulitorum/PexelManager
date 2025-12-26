#include "videoplayerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QNetworkReply>
#include <QPixmap>

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void VideoPlayerWidget::setupUi()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Stacked widget for video/image
    m_stack = new QStackedWidget(this);
    m_stack->setMinimumSize(320, 200);
    layout->addWidget(m_stack, 1);

    // Video widget (index 0)
    m_videoWidget = new QVideoWidget(this);
    m_stack->addWidget(m_videoWidget);

    // Image label (index 1)
    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setStyleSheet("background-color: black;");
    m_stack->addWidget(m_imageLabel);

    // Player setup
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(0.5);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

    // Controls widget (to hide for images)
    m_controlsWidget = new QWidget(this);
    auto controlsMainLayout = new QVBoxLayout(m_controlsWidget);
    controlsMainLayout->setContentsMargins(0, 0, 0, 0);

    // Controls layout
    auto controlsLayout = new QHBoxLayout;

    m_playPauseBtn = new QPushButton("Play", this);
    m_playPauseBtn->setFixedWidth(60);
    controlsLayout->addWidget(m_playPauseBtn);

    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setRange(0, 0);
    controlsLayout->addWidget(m_seekSlider, 1);

    m_timeLabel = new QLabel("0:00 / 0:00", this);
    m_timeLabel->setFixedWidth(100);
    controlsLayout->addWidget(m_timeLabel);

    controlsMainLayout->addLayout(controlsLayout);

    // Speed control layout
    auto speedLayout = new QHBoxLayout;

    speedLayout->addWidget(new QLabel("Speed:", this));

    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider->setRange(10, 100);  // 1.0x to 10.0x (value / 10)
    m_speedSlider->setValue(10);
    m_speedSlider->setFixedWidth(200);
    speedLayout->addWidget(m_speedSlider);

    m_speedLabel = new QLabel("1.0x", this);
    m_speedLabel->setFixedWidth(50);
    speedLayout->addWidget(m_speedLabel);

    speedLayout->addStretch();

    controlsMainLayout->addLayout(speedLayout);

    layout->addWidget(m_controlsWidget);

    // Connections
    connect(m_player, &QMediaPlayer::positionChanged, this, &VideoPlayerWidget::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &VideoPlayerWidget::onDurationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &VideoPlayerWidget::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, [](QMediaPlayer::Error error, const QString& errorString) {
        qDebug() << "MediaPlayer error:" << error << errorString;
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [](QMediaPlayer::MediaStatus status) {
        qDebug() << "MediaPlayer status:" << status;
    });

    connect(m_playPauseBtn, &QPushButton::clicked, this, &VideoPlayerWidget::onPlayPauseClicked);
    connect(m_speedSlider, &QSlider::valueChanged, this, &VideoPlayerWidget::onSpeedSliderChanged);

    connect(m_seekSlider, &QSlider::sliderPressed, this, &VideoPlayerWidget::onSeekSliderPressed);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &VideoPlayerWidget::onSeekSliderReleased);
}

void VideoPlayerWidget::playUrl(const QUrl& url)
{
    qDebug() << "VideoPlayerWidget::playUrl" << url;
    showVideoMode();
    m_player->setSource(url);
    m_player->play();
}

void VideoPlayerWidget::playFile(const QString& path)
{
    qDebug() << "VideoPlayerWidget::playFile" << path;
    showVideoMode();
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();
}

void VideoPlayerWidget::showImageUrl(const QUrl& url)
{
    qDebug() << "VideoPlayerWidget::showImageUrl" << url;
    m_player->stop();
    showImageMode();
    m_imageLabel->setText("Loading...");

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "PexelManager/1.0");
    auto reply = m_imageNetwork.get(request);
    connect(reply, &QNetworkReply::finished, this, &VideoPlayerWidget::onImageLoaded);
}

void VideoPlayerWidget::showImageFile(const QString& path)
{
    qDebug() << "VideoPlayerWidget::showImageFile" << path;
    m_player->stop();
    showImageMode();

    QPixmap pixmap(path);
    if (!pixmap.isNull()) {
        m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_imageLabel->setText("Failed to load image");
    }
}

void VideoPlayerWidget::stop()
{
    m_player->stop();
    m_player->setSource(QUrl());
    m_imageLabel->clear();
}

void VideoPlayerWidget::showVideoMode()
{
    m_stack->setCurrentIndex(0);
    m_controlsWidget->show();
}

void VideoPlayerWidget::showImageMode()
{
    m_stack->setCurrentIndex(1);
    m_controlsWidget->hide();
}

void VideoPlayerWidget::onImageLoaded()
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QPixmap pixmap;
        if (pixmap.loadFromData(reply->readAll())) {
            m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_imageLabel->setText("Failed to decode image");
        }
    } else {
        m_imageLabel->setText("Failed to load: " + reply->errorString());
    }

    reply->deleteLater();
}

qreal VideoPlayerWidget::playbackRate() const
{
    return m_player->playbackRate();
}

void VideoPlayerWidget::setPlaybackRate(qreal rate)
{
    m_player->setPlaybackRate(rate);
    m_speedSlider->setValue(static_cast<int>(rate * 10));
}

void VideoPlayerWidget::onPositionChanged(qint64 position)
{
    if (!m_seeking) {
        m_seekSlider->setValue(static_cast<int>(position));
    }

    qint64 duration = m_player->duration();
    m_timeLabel->setText(QString("%1 / %2")
        .arg(formatTime(position))
        .arg(formatTime(duration)));
}

void VideoPlayerWidget::onDurationChanged(qint64 duration)
{
    m_seekSlider->setRange(0, static_cast<int>(duration));
}

void VideoPlayerWidget::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    m_playPauseBtn->setText(state == QMediaPlayer::PlayingState ? "Pause" : "Play");
}

void VideoPlayerWidget::onSpeedSliderChanged(int value)
{
    qreal rate = value / 10.0;
    m_player->setPlaybackRate(rate);
    m_speedLabel->setText(QString("%1x").arg(rate, 0, 'f', 1));
    emit playbackRateChanged(rate);
}

void VideoPlayerWidget::onPlayPauseClicked()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void VideoPlayerWidget::onSeekSliderPressed()
{
    m_seeking = true;
}

void VideoPlayerWidget::onSeekSliderReleased()
{
    m_player->setPosition(m_seekSlider->value());
    m_seeking = false;
}

QString VideoPlayerWidget::formatTime(qint64 ms) const
{
    int secs = static_cast<int>(ms / 1000);
    int mins = secs / 60;
    secs = secs % 60;
    return QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'));
}
