#include "videoplayerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void VideoPlayerWidget::setupUi()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Video widget
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setMinimumSize(320, 200);
    layout->addWidget(m_videoWidget, 1);

    // Player setup
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(0.5);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);

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

    layout->addLayout(controlsLayout);

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

    layout->addLayout(speedLayout);

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
    m_player->setSource(url);
    m_player->play();
}

void VideoPlayerWidget::playFile(const QString& path)
{
    qDebug() << "VideoPlayerWidget::playFile" << path;
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();
}

void VideoPlayerWidget::stop()
{
    m_player->stop();
    m_player->setSource(QUrl());
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
