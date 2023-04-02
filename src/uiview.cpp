// uiview.cpp
//
// Copyright (C) 2017-2023 Kristofer Berggren
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

static int s_MinTerminalWidth = 40;
static int s_MinTerminalHeight = 6;
static int s_SearchWidthPad = 4;

UIView::UIView(QObject *p_Parent, Scrobbler* p_Scrobbler)
  : QObject(p_Parent)
  , m_Scrobbler(p_Scrobbler)
  , m_PlayerWindowWidth(s_MinTerminalWidth)
  , m_PlayerWindowHeight(s_MinTerminalHeight)
  , m_PlaylistWindowWidth(s_MinTerminalWidth)
  , m_PlaylistWindowMinHeight(6)
{
  std::string title = "namp";
  printf("\033]0;%s\007", title.c_str());

  setlocale(LC_ALL, "");
  initscr();
  noecho();

  QTimer::singleShot(0, this, &UIView::Timer);
  m_Timer = new QTimer();
  connect(m_Timer, &QTimer::timeout, this, &UIView::Timer);
  m_Timer->setInterval(1000);
  m_Timer->start();
}

UIView::~UIView()
{
  if (m_Timer != NULL)
  {
    m_Timer->stop();
    delete m_Timer;
    m_Timer = NULL;
  }

  wclear(stdscr);
  DeleteWindows();
  endwin();

  printf("\033]0;%s\007", "");
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
  Refresh();
}

void UIView::PositionChanged(qint64 p_Position)
{
  m_TrackPositionSec = p_Position / 1000;
  Refresh();

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
  Refresh();
}

void UIView::CurrentIndexChanged(int p_Position)
{
  m_PlaylistPosition = p_Position;
  SetPlaylistSelected(p_Position, true);
  Refresh();
}

void UIView::VolumeChanged(int p_Volume)
{
  m_VolumePercentage = p_Volume;
  Refresh();
}

void UIView::PlaybackModeUpdated(bool p_Shuffle)
{
  m_Shuffle = p_Shuffle;
  Refresh();
}

void UIView::Search()
{
  SetUIState(UISTATE_SEARCH);
  Refresh();
}

void UIView::SelectPrevious()
{
  SetPlaylistSelected((m_PlaylistSelected - 1), true);
  Refresh();
}

void UIView::SelectNext()
{
  SetPlaylistSelected((m_PlaylistSelected + 1), true);
  Refresh();
}

void UIView::PagePrevious()
{
  const int viewMax = m_PlaylistWindowHeight - 2;
  SetPlaylistSelected((m_PlaylistSelected - viewMax), true);
  Refresh();
}

void UIView::PageNext()
{
  const int viewMax = m_PlaylistWindowHeight - 2;
  SetPlaylistSelected((m_PlaylistSelected + viewMax), true);
  Refresh();
}

void UIView::Home()
{
  SetPlaylistSelected(0, true);
  Refresh();
}

void UIView::End()
{
  SetPlaylistSelected((m_Playlist.count() - 1), true);
  Refresh();
}

void UIView::PlaySelected()
{
  emit SetCurrentIndex(m_PlaylistSelected);
  emit Play();
}

void UIView::ToggleWindow()
{
  switch (m_UIState)
  {
    case UISTATE_PLAYER:
      SetUIState(UISTATE_PLAYLIST);
      break;
            
    case UISTATE_PLAYLIST:
      SetUIState(UISTATE_PLAYER);
      break;

    default:
      break;
  }

  Refresh();
}

void UIView::SetUIState(UIState p_UIState)
{
  m_PreviousUIState = m_UIState;
  m_UIState = p_UIState;
  if (m_UIState & UISTATE_SEARCH)
  {
    m_SearchString = "";
    m_SearchStringPos = 0;
    SetPlaylistSelected(0, true);
  }
  else if (m_PreviousUIState & UISTATE_SEARCH)
  {
    SetPlaylistSelected(m_PlaylistPosition, true);
  }

  emit UIStateUpdated(m_UIState);
}

void UIView::Timer()
{
  LoadTracksData();
  Refresh();
}

void UIView::Refresh()
{
  UpdateScreen();
  DrawPlayer();
  DrawPlaylist();
}

void UIView::UpdateScreen()
{
  int terminalWidth = -1;
  int terminalHeight = -1;
  getmaxyx(stdscr, terminalHeight, terminalWidth);
  if ((terminalWidth != m_TerminalWidth) || (terminalHeight != m_TerminalHeight))
  {
    m_TerminalWidth = terminalWidth;
    m_TerminalHeight = terminalHeight;

    DeleteWindows();
    CreateWindows();
  }
}

void UIView::DeleteWindows()
{
  wclear(stdscr);

  if (m_PlayerWindow != NULL)
  {
    wclear(m_PlayerWindow);
    wrefresh(m_PlayerWindow);
    delwin(m_PlayerWindow);
    m_PlayerWindow = NULL;
  }

  if (m_PlaylistWindow != NULL)
  {
    wclear(m_PlaylistWindow);
    wrefresh(m_PlaylistWindow);
    delwin(m_PlaylistWindow);
    m_PlaylistWindow = NULL;
  }
}

void UIView::CreateWindows()
{
  if (m_TerminalHeight >= (s_MinTerminalHeight * 2))
  {
    // Playlist can fit under player window (first option)

    int idealWindowWidth = std::max(s_MinTerminalWidth, m_TerminalWidth);
    m_PlayerWindowWidth = idealWindowWidth;
    m_PlaylistWindowWidth = idealWindowWidth;
    m_PlayerWindow = newwin(m_PlayerWindowHeight, m_PlayerWindowWidth, m_PlayerWindowY, m_PlayerWindowX);

    m_PlaylistWindowHeight = m_TerminalHeight - m_PlayerWindowHeight;
    m_PlaylistWindowX = 0;
    m_PlaylistWindowY = m_PlayerWindowHeight;
    m_PlaylistWindow = newwin(m_PlaylistWindowHeight, m_PlaylistWindowWidth, m_PlaylistWindowY, m_PlaylistWindowX);
  }
  else if (m_TerminalWidth >= 80)
  {
    // Playlist can fit on the right side of player window (second option)

    int idealWindowWidth = std::max(s_MinTerminalWidth, m_TerminalWidth / 2);
    m_PlayerWindowWidth = idealWindowWidth;
    m_PlaylistWindowWidth = idealWindowWidth + (m_TerminalWidth % 2);
    m_PlayerWindow = newwin(m_PlayerWindowHeight, m_PlayerWindowWidth, m_PlayerWindowY, m_PlayerWindowX);

    m_PlaylistWindowHeight = m_PlayerWindowHeight;
    m_PlaylistWindowX = m_PlayerWindowWidth;
    m_PlaylistWindowY = 0;
    m_PlaylistWindow = newwin(m_PlaylistWindowHeight, m_PlaylistWindowWidth, m_PlaylistWindowY, m_PlaylistWindowX);
  }
  else
  {
    // Disable playlist if it cannot fit (last option)

    int idealWindowWidth = std::max(s_MinTerminalWidth, m_TerminalWidth);
    m_PlayerWindowWidth = idealWindowWidth;
    m_PlayerWindow = newwin(m_PlayerWindowHeight, m_PlayerWindowWidth, m_PlayerWindowY, m_PlayerWindowX);

    m_PlaylistWindow = NULL;
  }

  m_TitleWidth = m_PlayerWindowWidth - 13;;
  m_VolumeWidth = m_PlayerWindowWidth - 15;
  m_PositionWidth = m_PlayerWindowWidth - 6;
}

void UIView::DrawPlayer()
{
  if (m_PlayerWindow != NULL)
  {
    // Border and title
    wborder(m_PlayerWindow, 0, 0, 0, 0, 0, 0, 0, 0);
    const int titleAttributes = (m_UIState == UISTATE_PLAYER) ? A_BOLD : A_NORMAL;
    wattron(m_PlayerWindow, titleAttributes);
    const int titlePos = (m_PlayerWindowWidth - 6) / 2;
    mvwprintw(m_PlayerWindow, 0, titlePos, " namp ");
    wattroff(m_PlayerWindow, titleAttributes);

    // Track position
    if (m_ViewPosition)
    {
      mvwprintw(m_PlayerWindow, 1, 3, " %02d:%02d", (m_TrackPositionSec / 60), (m_TrackPositionSec % 60));
    }
    else
    {
      mvwprintw(m_PlayerWindow, 1, 3, "      ");
    }
        
    // Track title
    mvwprintw(m_PlayerWindow, 1, 11, "%.*s", -m_TitleWidth, "");
    std::string fullName = GetPlayerTrackName(m_TitleWidth).toStdString();
    std::wstring trackName = Util::TrimPadWString(Util::ToWString(fullName), m_TitleWidth);
    mvwaddnwstr(m_PlayerWindow, 1, 11, trackName.c_str(), trackName.size());

    // Volume
    mvwprintw(m_PlayerWindow, 2, 11, "-%*c+", m_VolumeWidth, ' ');
    mvwhline(m_PlayerWindow, 2, 12, 0, (m_VolumeWidth * m_VolumePercentage) / 100);

    // Progress
    mvwprintw(m_PlayerWindow, 3, 2, "|%*c|", m_PositionWidth, ' ');
    if(m_TrackDurationSec != 0)
    {
      mvwhline(m_PlayerWindow, 3, 3, 0, (m_PositionWidth * m_TrackPositionSec) / m_TrackDurationSec);
    }

    // Playback controls
    char state = ' ';
    if (m_SetPlayed)
    {
      state = '^';
    }
    else if (m_SetPlaying)
    {
      state = '_';
    }
    mvwprintw(m_PlayerWindow, 4, 2, "|< |> || [] >|  [%c] Shuffle  %c", m_Shuffle ? 'X' : ' ', state);

    // Refresh
    wrefresh(m_PlayerWindow);
  }
}

QString UIView::GetPlayerTrackName(int p_MaxLength)
{
  QString trackName;
  if (m_PlaylistPosition < m_Playlist.count())
  {
    char position[10];
    snprintf(position, sizeof(position), "(%d:%02d)", (m_TrackDurationSec / 60), (m_TrackDurationSec % 60));
    trackName = m_Playlist.at(m_PlaylistPosition).name + " " + position;
  }

  const int trackNameLen = Util::WStringWidth(Util::ToWString(trackName.toStdString()));
  if (trackNameLen > p_MaxLength)
  {
    if (m_ScrollTitle)
    {
      // Scroll track names that cannot fit
      static QElapsedTimer lastUpdateTime;
      static int lastPlaylistPosition = -1;
      static int nextUpdateAtElapsed = 0;
      static int trackScrollOffset = 0;

      if (lastPlaylistPosition != m_PlaylistPosition)
      {
        // When track changed, hold title for 4 secs
        lastUpdateTime.start();
        trackScrollOffset = 0;
        nextUpdateAtElapsed = 3900;
        lastPlaylistPosition = m_PlaylistPosition;
      }
      else if (lastUpdateTime.elapsed() > nextUpdateAtElapsed)
      {
        // When timer elapsed, increment scroll position
        lastUpdateTime.start();
        const int maxScrollOffset = trackNameLen - p_MaxLength - 1;
        if (trackScrollOffset < maxScrollOffset)
        {
          // During scroll, view each offset for 1 sec
          nextUpdateAtElapsed = 900;
          ++trackScrollOffset;
        }
        else if (trackScrollOffset == maxScrollOffset)
        {
          // At end of scroll, hold title for 4 secs
          nextUpdateAtElapsed = 3900;
          ++trackScrollOffset;
        }
        else
        {
          trackScrollOffset = 0;
        }
      }

      trackName = trackName.mid(trackScrollOffset, p_MaxLength);
    }
    else
    {
      trackName = trackName.left(p_MaxLength);
    }
  }
    
  return trackName;
}

void UIView::KeyPress(int p_Key) // can move this to other slots later.
{
  switch (p_Key)
  {
    case '\n':
      if (m_PlaylistSelected < m_Resultlist.length())
      {
        emit SetCurrentIndex(m_Resultlist.at(m_PlaylistSelected).index);
        emit Play();
      }
      SetUIState(m_PreviousUIState);
      break;

    case KEY_LEFT:
      m_SearchStringPos = qBound(0, m_SearchStringPos - 1, m_SearchString.length());
      break;

    case KEY_RIGHT:
      m_SearchStringPos = qBound(0, m_SearchStringPos + 1, m_SearchString.length());
      break;

    case KEY_UP:
      SetPlaylistSelected(qBound(0, (m_PlaylistSelected - 1), (m_Resultlist.count() - 1)), true);
      break;

    case KEY_DOWN:
      SetPlaylistSelected(qBound(0, (m_PlaylistSelected + 1), (m_Resultlist.count() - 1)), true);
      break;

    case KEY_PPAGE:
      {
        const int viewMax = m_PlaylistWindowHeight - 2;
        SetPlaylistSelected(qBound(0, (m_PlaylistSelected - viewMax), (m_Resultlist.count() - 1)), true);
      }
      break;

    case KEY_NPAGE:
      {
        const int viewMax = m_PlaylistWindowHeight - 2;
        SetPlaylistSelected(qBound(0, (m_PlaylistSelected + viewMax), (m_Resultlist.count() - 1)), true);
      }
      break;

    case KEY_HOME:
      SetPlaylistSelected(0, true);
      break;

    case KEY_END:
      SetPlaylistSelected((m_Resultlist.count() - 1), true);
      break;

#ifdef __APPLE__
    case 127:
#endif
    case KEY_BACKSPACE:
      if (m_SearchStringPos > 0)
      {
        m_SearchString.remove(--m_SearchStringPos, 1);
      }
      break;

    case 27:
      SetUIState(m_PreviousUIState);
      break;

    default:
      if (ispunct(p_Key) || isalnum(p_Key) || (p_Key == ' '))
      {
        if (m_SearchString.length() < (m_PlaylistWindowWidth - s_SearchWidthPad))
        {
          m_SearchString.insert(m_SearchStringPos++, QChar(p_Key));
        }
        else
        {
          flash();
        }
      }
      break;
  }

  Refresh();
}

void UIView::DrawPlaylist()
{
  if (m_PlaylistWindow != NULL)
  {
    // Border
    wborder(m_PlaylistWindow, 0, 0, 0, 0, 0, 0, 0, 0);

    if (m_UIState & (UISTATE_PLAYER | UISTATE_PLAYLIST))
    {
      // Title
      const int titleAttributes = (m_UIState & (UISTATE_PLAYLIST | UISTATE_SEARCH)) ? A_BOLD : A_NORMAL;
      wattron(m_PlaylistWindow, titleAttributes);
      const int titlePos = (m_PlaylistWindowWidth - 10) / 2;
      mvwprintw(m_PlaylistWindow, 0, titlePos, " playlist ");
      wattroff(m_PlaylistWindow, titleAttributes);

      // Track list
      const int viewMax = m_PlaylistWindowHeight - 2;
      const int viewCount = qBound(0, m_Playlist.count(), viewMax);
      for (int i = 0; i < viewCount; ++i)
      {
        const int playlistIndex = i + m_PlaylistOffset;
        const int viewLength = m_PlaylistWindowWidth - 4;
        std::string fullName = m_Playlist.at(playlistIndex).name.toStdString();
        std::wstring trackName = Util::TrimPadWString(Util::ToWString(fullName), viewLength);
        wattron(m_PlaylistWindow, (playlistIndex == m_PlaylistSelected) ? A_REVERSE : A_NORMAL);
        std::wstring spaces(viewLength, L' ');
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, spaces.c_str(), spaces.size());
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, trackName.c_str(), trackName.size());
        wattroff(m_PlaylistWindow, (playlistIndex == m_PlaylistSelected) ? A_REVERSE : A_NORMAL);
      }
    }
    else
    {
      // Refresh search result list
      m_Resultlist.clear();
      for (const TrackInfo& trackInfo : m_Playlist)
      {
        if (trackInfo.path.contains(m_SearchString, Qt::CaseInsensitive) ||
            trackInfo.name.contains(m_SearchString, Qt::CaseInsensitive))
        {
          m_Resultlist.push_back(trackInfo);
        }
      }

      // Track list
      const int viewMax = m_PlaylistWindowHeight - 2;
      const int viewCount = qBound(0, m_Resultlist.count(), viewMax);
      for (int i = 0; i < viewCount; ++i)
      {
        const int playlistIndex = i + m_PlaylistOffset;
        const int viewLength = m_PlaylistWindowWidth - 4;
        std::string fullName = m_Resultlist.at(playlistIndex).name.toStdString();
        std::wstring trackName = Util::TrimPadWString(Util::ToWString(fullName), viewLength);
        wattron(m_PlaylistWindow, (playlistIndex == m_PlaylistSelected) ? A_REVERSE : A_NORMAL);
        std::wstring spaces(viewLength, L' ');
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, spaces.c_str(), spaces.size());
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, trackName.c_str(), trackName.size());
        wattroff(m_PlaylistWindow, (playlistIndex == m_PlaylistSelected) ? A_REVERSE : A_NORMAL);
      }

      // Clear remaining track list lines
      for (int i = viewCount; i < viewMax; ++i)
      {
        const int viewLength = m_PlaylistWindowWidth - 3;
        std::wstring trackName = Util::TrimPadWString(L"", viewLength);
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, trackName.c_str(), viewLength);
      }
      
      // Title
      wattron(m_PlaylistWindow, A_BOLD);
      const int searchStrLength = m_PlaylistWindowWidth - s_SearchWidthPad;
      const int viewLength = m_PlaylistWindowWidth - s_SearchWidthPad;
      std::wstring trackName = Util::TrimPadWString(Util::ToWString(" search: " + m_SearchString.toStdString()), viewLength);
      std::wstring spaces(viewLength, L' ');
      mvwaddnwstr(m_PlaylistWindow, 0, 2, spaces.c_str(), spaces.size());
      mvwaddnwstr(m_PlaylistWindow, 0, 2, trackName.c_str(), trackName.size());
      wattroff(m_PlaylistWindow, A_BOLD);
      wmove(m_PlaylistWindow, 0, searchStrLength - s_SearchWidthPad - 1 + m_SearchStringPos);
    }

    // Refresh
    wrefresh(m_PlaylistWindow);
  }
}

void UIView::MouseEventRequest(int p_X, int p_Y, uint32_t p_Button)
{
  // Set focus
  if ((p_Y >= m_PlayerWindowY) && (p_Y <= m_PlayerWindowY + m_PlayerWindowHeight) &&
      (p_X >= m_PlayerWindowX) && (p_X <= m_PlayerWindowX + m_PlayerWindowWidth))
  {
    if (m_UIState == UISTATE_PLAYLIST)
    {
      SetUIState(UISTATE_PLAYER);
      Refresh();
    }
  }
  else if ((p_Y >= m_PlaylistWindowY) && (p_Y <= m_PlaylistWindowY + m_PlaylistWindowHeight) &&
           (p_X >= m_PlaylistWindowX) && (p_X <= m_PlaylistWindowX + m_PlaylistWindowWidth))
  {
    if (m_UIState == UISTATE_PLAYER)
    {
      SetUIState(UISTATE_PLAYLIST);
      Refresh();
    }
  }

  // Handle click
  if (p_Button & BUTTON1_CLICKED)
  {
    // Position
    if ((p_Y == 1) && (p_X >= 2) && (p_X <= 8)) m_ViewPosition = !m_ViewPosition;

    // Title
    if ((p_Y == 1) && (p_X >= 11) && (p_X < (m_TitleWidth + 11))) m_ScrollTitle = !m_ScrollTitle;

    // Volume
    if ((p_Y == 2) && (p_X >= 11) && (p_X < (m_VolumeWidth + 11 + 2))) emit ProcessMouseEvent(UIMouseEvent(UIELEM_VOLUME, 100 * (p_X - 11) / m_VolumeWidth));

    // Position
    else if ((p_Y == 3) && (p_X >= 2) && (p_X < (m_PositionWidth + 2 + 2))) emit ProcessMouseEvent(UIMouseEvent(UIELEM_POSITION, 100 * (p_X - 2) / m_PositionWidth));

    // Previous
    else if ((p_Y == 4) && (p_X >= 2) && (p_X <= 3)) emit ProcessMouseEvent(UIMouseEvent(UIELEM_PREVIOUS, 0));
        
    // Play
    else if ((p_Y == 4) && (p_X >= 5) && (p_X <= 6)) emit ProcessMouseEvent(UIMouseEvent(UIELEM_PLAY, 0));
        
    // Pause
    else if ((p_Y == 4) && (p_X >= 8) && (p_X <= 9)) emit ProcessMouseEvent(UIMouseEvent(UIELEM_PAUSE, 0));

    // Stop
    else if ((p_Y == 4) && (p_X >= 11) && (p_X <= 12)) emit ProcessMouseEvent(UIMouseEvent(UIELEM_STOP, 0));

    // Next
    else if ((p_Y == 4) && (p_X >= 14) && (p_X <= 15)) emit ProcessMouseEvent(UIMouseEvent(UIELEM_NEXT, 0));

    // Shuffle
    else if ((p_Y == 4) && (p_X >= 18) && (p_X <= 20)) emit ProcessMouseEvent(UIMouseEvent(UIELEM_SHUFFLE, 0));

    // Playlist
    else if ((p_Y > m_PlaylistWindowY) && (p_Y < (m_PlaylistWindowY + m_PlaylistWindowHeight)) &&
             (p_X > (m_PlaylistWindowX + 1)) && (p_X < (m_PlaylistWindowX + m_PlaylistWindowWidth - 1)))
    {
      SetPlaylistSelected((m_PlaylistOffset + p_Y - m_PlaylistWindowY - 1), false);
      Refresh();
    }
  }

  // Handle double click
  if (p_Button & BUTTON1_DOUBLE_CLICKED)
  {
    // Playlist
    if ((p_Y > m_PlaylistWindowY) && (p_Y < (m_PlaylistWindowY + m_PlaylistWindowHeight)) &&
        (p_X > (m_PlaylistWindowX + 1)) && (p_X < (m_PlaylistWindowX + m_PlaylistWindowWidth - 1)))
    {
      SetPlaylistSelected((m_PlaylistOffset + p_Y - m_PlaylistWindowY - 1), false);
      Refresh();
      emit SetCurrentIndex(m_PlaylistSelected);
      emit Play();
    }
  }

  // Handle scroll down
#ifdef __APPLE__
  if (p_Button & 0x00200000)
#else
  if (p_Button & 0x08000000)
#endif
  {
    if (m_UIState == UISTATE_PLAYER)
    {
      emit ProcessMouseEvent(UIMouseEvent(UIELEM_VOLUMEDOWN, 0));
    }
    else
    {
      SetPlaylistSelected((qBound(0, m_PlaylistSelected + 1, m_Playlist.count() - 1)), true);
      Refresh();
    }
  }

  // Handle scroll up
#ifdef __APPLE__
  if (p_Button & 0x00010000)
#else
  if (p_Button & 0x00080000)
#endif
  {
    if (m_UIState == UISTATE_PLAYER)
    {
      emit ProcessMouseEvent(UIMouseEvent(UIELEM_VOLUMEUP, 0));
    }
    else
    {
      SetPlaylistSelected((qBound(0, m_PlaylistSelected - 1, m_Playlist.count() - 1)), true);
      Refresh();
    }
  }
}

void UIView::LoadTracksData()
{
  if (m_PlaylistLoaded) return;
    
  QElapsedTimer loadTime;
  loadTime.start();
  int i = 0;
  while ((loadTime.elapsed() < 50) && i < m_Playlist.count())
  {
    const int index = (m_PlaylistOffset + i) % m_Playlist.count();
    if (!m_Playlist[index].loaded)
    {
      TagLib::FileRef fileRef(m_Playlist[index].path.toStdString().c_str());
      if (!fileRef.isNull() && (fileRef.tag() != NULL))
      {
        TagLib::String artist = fileRef.tag()->artist();
        TagLib::String title = fileRef.tag()->title();
        if ((artist.length() > 0) && (title.length() > 0))
        {
          m_Playlist[index].artist = QString::fromStdString(artist.to8Bit(true));
          m_Playlist[index].title = QString::fromStdString(title.to8Bit(true));
          m_Playlist[index].name = m_Playlist[index].artist + " - " + m_Playlist[index].title;
        }
      }

      m_Playlist[index].loaded = true;
    }

    ++i;
  }

  if (i == m_Playlist.count())
  {
    m_PlaylistLoaded = true;
  }
}

void UIView::SetPlaylistSelected(int p_SelectedTrack, bool p_UpdateOffset)
{
  m_PlaylistSelected = qBound(0, p_SelectedTrack, (m_Playlist.count() - 1));
  if (p_UpdateOffset)
  {
    const int viewMax = m_PlaylistWindowHeight - 2;
    if (m_UIState & (UISTATE_PLAYER | UISTATE_PLAYLIST))
    {
      m_PlaylistOffset = qBound(0, (m_PlaylistSelected - ((viewMax - 1) / 2)), qMax(0, m_Playlist.count() - viewMax));
    }
    else
    {
      m_PlaylistOffset = qBound(0, (m_PlaylistSelected - ((viewMax - 1) / 2)), qMax(0, m_Resultlist.count() - viewMax));
    }
  }
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

