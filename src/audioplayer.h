// audioplayer.h
//
// Copyright (C) 2017 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QMediaPlaylist>

class AudioPlayer : public QObject
{
  Q_OBJECT

public:
  AudioPlayer(QObject *parent = NULL);
  ~AudioPlayer();
  void SetPlaylist(const QStringList& paths);
  void GetPlaybackMode(bool& p_Shuffle);
  void GetVolume(int& p_Volume);

signals:
  // Signals to media player
  void Play();
  void Stop();

  // Signals from media player
  void PositionChanged(qint64 p_Position);
  void DurationChanged(qint64 p_Position);
  void VolumeChanged(int p_Volume);

  // Signals to media playlist
  void Previous();
  void Next();
  void SetCurrentIndex(int);

  // Signals from audio player
  void PlaylistUpdated(const QVector<QString>& paths);
  void CurrentIndexChanged(int p_Position);
  void PlaybackModeUpdated(bool p_Shuffle);

public slots:
  void SetPlaybackMode(bool p_Shuffle);
  void ToggleShuffle();
  void VolumeUp();
  void VolumeDown();
  void SkipBackward();
  void SkipForward();
  void SetVolume(int);
  void SetPosition(int);
  void Pause();

private slots:
  void OnMediaPlaylistChanged();
  void OnPlaybackModeChanged(QMediaPlaylist::PlaybackMode p_Mode);

private:
  QMediaPlaylist m_MediaPlaylist;
  QMediaPlayer m_MediaPlayer;
};

