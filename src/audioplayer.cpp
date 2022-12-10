// audioplayer.cpp
//
// Copyright (C) 2017-2022 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QtGlobal>

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>
#endif

#include <QAudioOutput>
#include <QFileInfo>
#include <QObject>
#include <QMediaPlayer>

#include <dirent.h>
#include <string.h>

#include "audioplayer.h"

AudioPlayer::AudioPlayer(QObject *p_Parent /* = NULL */)
  : QObject(p_Parent)
  , m_MediaPlayer(this)
{
  // Signals to media player
  connect(this, SIGNAL(Play()), &m_MediaPlayer, SLOT(play()));
  connect(this, SIGNAL(Stop()), &m_MediaPlayer, SLOT(stop()));

  // Signals from media player
  connect(&m_MediaPlayer, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
          this, SLOT(OnMediaStatusChanged(QMediaPlayer::MediaStatus)));
  connect(&m_MediaPlayer, SIGNAL(positionChanged(qint64)), this, SIGNAL(PositionChanged(qint64)));
  connect(&m_MediaPlayer, SIGNAL(durationChanged(qint64)), this, SIGNAL(DurationChanged(qint64)));
  connect(&m_MediaPlayer, SIGNAL(volumeChanged(int)), this, SIGNAL(VolumeChanged(int)));

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  QAudioDevice audioDevice(QMediaDevices::defaultAudioOutput());
  m_AudioOutput.reset(new QAudioOutput());
  m_AudioOutput->setDevice(audioDevice);
  m_MediaPlayer.setAudioOutput(m_AudioOutput.get());
#endif
}

AudioPlayer::~AudioPlayer()
{
}

void AudioPlayer::SetPlaylist(const QStringList& p_Paths)
{
  foreach (QString const &path, p_Paths)
  {
    QFileInfo fileInfo(path);
    if (fileInfo.exists())
    {
      if (fileInfo.isFile())
      {
        m_PlayListPaths.push_back(fileInfo.absoluteFilePath());
      }
      else if (fileInfo.isDir())
      {
        std::vector<std::string> files;
        ListFiles(fileInfo.absoluteFilePath().toStdString(), files);
        for (auto& file : files)
        {
          m_PlayListPaths.push_back(QString::fromStdString(file));
        }
      }
    }
  }

  emit PlaylistUpdated(m_PlayListPaths);

  if (m_Shuffle)
  {
    m_CurrentIndex = rand() % m_PlayListPaths.size();
  }

  OnMediaChanged(true /*p_Forward*/);
}

void AudioPlayer::SetPlaybackMode(bool p_Shuffle)
{
  m_Shuffle = p_Shuffle;
  emit PlaybackModeUpdated(m_Shuffle);
}

void AudioPlayer::GetPlaybackMode(bool& p_Shuffle)
{
  p_Shuffle = m_Shuffle;
}

void AudioPlayer::ToggleShuffle()
{
  m_Shuffle = !m_Shuffle;
  emit PlaybackModeUpdated(m_Shuffle);
}

void AudioPlayer::VolumeUp()
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  float volume = qBound(0.0, m_MediaPlayer.audioOutput()->volume() + 0.05, 1.0);
  m_MediaPlayer.audioOutput()->setVolume(volume);
  emit VolumeChanged((int)round(volume * 100.0));
#else
  int volume = qBound(0, m_MediaPlayer.volume() + 5, 100);
  m_MediaPlayer.setVolume(volume);
  emit VolumeChanged(volume);
#endif
}

void AudioPlayer::VolumeDown()
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  float volume = qBound(0.0, m_MediaPlayer.audioOutput()->volume() - 0.05, 1.0);
  m_MediaPlayer.audioOutput()->setVolume(volume);
  emit VolumeChanged((int)round(volume * 100.0));
#else
  int volume = qBound(0, m_MediaPlayer.volume() - 5, 100);
  m_MediaPlayer.setVolume(volume);
  emit VolumeChanged(volume);
#endif
}

void AudioPlayer::SkipBackward()
{
  m_MediaPlayer.setPosition(qBound(0ll, m_MediaPlayer.position() - 3000, m_MediaPlayer.duration()));
}

void AudioPlayer::SkipForward()
{
  m_MediaPlayer.setPosition(qBound(0ll, m_MediaPlayer.position() + 3000, m_MediaPlayer.duration()));
}

void AudioPlayer::SetVolume(int p_VolumePercentage)
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  float volume = qBound(0.0, p_VolumePercentage / 100.0, 1.0);
  m_MediaPlayer.audioOutput()->setVolume(volume);
  emit VolumeChanged((int)round(volume * 100.0));
#else
  int volume = qBound(0, p_VolumePercentage, 100);
  m_MediaPlayer.setVolume(volume);
  emit VolumeChanged(volume);
#endif
}

void AudioPlayer::GetVolume(int& p_Volume)
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  p_Volume = (int)round(m_MediaPlayer.audioOutput()->volume() * 100.0);
#else
  p_Volume = m_MediaPlayer.volume();
#endif
}

bool AudioPlayer::IsInited()
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  return (m_MediaPlayer.audioOutput() != nullptr);
#else
  return true;
#endif
}

void AudioPlayer::SetPosition(int p_PositionPercentage)
{
  m_MediaPlayer.setPosition(qBound(0ll, (p_PositionPercentage * m_MediaPlayer.duration()) / 100ll, m_MediaPlayer.duration()));
}

void AudioPlayer::OnMediaStatusChanged(QMediaPlayer::MediaStatus p_MediaStatus)
{
  if (p_MediaStatus == QMediaPlayer::EndOfMedia)
  {
    Next();
  }
}

void AudioPlayer::Pause()
{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  switch (m_MediaPlayer.playbackState())
#else
  switch (m_MediaPlayer.state())
#endif
  {
    case QMediaPlayer::PlayingState:
      m_MediaPlayer.pause();
      break;

    case QMediaPlayer::PausedState:
      m_MediaPlayer.play();
      break;

    default:
    case QMediaPlayer::StoppedState:
      break;
  }
}

void AudioPlayer::Previous()
{
  if (m_Shuffle)
  {
    if (!m_CurrentIndexHistory.empty())
    {
      m_CurrentIndex = m_CurrentIndexHistory.front();
      m_CurrentIndexHistory.removeFirst();
    }
    else
    {
      --m_CurrentIndex;
    }
  }
  else
  {
    --m_CurrentIndex;
  }

  OnMediaChanged(false /*p_Forward*/);
}

void AudioPlayer::Next()
{
  if (m_Shuffle && (m_PlayListPaths.size() > 2))
  {
    int newIndex = m_CurrentIndex;
    while (newIndex == m_CurrentIndex)
    {
      newIndex = rand() % m_PlayListPaths.size();
    }

    m_CurrentIndex = newIndex;
  }
  else
  {
    ++m_CurrentIndex;
  }
  OnMediaChanged(true /*p_Forward*/);
}

void AudioPlayer::SetCurrentIndex(int p_CurrentIndex)
{
  m_CurrentIndex = p_CurrentIndex;
  OnMediaChanged(true /*p_Forward*/);
}

void AudioPlayer::OnMediaChanged(bool p_Forward)
{
  if (m_PlayListPaths.empty()) return;

  if (m_CurrentIndex >= m_PlayListPaths.size())
  {
    m_CurrentIndex = 0;
  }
  else if (m_CurrentIndex < 0)
  {
    m_CurrentIndex = m_PlayListPaths.size() - 1;
  }

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  m_MediaPlayer.setSource(QUrl::fromLocalFile(m_PlayListPaths.at(m_CurrentIndex)));
#else
  m_MediaPlayer.setMedia(QUrl::fromLocalFile(m_PlayListPaths.at(m_CurrentIndex)));
#endif
  m_MediaPlayer.play();

  if (p_Forward)
  {
    m_CurrentIndexHistory.push_front(m_CurrentIndex);
    while (m_CurrentIndexHistory.size() > 1000)
    {
      // restrict max history kept
      m_CurrentIndexHistory.removeLast();
    }
  }

  emit CurrentIndexChanged(m_CurrentIndex);
}

void AudioPlayer::ListFiles(const std::string& p_Path, std::vector<std::string>& p_Files)
{
  DIR* dir = opendir(p_Path.c_str());
  if (!dir)
  {
    return;
  }

  struct dirent* entry = NULL;
  while ((entry = readdir(dir)))
  {
    if ((strlen(entry->d_name) == 0) || (entry->d_name[0] == '.'))
    {
      continue;
    }

    if (entry->d_type == DT_DIR)
    {
      ListFiles(p_Path + "/" + std::string(entry->d_name), p_Files);
      continue;
    }

    if (entry->d_type == DT_REG)
    {
      p_Files.push_back(p_Path + "/" + std::string(entry->d_name));
    }
  }

  closedir(dir);
}
