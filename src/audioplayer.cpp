// audioplayer.cpp
//
// Copyright (C) 2017-2026 Kristofer Berggren
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
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QMediaPlayer>

#include <dirent.h>
#include <string.h>

#include "audioplayer.h"
#include "log.h"
#include "util.h"

AudioPlayer::AudioPlayer(QObject *p_Parent /* = NULL */)
  : QObject(p_Parent)
  , m_MediaPlayer(this)
{
  // (Play and Stop are now slots, not signals forwarded to QMediaPlayer)

  // Signals from media player
  connect(&m_MediaPlayer, &QMediaPlayer::mediaStatusChanged, this, &AudioPlayer::OnMediaStatusChanged);
  connect(&m_MediaPlayer, &QMediaPlayer::positionChanged, this, &AudioPlayer::PositionChanged);
  connect(&m_MediaPlayer, &QMediaPlayer::durationChanged, this, &AudioPlayer::DurationChanged);
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  connect(&m_MediaPlayer, &QMediaPlayer::errorOccurred, this, &AudioPlayer::OnErrorOccurred);
#else
  connect(&m_MediaPlayer, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this, &AudioPlayer::OnErrorOccurred);
#endif

  // Spectrum analyzer
  m_Spectrum = new Spectrum([this]() { return m_MediaPlayer.position(); }, this);
  connect(m_Spectrum, &Spectrum::SpectrumChanged, this, &AudioPlayer::SpectrumChanged);

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  QAudioDevice audioDevice(QMediaDevices::defaultAudioOutput());
  m_AudioOutput.reset(new QAudioOutput());
  m_AudioOutput->setDevice(audioDevice);
  m_MediaPlayer.setAudioOutput(m_AudioOutput.get());
  connect(&m_MediaDevices, &QMediaDevices::audioOutputsChanged, this, &AudioPlayer::OnAudioOutputsChanged);
#endif
}

AudioPlayer::~AudioPlayer()
{
}

void AudioPlayer::Shutdown()
{
  m_Spectrum->Stop();
  m_MediaPlayer.stop();
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  m_MediaPlayer.setSource(QUrl());
#else
  m_MediaPlayer.setMedia(QMediaContent());
#endif
}

void AudioPlayer::SetPlaylist(const QStringList& p_Paths, const QString& p_CurrentTrack)
{
  foreach (QString const &path, p_Paths)
  {
    QFileInfo fileInfo(path);
    if (fileInfo.exists())
    {
      if (fileInfo.isFile())
      {
        const QString filePath = fileInfo.absoluteFilePath();
        if (!IsSupportedFileType(filePath)) continue;

        m_PlayListPaths.push_back(filePath);
      }
      else if (fileInfo.isDir())
      {
        std::vector<std::string> files;
        ListFiles(QDir::cleanPath(fileInfo.absoluteFilePath()).toStdString(), files);

        std::sort(std::begin(files), std::end(files),
                  [](const std::string& a, const std::string& b)
        {
          // Sort files before subdirectories at every directory level by
          // replacing the last '/' (before the filename) with '\x01',
          // which sorts before '/' (0x2F) in directory paths.
          std::string ka = a, kb = b;
          auto la = ka.rfind('/'); if (la != std::string::npos) ka[la] = '\x01';
          auto lb = kb.rfind('/'); if (lb != std::string::npos) kb[lb] = '\x01';
          return ka < kb;
        });

        for (auto& file : files)
        {
          const QString filePath = QString::fromStdString(file);
          if (!IsSupportedFileType(filePath)) continue;

          m_PlayListPaths.push_back(filePath);
        }
      }
    }
  }

  emit PlaylistUpdated(m_PlayListPaths);

  const int currentIndex = m_PlayListPaths.indexOf(p_CurrentTrack);
  if (currentIndex != -1)
  {
    m_CurrentIndex = currentIndex;
  }
  else if (m_Shuffle)
  {
    m_CurrentIndex = rand() % m_PlayListPaths.size();
  }
  else
  {
    m_CurrentIndex = 0;
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

void AudioPlayer::ExternalEdit(int p_SelectedIndex)
{
  const bool editingCurrent = (p_SelectedIndex == m_CurrentIndex);
  if (editingCurrent)
  {
    // Release the file handle so idntag can safely rewrite it, and so the
    // decoder is not reading shifted byte offsets while tags are in flux.
    m_MediaPlayer.stop();
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
    m_MediaPlayer.setSource(QUrl());
#else
    m_MediaPlayer.setMedia(QMediaContent());
#endif
  }

  QString selectedTrackPath = m_PlayListPaths.at(p_SelectedIndex);
  QString cmd = "idntag --edit --report \"\" \"" + selectedTrackPath + "\"";
  bool result = Util::RunProgram(cmd.toStdString());

  if (editingCurrent)
  {
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
    m_MediaPlayer.setSource(QUrl::fromLocalFile(selectedTrackPath));
#else
    m_MediaPlayer.setMedia(QUrl::fromLocalFile(selectedTrackPath));
#endif
    m_MediaPlayer.play();
  }

  if (result)
  {
    emit RefreshTrackData(p_SelectedIndex);
#ifdef HAS_GUI
    if (editingCurrent)
    {
      emit RefreshLyrics(m_CurrentTrack);
    }
#endif
  }
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
  m_MediaPlayer.pause();
  m_MediaPlayer.setPosition(qBound(0ll, m_MediaPlayer.position() - 3000, m_MediaPlayer.duration()));
  m_MediaPlayer.play();
  m_Spectrum->SetPaused(false);
}

void AudioPlayer::SkipForward()
{
  m_MediaPlayer.pause();
  m_MediaPlayer.setPosition(qBound(0ll, m_MediaPlayer.position() + 3000, m_MediaPlayer.duration()));
  m_MediaPlayer.play();
  m_Spectrum->SetPaused(false);
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

void AudioPlayer::GetCurrentTrack(QString& p_CurrentTrack)
{
  p_CurrentTrack = m_CurrentTrack;
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
  static const char* statusNames[] = {
    "UnknownMediaStatus", "NoMedia", "LoadingMedia", "LoadedMedia",
    "StalledMedia", "BufferingMedia", "BufferedMedia", "EndOfMedia", "InvalidMedia"
  };
  const int idx = static_cast<int>(p_MediaStatus);
  const char* statusName = (idx >= 0 && idx <= 8) ? statusNames[idx] : "Unknown";
  Log::Debug("OnMediaStatusChanged: %s (%d) track=%s", statusName, idx,
             m_CurrentTrack.toStdString().c_str());

  if (p_MediaStatus == QMediaPlayer::EndOfMedia)
  {
    Next();
  }
  else if (p_MediaStatus == QMediaPlayer::InvalidMedia)
  {
    Log::Warning("InvalidMedia for track=%s error=%s",
                 m_CurrentTrack.toStdString().c_str(),
                 m_MediaPlayer.errorString().toStdString().c_str());
    Next();
  }
}

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
void AudioPlayer::OnErrorOccurred(QMediaPlayer::Error p_Error, const QString& p_ErrorString)
{
  Log::Warning("QMediaPlayer error %d: %s (track=%s)", static_cast<int>(p_Error),
               p_ErrorString.toStdString().c_str(), m_CurrentTrack.toStdString().c_str());
}

void AudioPlayer::OnAudioOutputsChanged()
{
  QAudioDevice newDevice = QMediaDevices::defaultAudioOutput();
  if (newDevice.isNull())
  {
    Log::Warning("Audio output device lost, no available output devices");
    m_MediaPlayer.stop();
    return;
  }

  Log::Info("Audio output device changed, switching to: %s",
            newDevice.description().toStdString().c_str());
  m_AudioOutput->setDevice(newDevice);

  // Re-start playback on the new device if we were playing
  if (m_MediaPlayer.playbackState() == QMediaPlayer::PlayingState)
  {
    qint64 pos = m_MediaPlayer.position();
    m_MediaPlayer.stop();
    m_MediaPlayer.setSource(QUrl::fromLocalFile(m_CurrentTrack));
    m_MediaPlayer.setPosition(pos);
    m_MediaPlayer.play();
  }
}
#else
void AudioPlayer::OnErrorOccurred(QMediaPlayer::Error p_Error)
{
  Log::Warning("QMediaPlayer error %d: %s (track=%s)", static_cast<int>(p_Error),
               m_MediaPlayer.errorString().toStdString().c_str(),
               m_CurrentTrack.toStdString().c_str());
}
#endif

void AudioPlayer::Play()
{
  m_MediaPlayer.play();
  m_Spectrum->SetPaused(false);
  if (m_Spectrum->IsRunning())
  {
    m_Spectrum->StartTrack(m_CurrentTrack);
  }
}

void AudioPlayer::Stop()
{
  m_MediaPlayer.stop();
  m_Spectrum->StartDecay();
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
      m_Spectrum->SetPaused(true);
      break;

    case QMediaPlayer::PausedState:
      m_MediaPlayer.play();
      m_Spectrum->SetPaused(false);
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
  Log::Debug("Next() called, currentIndex=%d", m_CurrentIndex);
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

void AudioPlayer::SetAnalyzerEnabled(bool p_Enabled)
{
  if (p_Enabled)
  {
    m_Spectrum->StartTrack(m_CurrentTrack);
  }
  else
  {
    m_Spectrum->Stop();
  }
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

  m_CurrentTrack = m_PlayListPaths.at(m_CurrentIndex);
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  m_MediaPlayer.setSource(QUrl::fromLocalFile(m_CurrentTrack));
#else
  m_MediaPlayer.setMedia(QUrl::fromLocalFile(m_CurrentTrack));
#endif
  m_MediaPlayer.play();

  if (m_Spectrum->IsRunning())
  {
    m_Spectrum->StartTrack(m_CurrentTrack);
  }

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
#ifdef HAS_GUI
  emit TrackChanged(m_CurrentTrack);
#endif
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

bool AudioPlayer::IsSupportedFileType(const QString& p_Path)
{
  if (p_Path.endsWith(".cdg", Qt::CaseInsensitive) ||
      p_Path.endsWith(".lrc", Qt::CaseInsensitive) ||
      p_Path.endsWith(".m3u", Qt::CaseInsensitive) ||
      p_Path.endsWith(".md", Qt::CaseInsensitive) ||
      p_Path.endsWith(".txt", Qt::CaseInsensitive) ||
      p_Path.endsWith(".zip", Qt::CaseInsensitive))
  {
    return false;
  }

  return true;
}
