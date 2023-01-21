// audioplayer.h
//
// Copyright (C) 2017-2023 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QtGlobal>

#include <QObject>
#include <QMediaPlayer>

#include <string>
#include <vector>

class AudioPlayer : public QObject
{
  Q_OBJECT

public:
  AudioPlayer(QObject *parent = NULL);
  ~AudioPlayer();
  void SetPlaylist(const QStringList& paths);
  void GetPlaybackMode(bool& p_Shuffle);
  void GetVolume(int& p_Volume);
  bool IsInited();

signals:
  // Signals to media player
  void Play();
  void Stop();

  // Signals from media player
  void PositionChanged(qint64 p_Position);
  void DurationChanged(qint64 p_Position);
  void VolumeChanged(int p_Volume);

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

  void Previous();
  void Next();
  void SetCurrentIndex(int);

private slots:
  void OnMediaStatusChanged(QMediaPlayer::MediaStatus p_MediaStatus);

private:
  void OnMediaChanged(bool p_Forward);
  static void ListFiles(const std::string& p_Path, std::vector<std::string>& p_Files);
  static bool IsSupportedFileType(const QString& p_Path);

private:
  QMediaPlayer m_MediaPlayer;
  QVector<QString> m_PlayListPaths;
  bool m_Shuffle = false;
  int m_CurrentIndex = 0;
  QList<int> m_CurrentIndexHistory;
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  QScopedPointer<QAudioOutput> m_AudioOutput;
#endif
};
