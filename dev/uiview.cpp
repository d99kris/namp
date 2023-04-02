// uiview.cpp
//
// Copyright (C) 2023 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QFileInfo>
#include <QObject>
#include <QTime>
#include <QTimer>
#include <QVector>

#include <locale.h>
#include <wchar.h>

#include <ncurses.h>

#include <fileref.h>
#include <tag.h>

#include "scrobbler.h"
#include "log.h"
#include "uiview.h"
#include "util.h"
#include "version.h"

UIView::UIView(QObject *p_Parent, Scrobbler* p_Scrobbler)
  : QObject(p_Parent)
  , m_Scrobbler(p_Scrobbler)
{
  printf("Namp Performance MPEG 1.0/2.0/2.5 Audio Player for Layer 1, 2, and 3.\n");
  printf("Version v" VERSION ". Written and copyright by Kristofer Berggren.\n");
}

UIView::~UIView()
{
}

void UIView::PlaylistUpdated(const QVector<QString>& p_Paths)
{
  m_Playlist.clear();
  int index = 0;
  for (const QString& trackPath : p_Paths)
  {
    m_Playlist.push_back(TrackInfo(trackPath, QFileInfo(trackPath).completeBaseName(), false, 0, index++));
  }
  m_PlaylistLoaded = false;
}

void UIView::PositionChanged(qint64 p_Position)
{
  m_TrackPositionSec = p_Position / 1000;

  if (m_Scrobbler && (m_TrackDurationSec > 0) && (m_PlaylistPosition < m_Playlist.count()))
  {
    if (m_TrackPositionSec == 0)
    {
      m_SetPlaying = false;
      m_SetPlayed = false;
      m_PlayTime.restart();
    }
    
    const qint64 elapsedSec = m_PlayTime.elapsed() / 1000;
    if (!m_SetPlayed && (elapsedSec >= 10) && (m_TrackPositionSec >= (m_TrackDurationSec / 2))) // scrobble played after 50% (min 10 sec)
    {
      const QString& artist = m_Playlist.at(m_PlaylistPosition).artist;
      const QString& title = m_Playlist.at(m_PlaylistPosition).title;
      m_Scrobbler->Played(artist, title, m_TrackDurationSec);
      m_SetPlayed = true;
    }
    else if (!m_SetPlaying && (elapsedSec >= 3)) // scrobble playing after 3 sec
    {
      const QString& artist = m_Playlist.at(m_PlaylistPosition).artist;
      const QString& title = m_Playlist.at(m_PlaylistPosition).title;
      m_Scrobbler->Playing(artist, title, m_TrackDurationSec);
      m_SetPlaying = true;
    }
  }
}

void UIView::DurationChanged(qint64 p_Position)
{
  m_TrackDurationSec = p_Position / 1000;
}

void UIView::CurrentIndexChanged(int p_Position)
{
  m_PlaylistPosition = p_Position;
  SetPlaylistSelected(p_Position, true);

  printf("\n");

  if (m_PlaylistPosition >= m_Playlist.count())
  {
    const QString trackName = "Unknown";
    printf("Playing %s\n", qPrintable(trackName));
    return;
  }

  if (!m_Playlist[m_PlaylistPosition].loaded)
  {
    TagLib::FileRef fileRef(m_Playlist[m_PlaylistPosition].path.toStdString().c_str());
    if (!fileRef.isNull() && (fileRef.tag() != NULL))
    {
      TagLib::String artist = fileRef.tag()->artist();
      TagLib::String title = fileRef.tag()->title();
      if ((artist.length() > 0) && (title.length() > 0))
      {
        m_Playlist[m_PlaylistPosition].artist = QString::fromStdString(artist.to8Bit(true));
        m_Playlist[m_PlaylistPosition].title = QString::fromStdString(title.to8Bit(true));
        m_Playlist[m_PlaylistPosition].name = m_Playlist[m_PlaylistPosition].artist + " - " + m_Playlist[m_PlaylistPosition].title;
      }
    }

    m_Playlist[m_PlaylistPosition].loaded = true;
  }

  printf("Playing %s\n", qPrintable(m_Playlist.at(m_PlaylistPosition).path));
  printf("Artist  %s\n", qPrintable(m_Playlist.at(m_PlaylistPosition).artist));
  printf("Title   %s\n", qPrintable(m_Playlist.at(m_PlaylistPosition).title));
}

void UIView::VolumeChanged(int /*p_Volume*/)
{
}

void UIView::PlaybackModeUpdated(bool /*p_Shuffle*/)
{
}

void UIView::Search()
{
}

void UIView::SelectPrevious()
{
}

void UIView::SelectNext()
{
}

void UIView::PagePrevious()
{
}

void UIView::PageNext()
{
}

void UIView::Home()
{
}

void UIView::End()
{
}

void UIView::PlaySelected()
{
}

void UIView::ToggleWindow()
{
}

void UIView::SetUIState(UIState /*p_UIState*/)
{
}

void UIView::Timer()
{
}

void UIView::Refresh()
{
}

void UIView::UpdateScreen()
{
}

void UIView::DeleteWindows()
{
}

void UIView::CreateWindows()
{
}

void UIView::DrawPlayer()
{
}

QString UIView::GetPlayerTrackName(int /*p_MaxLength*/)
{
  QString trackName;
  return trackName;
}

void UIView::KeyPress(int /*p_Key*/) // can move this to other slots later.
{
}

void UIView::DrawPlaylist()
{
}

void UIView::MouseEventRequest(int /*p_X*/, int /*p_Y*/, uint32_t /*p_Button*/)
{
}

void UIView::LoadTracksData()
{
}

void UIView::SetPlaylistSelected(int /*p_SelectedTrack*/, bool /*p_UpdateOffset*/)
{
}

void UIView::GetScrollTitle(bool& p_ScrollTitle)
{
  p_ScrollTitle = m_ScrollTitle;
}

void UIView::SetScrollTitle(const bool& p_ScrollTitle)
{
  m_ScrollTitle = p_ScrollTitle;
}

void UIView::GetViewPosition(bool& p_ViewPosition)
{
  p_ViewPosition = m_ViewPosition;
}

void UIView::SetViewPosition(const bool& p_ViewPosition)
{
  m_ViewPosition = p_ViewPosition;
}
