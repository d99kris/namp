// audioplayer.cpp
//
// Copyright (C) 2017-2019 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QFileInfo>
#include <QObject>
#include <QMediaPlayer>
#include <QMediaPlaylist>

#include <dirent.h>
#include <string.h>

#include "audioplayer.h"

AudioPlayer::AudioPlayer(QObject *p_Parent /* = NULL */)
  : QObject(p_Parent)
  , m_MediaPlaylist(this)
  , m_MediaPlayer(this)
{
  m_MediaPlayer.setPlaylist(&m_MediaPlaylist);

  // Signals to media player
  connect(this, SIGNAL(Play()), &m_MediaPlayer, SLOT(play()));
  connect(this, SIGNAL(Stop()), &m_MediaPlayer, SLOT(stop()));

  // Signals from media player
  connect(&m_MediaPlayer, SIGNAL(positionChanged(qint64)), this, SIGNAL(PositionChanged(qint64)));
  connect(&m_MediaPlayer, SIGNAL(durationChanged(qint64)), this, SIGNAL(DurationChanged(qint64)));
  connect(&m_MediaPlayer, SIGNAL(volumeChanged(int)), this, SIGNAL(VolumeChanged(int)));
    
  // Signals to media playlist
  connect(this, SIGNAL(Previous()), &m_MediaPlaylist, SLOT(previous()));
  connect(this, SIGNAL(Next()), &m_MediaPlaylist, SLOT(next()));
  connect(this, SIGNAL(SetCurrentIndex(int)), &m_MediaPlaylist, SLOT(setCurrentIndex(int)));

  // Signals from media playlist
  connect(&m_MediaPlaylist, SIGNAL(playbackModeChanged(QMediaPlaylist::PlaybackMode)), this, SLOT(OnPlaybackModeChanged(QMediaPlaylist::PlaybackMode)));
  connect(&m_MediaPlaylist, SIGNAL(currentIndexChanged(int)), this, SIGNAL(CurrentIndexChanged(int)));
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
        QUrl url = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
        m_MediaPlaylist.addMedia(url);
      }
      else if (fileInfo.isDir())
      {
        std::vector<std::string> files;
        ListFiles(fileInfo.absoluteFilePath().toStdString(), files);
        for (auto& file : files)
        {
          QUrl url = QUrl::fromLocalFile(QString::fromStdString(file));
          m_MediaPlaylist.addMedia(url);
        }
      }
    }
  }
  emit OnMediaPlaylistChanged();
}

void AudioPlayer::SetPlaybackMode(bool p_Shuffle)
{
  m_MediaPlaylist.setPlaybackMode(p_Shuffle ? QMediaPlaylist::Random : QMediaPlaylist::Loop);
}

void AudioPlayer::GetPlaybackMode(bool& p_Shuffle)
{
  p_Shuffle = (m_MediaPlaylist.playbackMode() == QMediaPlaylist::Random);
}

void AudioPlayer::ToggleShuffle()
{
  SetPlaybackMode(!(m_MediaPlaylist.playbackMode() == QMediaPlaylist::Random));
}

void AudioPlayer::VolumeUp()
{
  m_MediaPlayer.setVolume(qBound(0, m_MediaPlayer.volume() + 5, 100));
}

void AudioPlayer::VolumeDown()
{
  m_MediaPlayer.setVolume(qBound(0, m_MediaPlayer.volume() - 5, 100));
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
  m_MediaPlayer.setVolume(qBound(0, p_VolumePercentage, 100));
}

void AudioPlayer::GetVolume(int& p_Volume)
{
  p_Volume = m_MediaPlayer.volume();
}

void AudioPlayer::SetPosition(int p_PositionPercentage)
{
  m_MediaPlayer.setPosition(qBound(0ll, (p_PositionPercentage * m_MediaPlayer.duration()) / 100ll, m_MediaPlayer.duration()));
}

void AudioPlayer::OnMediaPlaylistChanged()
{
  QVector<QString> paths;
  int count = m_MediaPlaylist.mediaCount();
  for (int index = 0; index < count; ++index)
  {
    paths.push_back(m_MediaPlaylist.media(index).canonicalUrl().toLocalFile());
  }
  emit PlaylistUpdated(paths);
}

void AudioPlayer::OnPlaybackModeChanged(QMediaPlaylist::PlaybackMode p_Mode)
{
  emit PlaybackModeUpdated(p_Mode == QMediaPlaylist::Random);
}

void AudioPlayer::Pause()
{
  switch (m_MediaPlayer.state())
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
