#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QAudioOutput>

class VideoPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayerWidget(QWidget* parent = nullptr);

    void playUrl(const QUrl& url);
    void playFile(const QString& path);
    void stop();

    qreal playbackRate() const;
    void setPlaybackRate(qreal rate);

signals:
    void playbackRateChanged(qreal rate);

private slots:
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onSpeedSliderChanged(int value);
    void onPlayPauseClicked();
    void onSeekSliderPressed();
    void onSeekSliderReleased();

private:
    void setupUi();
    QString formatTime(qint64 ms) const;

    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;
    QVideoWidget* m_videoWidget;

    QSlider* m_seekSlider;
    QSlider* m_speedSlider;
    QLabel* m_timeLabel;
    QLabel* m_speedLabel;
    QPushButton* m_playPauseBtn;

    bool m_seeking = false;
};
