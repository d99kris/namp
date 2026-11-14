// uiview.cpp
//
// Copyright (C) 2017-2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QTime>
#include <QTimer>
#include <QVector>

#include <vector>

#include <locale.h>
#include <signal.h>
#include <string.h>
#include <wchar.h>

#include <ncurses.h>

#include <fileref.h>
#include <tag.h>

#include "scrobbler.h"
#include "log.h"
#include "uicolorconfig.h"
#include "uiview.h"
#include "util.h"

static bool s_ShowTrackPath = false;
static int s_MinTerminalWidth = 40;
static int s_MinTerminalHeight = 6;
static int s_SearchWidthPad = 4;

static void SetCChar(cchar_t* p_CChar, const wchar_t* p_Str, int p_Attr)
{
  setcchar(p_CChar, p_Str, (p_Attr & A_ATTRIBUTES & ~A_COLOR), PAIR_NUMBER(p_Attr), NULL);
}

static void DrawBorder(WINDOW* p_Window, int p_Attr)
{
  static const bool rounded = (UIColorConfig::GetString("border_style") == "rounded");
  if (rounded)
  {
    cchar_t vline, hline, ulcorner, urcorner, llcorner, lrcorner;
    SetCChar(&vline, L"│", p_Attr);
    SetCChar(&hline, L"─", p_Attr);
    SetCChar(&ulcorner, L"╭", p_Attr);
    SetCChar(&urcorner, L"╮", p_Attr);
    SetCChar(&llcorner, L"╰", p_Attr);
    SetCChar(&lrcorner, L"╯", p_Attr);
    wborder_set(p_Window, &vline, &vline, &hline, &hline, &ulcorner, &urcorner, &llcorner, &lrcorner);
  }
  else
  {
    wborder(p_Window, ACS_VLINE | p_Attr, ACS_VLINE | p_Attr, ACS_HLINE | p_Attr, ACS_HLINE | p_Attr,
            ACS_ULCORNER | p_Attr, ACS_URCORNER | p_Attr, ACS_LLCORNER | p_Attr, ACS_LRCORNER | p_Attr);
  }
}

static void DrawRepeated(WINDOW* p_Window, int p_Y, int p_X, int p_Count, const std::wstring& p_Char, int p_Attr)
{
  wattron(p_Window, p_Attr);
  for (int i = 0; i < p_Count; ++i)
  {
    mvwaddnwstr(p_Window, p_Y, p_X + i, p_Char.c_str(), 1);
  }
  wattroff(p_Window, p_Attr);
}

// Draw horizontal bar (volume / progress) with optional rail for unfilled part
static void DrawBar(WINDOW* p_Window, int p_Y, int p_X, int p_Width, int p_Filled, int p_FillAttr,
                    int p_RailAttr)
{
  static const std::wstring fillChar = UIColorConfig::GetWString("bar_fill_char");
  static const std::wstring railChar = UIColorConfig::GetWString("bar_rail_char");

  if (p_Width <= 0) return;

  mvwprintw(p_Window, p_Y, p_X, "%*c", p_Width, ' ');
  if (!railChar.empty())
  {
    DrawRepeated(p_Window, p_Y, p_X, p_Width, railChar, p_RailAttr);
  }

  const int filled = qBound(0, p_Filled, p_Width);
  if (filled == 0) return;

  if (fillChar.empty())
  {
    mvwhline(p_Window, p_Y, p_X, ACS_HLINE | p_FillAttr, filled);
  }
  else
  {
    DrawRepeated(p_Window, p_Y, p_X, filled, fillChar, p_FillAttr);
  }
}

// Draw a toggle such as "[X] Shuffle", returns its width
static int DrawToggle(WINDOW* p_Window, int p_Y, int p_X, bool p_Checked, const char* p_Label)
{
  const int controlsAttr = UIColorConfig::GetAttr("controls");
  const int toggleAttr = UIColorConfig::GetAttr("toggle", controlsAttr);
  wattron(p_Window, controlsAttr);
  mvwprintw(p_Window, p_Y, p_X, "[");
  wattroff(p_Window, controlsAttr);
  wattron(p_Window, toggleAttr);
  mvwprintw(p_Window, p_Y, p_X + 1, "%c", p_Checked ? 'X' : ' ');
  wattroff(p_Window, toggleAttr);
  wattron(p_Window, controlsAttr);
  mvwprintw(p_Window, p_Y, p_X + 2, "] %s", p_Label);
  wattroff(p_Window, controlsAttr);
  return 4 + static_cast<int>(strlen(p_Label));
}

static std::vector<std::wstring> WrapWString(const std::wstring& p_Str, int p_Width)
{
  std::vector<std::wstring> rows;
  std::wstring row;
  int rowWidth = 0;
  std::wstring word;
  auto addWord = [&]()
  {
    if (word.empty()) return;

    const int wordWidth = Util::WStringWidth(word);
    const int candidateWidth = row.empty() ? wordWidth : (rowWidth + 1 + wordWidth);
    if (candidateWidth <= p_Width)
    {
      if (!row.empty()) row += L' ';
      row += word;
      rowWidth = candidateWidth;
      word.clear();
      return;
    }

    if (!row.empty())
    {
      rows.push_back(row);
      row.clear();
    }

    // Hard-split words that are wider than the view
    while (Util::WStringWidth(word) > p_Width)
    {
      std::wstring part = Util::TrimPadWString(word, p_Width);
      if (part.empty()) part = word.substr(0, 1);
      rows.push_back(part);
      word = word.substr(part.size());
    }

    row = word;
    rowWidth = Util::WStringWidth(word);
    word.clear();
  };

  for (wchar_t ch : p_Str)
  {
    if ((ch == L' ') || (ch == L'\t'))
    {
      addWord();
    }
    else
    {
      word += ch;
    }
  }
  addWord();

  if (!row.empty() || rows.empty())
  {
    rows.push_back(row);
  }

  return rows;
}

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
  raw();
  timeout(0);
  keypad(stdscr, TRUE);
  signal(SIGINT, SIG_IGN);
  UIColorConfig::Init();

  QTimer::singleShot(0, this, &UIView::Timer);
  m_Timer = new QTimer();
  connect(m_Timer, &QTimer::timeout, this, &UIView::Timer);
  m_Timer->setInterval(1000);
  m_Timer->start();

  if (m_Scrobbler)
  {
    connect(m_Scrobbler, &Scrobbler::Result, this, &UIView::ScrobbleResult);
  }
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
  UpdateCommonAncestorPath();
  m_PlaylistLoaded = false;
  Refresh();
}

void UIView::PositionChanged(qint64 p_Position)
{
  m_TrackPositionSec = p_Position / 1000;
  m_TrackPositionMs = p_Position;
  Refresh();

  if (m_Scrobbler && (m_TrackDurationSec > 0) && (m_PlaylistPosition < m_Playlist.count()))
  {
    if (m_TrackPositionSec == 0)
    {
      m_SetPlaying = false;
      m_SetPlayed = false;
      m_ScrobbleState = SCROBBLESTATE_NONE;
      ++m_ScrobbleId;
      m_PlayTime.restart();
    }

    const TrackInfo& track = m_Playlist.at(m_PlaylistPosition);
    if (!track.loaded)
    {
      // wait for tags to be loaded before deciding whether track can be scrobbled
    }
    else if (track.artist.isEmpty() || track.title.isEmpty())
    {
      m_ScrobbleState = SCROBBLESTATE_SKIPPED;
    }
    else
    {
      const qint64 elapsedSec = m_PlayTime.elapsed() / 1000;
      if (!m_SetPlayed && (elapsedSec >= 10) && (m_TrackPositionSec >= (m_TrackDurationSec / 2))) // scrobble played after 50% (min 10 sec)
      {
        m_SetPlayed = true;
        m_Scrobbler->Played(track.artist, track.title, m_TrackDurationSec, m_ScrobbleId);
      }
      else if (!m_SetPlaying && (elapsedSec >= 3)) // scrobble playing after 3 sec
      {
        m_SetPlaying = true;
        m_Scrobbler->Playing(track.artist, track.title, m_TrackDurationSec, m_ScrobbleId);
      }
    }
  }
}

void UIView::DurationChanged(qint64 p_Position)
{
  m_TrackDurationSec = p_Position / 1000;
  m_TrackDurationMs = p_Position;

  // If plain lyrics arrived before the duration was known, synthesize
  // timestamps now so they scroll along with playback.
  if (!m_Lyrics.synced && Lyrics::AssignSyntheticTimestamps(m_Lyrics, m_TrackDurationMs))
  {
    m_LyricsSynthetic = true;
  }

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

void UIView::SpectrumChanged(const QVector<float>& p_Spectrum)
{
  m_SpectrumBands = p_Spectrum;
  if (m_PlayerWindow != NULL && m_ViewAnalyzer)
  {
    DrawSpectrumBars();
    wrefresh(m_PlayerWindow);
  }
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
  if (IsLyricsViewActive())
  {
    SetLyricsScrollRow(m_LyricsScrollRow - 1);
    return;
  }

  SetPlaylistSelected((m_PlaylistSelected - 1), true);
  Refresh();
}

void UIView::SelectNext()
{
  if (IsLyricsViewActive())
  {
    SetLyricsScrollRow(m_LyricsScrollRow + 1);
    return;
  }

  SetPlaylistSelected((m_PlaylistSelected + 1), true);
  Refresh();
}

void UIView::PagePrevious()
{
  if (IsLyricsViewActive())
  {
    SetLyricsScrollRow(m_LyricsScrollRow - (m_PlaylistWindowHeight - 2));
    return;
  }

  int pageSize = m_ViewFolders ? VisibleTrackCount() : (m_PlaylistWindowHeight - 2);
  SetPlaylistSelected((m_PlaylistSelected - pageSize), true);
  Refresh();
}

void UIView::PageNext()
{
  if (IsLyricsViewActive())
  {
    SetLyricsScrollRow(m_LyricsScrollRow + (m_PlaylistWindowHeight - 2));
    return;
  }

  int pageSize = m_ViewFolders ? VisibleTrackCount() : (m_PlaylistWindowHeight - 2);
  SetPlaylistSelected((m_PlaylistSelected + pageSize), true);
  Refresh();
}

void UIView::Home()
{
  if (IsLyricsViewActive())
  {
    SetLyricsScrollRow(0);
    return;
  }

  SetPlaylistSelected(0, true);
  Refresh();
}

void UIView::End()
{
  if (IsLyricsViewActive())
  {
    SetLyricsScrollRow(m_LyricsScrollMax);
    return;
  }

  SetPlaylistSelected((m_Playlist.count() - 1), true);
  Refresh();
}

void UIView::PlaySelected()
{
  // Playlist selection is not visible in lyrics view
  if (IsLyricsViewActive()) return;

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

void UIView::ScrobbleResult(int p_Id, bool p_Played, bool p_Success)
{
  // ignore late results for a previous track
  if (p_Id != m_ScrobbleId) return;

  if (!p_Success)
  {
    m_ScrobbleState = SCROBBLESTATE_FAILED;
  }
  else
  {
    m_ScrobbleState = p_Played ? SCROBBLESTATE_PLAYED : SCROBBLESTATE_PLAYING;
  }

  Refresh();
}

void UIView::Refresh()
{
  UpdateScreen();
  DrawPlayer();
  DrawPlaylist();
}

void UIView::UpdateScreen(bool p_Force /*= false*/)
{
  int terminalWidth = -1;
  int terminalHeight = -1;
  getmaxyx(stdscr, terminalHeight, terminalWidth);
  if (p_Force || (terminalWidth != m_TerminalWidth) || (terminalHeight != m_TerminalHeight))
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

  UpdateLyricsWindowVisible();

  m_TitleWidth = m_PlayerWindowWidth - 13;;
  m_VolumeWidth = m_PlayerWindowWidth - 15;
  m_PositionWidth = m_PlayerWindowWidth - 6;
}

void UIView::DrawPlayer()
{
  if (m_PlayerWindow != NULL)
  {
    // Border and title
    const bool playerFocus = (m_UIState == UISTATE_PLAYER);
    DrawBorder(m_PlayerWindow, UIColorConfig::GetAttr(playerFocus ? "border_focus" : "border"));
    const int titleAttributes = UIColorConfig::GetAttr(playerFocus ? "title_focus" : "title");
    wattron(m_PlayerWindow, titleAttributes);
    const int titlePos = (m_PlayerWindowWidth - 6) / 2;
    mvwprintw(m_PlayerWindow, 0, titlePos, " namp ");
    wattroff(m_PlayerWindow, titleAttributes);

    // Track position
    if (m_ViewPosition)
    {
      const int positionAttr = UIColorConfig::GetAttr("position");
      wattron(m_PlayerWindow, positionAttr);
      mvwprintw(m_PlayerWindow, 1, 3, " %02d:%02d", (m_TrackPositionSec / 60), (m_TrackPositionSec % 60));
      wattroff(m_PlayerWindow, positionAttr);
    }
    else
    {
      mvwprintw(m_PlayerWindow, 1, 3, "      ");
    }

    // Track title
    mvwprintw(m_PlayerWindow, 1, 11, "%.*s", -m_TitleWidth, "");
    std::string fullName = GetPlayerTrackName(m_TitleWidth).toStdString();
    std::wstring trackName = Util::TrimPadWString(Util::ToWString(fullName), m_TitleWidth);
    const int trackAttr = UIColorConfig::GetAttr("track");
    wattron(m_PlayerWindow, trackAttr);
    mvwaddnwstr(m_PlayerWindow, 1, 11, trackName.c_str(), trackName.size());
    wattroff(m_PlayerWindow, trackAttr);

    // Spectrum analyzer
    if (m_ViewAnalyzer)
    {
      DrawSpectrumBars();
    }
    else
    {
      mvwprintw(m_PlayerWindow, 2, 2, "        ");
    }

    // Volume
    const int controlsAttr = UIColorConfig::GetAttr("controls");
    wattron(m_PlayerWindow, controlsAttr);
    mvwprintw(m_PlayerWindow, 2, 11, "-");
    mvwprintw(m_PlayerWindow, 2, 12 + m_VolumeWidth, "+");
    wattroff(m_PlayerWindow, controlsAttr);
    DrawBar(m_PlayerWindow, 2, 12, m_VolumeWidth, (m_VolumeWidth * m_VolumePercentage) / 100,
            UIColorConfig::GetAttr("volume"), UIColorConfig::GetAttr("volume_rail"));

    // Progress
    wattron(m_PlayerWindow, controlsAttr);
    mvwprintw(m_PlayerWindow, 3, 2, "|");
    mvwprintw(m_PlayerWindow, 3, 3 + m_PositionWidth, "|");
    wattroff(m_PlayerWindow, controlsAttr);
    const int progress = (m_TrackDurationSec != 0) ? ((m_PositionWidth * m_TrackPositionSec) / m_TrackDurationSec) : 0;
    DrawBar(m_PlayerWindow, 3, 3, m_PositionWidth, progress,
            UIColorConfig::GetAttr("progress"), UIColorConfig::GetAttr("progress_rail"));

    // Playback controls
    int xpos = 2;
    wattron(m_PlayerWindow, controlsAttr);
    mvwprintw(m_PlayerWindow, 4, xpos, "|< |> || [] >|");
    wattroff(m_PlayerWindow, controlsAttr);
    mvwprintw(m_PlayerWindow, 4, xpos + 14, "  ");
    xpos += 16;

    // Scrobble indicator (only when scrobbler is configured)
    if (m_Scrobbler)
    {
      char state = ' ';
      switch (m_ScrobbleState)
      {
        case SCROBBLESTATE_SKIPPED: state = '~'; break;
        case SCROBBLESTATE_PLAYING: state = '_'; break;
        case SCROBBLESTATE_PLAYED: state = '^'; break;
        case SCROBBLESTATE_FAILED: state = '!'; break;
        default: break;
      }
      const int scrobbleAttr = UIColorConfig::GetAttr("scrobble", controlsAttr);
      wattron(m_PlayerWindow, scrobbleAttr);
      mvwprintw(m_PlayerWindow, 4, xpos, "%c", state);
      wattroff(m_PlayerWindow, scrobbleAttr);
      mvwprintw(m_PlayerWindow, 4, xpos + 1, "  ");
      xpos += 3;
    }

    // Shuffle toggle
    m_ShuffleX = xpos;
    xpos += DrawToggle(m_PlayerWindow, 4, xpos, m_Shuffle, "Shuffle");

    // Lyrics toggle, or karaoke toggle for CDG tracks (when available and
    // fits in window, with trailing space margin)
    m_LyricsX = -1;
    if (m_LyricsAvailable && (xpos + 15 <= m_PlayerWindowWidth))
    {
      m_LyricsX = xpos + 2;
      mvwprintw(m_PlayerWindow, 4, xpos, "  ");
      if (m_CdgTrack)
      {
        DrawToggle(m_PlayerWindow, 4, xpos + 2, m_CdgEnabled, "Karaoke");
      }
      else
      {
        DrawToggle(m_PlayerWindow, 4, xpos + 2, m_LyricsEnabled, "Lyrics");
        mvwprintw(m_PlayerWindow, 4, xpos + 12, " ");
      }
    }

    // Refresh
    wrefresh(m_PlayerWindow);
  }
}

QString UIView::GetPlayerTrackName(int p_MaxLength)
{
  QString trackName;
  if (m_PlaylistPosition < m_Playlist.count())
  {
    char position[16];
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

    case 3: // CTRLC
    case 27: // ESC
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
  if ((m_PlaylistWindow != NULL) && IsLyricsViewActive())
  {
    DrawLyrics();
    return;
  }

  if (m_PlaylistWindow != NULL)
  {
    // Border
    const bool playlistFocus = (m_UIState & (UISTATE_PLAYLIST | UISTATE_SEARCH));
    DrawBorder(m_PlaylistWindow, UIColorConfig::GetAttr(playlistFocus ? "border_focus" : "border"));

    const int playlistAttr = UIColorConfig::GetAttr("playlist");
    const int selectedAttr = UIColorConfig::GetAttr("playlist_selected");
    const int playingAttr = UIColorConfig::GetAttr("playlist_playing", playlistAttr);
    const bool queueMarkerSet = UIColorConfig::IsSet("queue_marker");
    const int queueMarkerAttr = UIColorConfig::GetAttr("queue_marker");

    if (m_UIState & (UISTATE_PLAYER | UISTATE_PLAYLIST))
    {
      // Title
      const int titleAttributes = UIColorConfig::GetAttr(playlistFocus ? "title_focus" : "title");
      wattron(m_PlaylistWindow, titleAttributes);
      const int titlePos = (m_PlaylistWindowWidth - 10) / 2;
      mvwprintw(m_PlaylistWindow, 0, titlePos, " playlist ");
      wattroff(m_PlaylistWindow, titleAttributes);

      // Track list
      const int viewMax = m_PlaylistWindowHeight - 2;
      const int viewLength = m_PlaylistWindowWidth - 4;
      int row = 0;
      int trackIndex = m_PlaylistOffset;
      while (row < viewMax && trackIndex < m_Playlist.count())
      {
        if (NeedsSeparatorBefore(trackIndex))
        {
          static const std::wstring sepCharStr = UIColorConfig::GetWString("folder_sep_char");
          static const QString sepChar = QString::fromStdWString(sepCharStr.empty() ? L"=" : sepCharStr.substr(0, 1));
          QString folderName = GetFolderDisplayName(trackIndex);
          QString sepLine;
          if (folderName.isEmpty())
          {
            sepLine = sepChar.repeated(viewLength);
          }
          else
          {
            int nameLen = folderName.length();
            int padTotal = viewLength - nameLen - 2;
            int padLeft = qMax(1, padTotal / 2);
            int padRight = qMax(1, padTotal - padLeft);
            sepLine = sepChar.repeated(padLeft) + " " + folderName + " " + sepChar.repeated(padRight);
          }
          std::wstring sepWStr = Util::TrimPadWString(Util::ToWString(sepLine.toStdString()), viewLength);
          const int folderAttr = UIColorConfig::GetAttr("folder");
          wattron(m_PlaylistWindow, folderAttr);
          std::wstring spaces(viewLength, L' ');
          mvwaddnwstr(m_PlaylistWindow, row + 1, 2, spaces.c_str(), spaces.size());
          mvwaddnwstr(m_PlaylistWindow, row + 1, 2, sepWStr.c_str(), sepWStr.size());
          wattroff(m_PlaylistWindow, folderAttr);
          ++row;
          if (row >= viewMax) break;
        }

        std::string fullName = (s_ShowTrackPath ? m_Playlist.at(trackIndex).path : m_Playlist.at(trackIndex).name).toStdString();
        std::wstring marker = GetQueueMarker(trackIndex);
        const int markerWidth = Util::WStringWidth(marker);
        const int nameWidth = qMax(0, viewLength - markerWidth);
        std::wstring trackName = Util::TrimPadWString(Util::ToWString(fullName), nameWidth);
        std::wstring line = trackName + marker;
        line = Util::TrimPadWString(line, viewLength);
        const bool selected = (trackIndex == m_PlaylistSelected);
        const int rowAttr = selected ? selectedAttr : ((trackIndex == m_PlaylistPosition) ? playingAttr : playlistAttr);
        wattron(m_PlaylistWindow, rowAttr);
        std::wstring spaces(viewLength, L' ');
        mvwaddnwstr(m_PlaylistWindow, row + 1, 2, spaces.c_str(), spaces.size());
        mvwaddnwstr(m_PlaylistWindow, row + 1, 2, line.c_str(), line.size());
        wattroff(m_PlaylistWindow, rowAttr);
        if (!selected && queueMarkerSet && !marker.empty() && (nameWidth < viewLength))
        {
          std::wstring markerStr = Util::TrimPadWString(marker, viewLength - nameWidth);
          wattron(m_PlaylistWindow, queueMarkerAttr);
          mvwaddnwstr(m_PlaylistWindow, row + 1, 2 + nameWidth, markerStr.c_str(), markerStr.size());
          wattroff(m_PlaylistWindow, queueMarkerAttr);
        }
        ++row;
        ++trackIndex;
      }

      // Clear remaining rows
      for (int i = row; i < viewMax; ++i)
      {
        std::wstring clearStr = Util::TrimPadWString(L"", viewLength);
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, clearStr.c_str(), viewLength);
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
        std::wstring marker = GetQueueMarker(m_Resultlist.at(playlistIndex).index);
        const int markerWidth = Util::WStringWidth(marker);
        const int nameWidth = qMax(0, viewLength - markerWidth);
        std::wstring trackName = Util::TrimPadWString(Util::ToWString(fullName), nameWidth);
        std::wstring line = trackName + marker;
        line = Util::TrimPadWString(line, viewLength);
        const bool selected = (playlistIndex == m_PlaylistSelected);
        const bool playing = (m_Resultlist.at(playlistIndex).index == m_PlaylistPosition);
        const int rowAttr = selected ? selectedAttr : (playing ? playingAttr : playlistAttr);
        wattron(m_PlaylistWindow, rowAttr);
        std::wstring spaces(viewLength, L' ');
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, spaces.c_str(), spaces.size());
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, line.c_str(), line.size());
        wattroff(m_PlaylistWindow, rowAttr);
        if (!selected && queueMarkerSet && !marker.empty() && (nameWidth < viewLength))
        {
          std::wstring markerStr = Util::TrimPadWString(marker, viewLength - nameWidth);
          wattron(m_PlaylistWindow, queueMarkerAttr);
          mvwaddnwstr(m_PlaylistWindow, i + 1, 2 + nameWidth, markerStr.c_str(), markerStr.size());
          wattroff(m_PlaylistWindow, queueMarkerAttr);
        }
      }

      // Clear remaining track list lines
      for (int i = viewCount; i < viewMax; ++i)
      {
        const int viewLength = m_PlaylistWindowWidth - 3;
        std::wstring trackName = Util::TrimPadWString(L"", viewLength);
        mvwaddnwstr(m_PlaylistWindow, i + 1, 2, trackName.c_str(), viewLength);
      }

      // Title
      const int searchAttr = UIColorConfig::GetAttr("search");
      wattron(m_PlaylistWindow, searchAttr);
      const int searchStrLength = m_PlaylistWindowWidth - s_SearchWidthPad;
      const int viewLength = m_PlaylistWindowWidth - s_SearchWidthPad;
      std::wstring trackName = Util::TrimPadWString(Util::ToWString(" search: " + m_SearchString.toStdString()), viewLength);
      std::wstring spaces(viewLength, L' ');
      mvwaddnwstr(m_PlaylistWindow, 0, 2, spaces.c_str(), spaces.size());
      mvwaddnwstr(m_PlaylistWindow, 0, 2, trackName.c_str(), trackName.size());
      wattroff(m_PlaylistWindow, searchAttr);
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

    // Analyzer
    if ((p_Y == 2) && (p_X >= 2) && (p_X <= 9)) { m_ViewAnalyzer = !m_ViewAnalyzer; emit AnalyzerEnabled(m_ViewAnalyzer); }

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
    else if ((p_Y == 4) && (p_X >= m_ShuffleX) && (p_X <= m_ShuffleX + 2)) emit ProcessMouseEvent(UIMouseEvent(UIELEM_SHUFFLE, 0));

    // Lyrics / Karaoke
    else if ((m_LyricsX >= 0) && (p_Y == 4) && (p_X >= m_LyricsX) && (p_X <= m_LyricsX + 2)) emit ProcessMouseEvent(UIMouseEvent(m_CdgTrack ? UIELEM_KARAOKE : UIELEM_LYRICS, 0));

    // Playlist
    else if (!IsLyricsViewActive() &&
             (p_Y > m_PlaylistWindowY) && (p_Y < (m_PlaylistWindowY + m_PlaylistWindowHeight)) &&
             (p_X > (m_PlaylistWindowX + 1)) && (p_X < (m_PlaylistWindowX + m_PlaylistWindowWidth - 1)))
    {
      int clickedIndex = ScreenRowToTrackIndex(p_Y - m_PlaylistWindowY - 1);
      if (clickedIndex >= 0)
      {
        SetPlaylistSelected(clickedIndex, false);
        Refresh();
      }
    }
  }

  // Handle double click
  if (p_Button & BUTTON1_DOUBLE_CLICKED)
  {
    // Playlist
    if (!IsLyricsViewActive() &&
        (p_Y > m_PlaylistWindowY) && (p_Y < (m_PlaylistWindowY + m_PlaylistWindowHeight)) &&
        (p_X > (m_PlaylistWindowX + 1)) && (p_X < (m_PlaylistWindowX + m_PlaylistWindowWidth - 1)))
    {
      int clickedIndex = ScreenRowToTrackIndex(p_Y - m_PlaylistWindowY - 1);
      if (clickedIndex >= 0)
      {
        SetPlaylistSelected(clickedIndex, false);
        Refresh();
        emit SetCurrentIndex(m_PlaylistSelected);
        emit Play();
      }
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
    else if (!IsLyricsViewActive())
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
    else if (!IsLyricsViewActive())
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
      // reset to defaults, so stale data is not kept if tags were removed
      m_Playlist[index].artist.clear();
      m_Playlist[index].title.clear();
      m_Playlist[index].name = QFileInfo(m_Playlist[index].path).completeBaseName();

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

void UIView::RefreshTrackData(int p_TrackIndex)
{
  m_Playlist[p_TrackIndex].loaded = false;
  m_PlaylistLoaded = false;

  if (p_TrackIndex == m_PlaylistPosition)
  {
    m_SetPlaying = false;
    m_SetPlayed = false;
    m_ScrobbleState = SCROBBLESTATE_NONE;
    ++m_ScrobbleId;
  }

  UpdateScreen(true /*p_Force*/);
  DrawPlayer();
  DrawPlaylist();
}

void UIView::SetPlaylistSelected(int p_SelectedTrack, bool p_UpdateOffset)
{
  m_PlaylistSelected = qBound(0, p_SelectedTrack, (m_Playlist.count() - 1));
  if (p_UpdateOffset)
  {
    const int viewMax = m_PlaylistWindowHeight - 2;
    if (m_UIState & (UISTATE_PLAYER | UISTATE_PLAYLIST))
    {
      if (m_ViewFolders)
      {
        // Walk backward from selected track, counting both track rows and
        // separator rows, to find the offset that centers it in the viewport.
        int targetRow = (viewMax - 1) / 2;
        int offset = m_PlaylistSelected;
        int rowsAbove = 0;
        while (offset > 0 && rowsAbove < targetRow)
        {
          if (NeedsSeparatorBefore(offset))
            ++rowsAbove;
          --offset;
          ++rowsAbove;
        }
        m_PlaylistOffset = qMax(0, offset);

        // Clamp: avoid unnecessary empty space at the bottom of the viewport
        int totalRows = 0;
        for (int i = m_PlaylistOffset; i < m_Playlist.count(); ++i)
        {
          if (NeedsSeparatorBefore(i))
            ++totalRows;
          ++totalRows;
          if (totalRows >= viewMax) break;
        }
        while (totalRows < viewMax && m_PlaylistOffset > 0)
        {
          --m_PlaylistOffset;
          ++totalRows;
          if (NeedsSeparatorBefore(m_PlaylistOffset))
            ++totalRows;
        }
      }
      else
      {
        m_PlaylistOffset = qBound(0, (m_PlaylistSelected - ((viewMax - 1) / 2)), qMax(0, m_Playlist.count() - viewMax));
      }
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

void UIView::GetViewAnalyzer(bool& p_ViewAnalyzer)
{
  p_ViewAnalyzer = m_ViewAnalyzer;
}

void UIView::SetViewAnalyzer(const bool& p_ViewAnalyzer)
{
  m_ViewAnalyzer = p_ViewAnalyzer;
  emit AnalyzerEnabled(m_ViewAnalyzer);
}

void UIView::GetViewFolders(bool& p_ViewFolders)
{
  p_ViewFolders = m_ViewFolders;
}

void UIView::SetViewFolders(const bool& p_ViewFolders)
{
  m_ViewFolders = p_ViewFolders;
}

void UIView::ToggleFolders()
{
  m_ViewFolders = !m_ViewFolders;
  Refresh();
}

bool UIView::NeedsSeparatorBefore(int p_PlaylistIndex) const
{
  if (!m_ViewFolders) return false;
  if (p_PlaylistIndex < 0 || p_PlaylistIndex >= m_Playlist.count()) return false;
  if (p_PlaylistIndex == 0) return true;
  QString thisDir = QFileInfo(m_Playlist.at(p_PlaylistIndex).path).path();
  QString prevDir = QFileInfo(m_Playlist.at(p_PlaylistIndex - 1).path).path();
  return (thisDir != prevDir);
}

QString UIView::GetFolderDisplayName(int p_PlaylistIndex) const
{
  QString dirPath = QFileInfo(m_Playlist.at(p_PlaylistIndex).path).path();
  if (!m_CommonAncestorPath.isEmpty() && dirPath.startsWith(m_CommonAncestorPath))
  {
    QString relative = dirPath.mid(m_CommonAncestorPath.length());
    if (relative.startsWith('/')) relative = relative.mid(1);
    return relative;
  }
  return QFileInfo(dirPath).fileName();
}

void UIView::UpdateCommonAncestorPath()
{
  if (m_Playlist.isEmpty())
  {
    m_CommonAncestorPath.clear();
    return;
  }

  QStringList commonParts = QFileInfo(m_Playlist.at(0).path).path().split('/');
  for (int i = 1; i < m_Playlist.count(); ++i)
  {
    QStringList parts = QFileInfo(m_Playlist.at(i).path).path().split('/');
    int minLen = qMin(commonParts.size(), parts.size());
    int match = 0;
    while (match < minLen && commonParts.at(match) == parts.at(match))
      ++match;
    commonParts = commonParts.mid(0, match);
  }

  m_CommonAncestorPath = commonParts.join('/');
}

int UIView::VisibleTrackCount() const
{
  const int viewMax = m_PlaylistWindowHeight - 2;
  int count = 0;
  int rows = 0;
  for (int i = m_PlaylistOffset; i < m_Playlist.count() && rows < viewMax; ++i)
  {
    if (NeedsSeparatorBefore(i))
      ++rows;
    if (rows < viewMax)
    {
      ++count;
      ++rows;
    }
  }
  return qMax(1, count);
}

int UIView::ScreenRowToTrackIndex(int p_ScreenRow) const
{
  int row = 0;
  int trackIndex = m_PlaylistOffset;
  while (trackIndex < m_Playlist.count() && row <= p_ScreenRow)
  {
    if (NeedsSeparatorBefore(trackIndex))
    {
      if (row == p_ScreenRow) return -1;
      ++row;
    }
    if (row == p_ScreenRow) return trackIndex;
    ++row;
    ++trackIndex;
  }
  return -1;
}

void UIView::SetLyricsAvailable(bool p_Available)
{
  m_LyricsAvailable = p_Available;
}

void UIView::LyricsUpdated(bool p_Enabled)
{
  m_LyricsEnabled = p_Enabled;
  Refresh();
}

void UIView::CdgUpdated(bool p_HasCdg, bool p_Enabled)
{
  m_CdgTrack = p_HasCdg;
  m_CdgEnabled = p_Enabled;
  Refresh();
}

void UIView::SetLyrics(const LyricsData& p_Lyrics)
{
  m_LyricsScrollRow = 0;
  m_LyricsRowsWidth = -1;
  m_Lyrics = p_Lyrics;
  m_LyricsStatus.clear();
  m_LyricsSynthetic = false;

  // Plain lyrics are spread across the track duration (when known)
  if (!m_Lyrics.synced && Lyrics::AssignSyntheticTimestamps(m_Lyrics, m_TrackDurationMs))
  {
    m_LyricsSynthetic = true;
  }

  Refresh();
}

void UIView::ClearLyrics()
{
  ResetLyrics("no lyrics found");
}

void UIView::LyricsLoading()
{
  ResetLyrics("loading lyrics...");
}

void UIView::LyricsRetrying(int p_Retry, int p_MaxRetries)
{
  ResetLyrics("lyrics service busy, retrying (" + std::to_string(p_Retry) + "/" +
              std::to_string(p_MaxRetries) + ")...");
}

void UIView::LyricsServiceBusy()
{
  ResetLyrics("lyrics service busy, try again later");
}

void UIView::ResetLyrics(const std::string& p_Status)
{
  m_LyricsScrollRow = 0;
  m_LyricsRowsWidth = -1;
  m_Lyrics = LyricsData();
  m_LyricsStatus = p_Status;
  m_LyricsSynthetic = false;
  Refresh();
}

void UIView::GetLyricsWindowEnabled(bool& p_LyricsWindowEnabled)
{
  p_LyricsWindowEnabled = m_LyricsWindowEnabled;
}

void UIView::SetLyricsWindowEnabled(const bool& p_LyricsWindowEnabled)
{
  m_LyricsWindowEnabled = p_LyricsWindowEnabled;
  UpdateLyricsWindowVisible();
}

void UIView::UpdateLyricsWindowVisible()
{
  // Keep lyrics window visible when the terminal is too small to show the
  // playlist window, as terminal lyrics would not be displayed anywhere.
  const bool lyricsWindowVisible = m_LyricsWindowEnabled || (m_PlaylistWindow == NULL);
  if (lyricsWindowVisible == m_LyricsWindowVisible) return;

  m_LyricsWindowVisible = lyricsWindowVisible;
  emit LyricsWindowEnabledChanged(m_LyricsWindowVisible);
}

void UIView::SetLyricsScrollRow(int p_Row)
{
  // Manual scrolling only applies to lyrics without (synthetic) timestamps,
  // i.e. when track duration is unknown
  if (m_Lyrics.synced) return;

  m_LyricsScrollRow = qBound(0, p_Row, m_LyricsScrollMax);
  Refresh();
}

void UIView::ToggleLyricsWindow()
{
  SetLyricsWindowEnabled(!m_LyricsWindowEnabled);
  Refresh();
}

bool UIView::IsLyricsViewActive() const
{
  // Lyrics are not shown for CDG tracks, which use karaoke instead
  return m_LyricsAvailable && m_LyricsEnabled && !m_LyricsWindowVisible && !m_CdgTrack &&
    (m_UIState & (UISTATE_PLAYER | UISTATE_PLAYLIST));
}

void UIView::DrawLyrics()
{
  // Border and title
  const bool lyricsFocus = (m_UIState & UISTATE_PLAYLIST);
  DrawBorder(m_PlaylistWindow, UIColorConfig::GetAttr(lyricsFocus ? "border_focus" : "border"));
  const int titleAttributes = UIColorConfig::GetAttr(lyricsFocus ? "title_focus" : "title");
  wattron(m_PlaylistWindow, titleAttributes);
  const int titlePos = (m_PlaylistWindowWidth - 8) / 2;
  mvwprintw(m_PlaylistWindow, 0, titlePos, " lyrics ");
  wattroff(m_PlaylistWindow, titleAttributes);

  const int viewMax = m_PlaylistWindowHeight - 2;
  const int viewLength = m_PlaylistWindowWidth - 4;

  UpdateLyricsRows(viewLength);
  const std::vector<std::wstring>& rows = m_LyricsRows;
  const std::vector<int>& rowLines = m_LyricsRowLines;

  int currentLine = -1;
  int topRow = 0;
  if (m_Lyrics.lines.isEmpty())
  {
    topRow = (viewMax - 1) / 2;
  }
  else
  {
    if (m_Lyrics.synced)
    {
      currentLine = Lyrics::FindCurrentLine(m_Lyrics, m_TrackPositionMs);

      // Anchor current line at center for timestamped lyrics, and higher up for
      // synthetic timestamps so that more of the upcoming lyrics are visible
      const int anchorRow = m_LyricsSynthetic ? ((viewMax * 3) / 10) : ((viewMax - 1) / 2);
      if (currentLine >= 0)
      {
        const int currentFirstRow = m_LyricsLineFirstRow.at(currentLine);
        const int currentRowCount = m_LyricsLineFirstRow.at(currentLine + 1) - currentFirstRow;
        topRow = anchorRow - currentFirstRow - ((currentRowCount - 1) / 2);
      }
      else
      {
        topRow = anchorRow;
      }
    }
    else
    {
      m_LyricsScrollMax = qMax(0, (int)rows.size() - viewMax);
      m_LyricsScrollRow = qBound(0, m_LyricsScrollRow, m_LyricsScrollMax);
      topRow = -m_LyricsScrollRow;
    }
  }

  // Draw rows centered horizontally, dim past lines and bold current line
  const bool highlight = m_Lyrics.synced && !m_LyricsSynthetic && (currentLine >= 0);
  const int lyricsAttr = UIColorConfig::GetAttr("lyrics");
  const int lyricsPastAttr = UIColorConfig::GetAttr("lyrics_past");
  const int lyricsCurrentAttr = UIColorConfig::GetAttr("lyrics_current");
  const int lyricsStatusAttr = UIColorConfig::GetAttr("lyrics_status");
  for (int i = 0; i < viewMax; ++i)
  {
    std::wstring spaces(viewLength, L' ');
    mvwaddnwstr(m_PlaylistWindow, i + 1, 2, spaces.c_str(), spaces.size());

    const int rowIndex = i - topRow;
    if ((rowIndex < 0) || (rowIndex >= (int)rows.size())) continue;

    const int line = rowLines.at(rowIndex);
    int attributes = lyricsAttr;
    if (line < 0)
    {
      attributes = lyricsStatusAttr;
    }
    else if (highlight)
    {
      attributes = (line == currentLine) ? lyricsCurrentAttr : ((line < currentLine) ? lyricsPastAttr : lyricsAttr);
    }

    const std::wstring& row = rows.at(rowIndex);
    const int x = 2 + qMax(0, (viewLength - Util::WStringWidth(row)) / 2);
    wattron(m_PlaylistWindow, attributes);
    mvwaddnwstr(m_PlaylistWindow, i + 1, x, row.c_str(), row.size());
    wattroff(m_PlaylistWindow, attributes);
  }

  // Refresh
  wrefresh(m_PlaylistWindow);
}

void UIView::UpdateLyricsRows(int p_Width)
{
  if (p_Width == m_LyricsRowsWidth) return;

  // Wrap lyrics lines into view rows, tracking which lyrics line each row
  // belongs to, and the first row of each lyrics line
  m_LyricsRowsWidth = p_Width;
  m_LyricsRows.clear();
  m_LyricsRowLines.clear();
  m_LyricsLineFirstRow.clear();
  if (m_Lyrics.lines.isEmpty())
  {
    m_LyricsRows.push_back(Util::ToWString(m_LyricsStatus));
    m_LyricsRowLines.push_back(-1);
    return;
  }

  for (int i = 0; i < m_Lyrics.lines.size(); ++i)
  {
    m_LyricsLineFirstRow.push_back(m_LyricsRows.size());
    std::vector<std::wstring> lineRows = WrapWString(Util::ToWString(m_Lyrics.lines.at(i).text.toStdString()), p_Width);
    for (const std::wstring& lineRow : lineRows)
    {
      m_LyricsRows.push_back(lineRow);
      m_LyricsRowLines.push_back(i);
    }
  }
  m_LyricsLineFirstRow.push_back(m_LyricsRows.size());
}

void UIView::ToggleAnalyzer()
{
  m_ViewAnalyzer = !m_ViewAnalyzer;
  emit AnalyzerEnabled(m_ViewAnalyzer);
  Refresh();
}

void UIView::DrawSpectrumBars()
{
  // Characters for levels 0-7, left-padded with space when fewer are given
  // (i.e. 7 characters set levels 1-7 and keep level 0 blank)
  static const std::wstring bars = []()
  {
    std::wstring chars = UIColorConfig::GetWString("spectrum_chars").substr(0, 8);
    if (chars.empty()) chars = L"\u2581\u2582\u2583\u2584\u2585\u2586\u2587";
    return std::wstring(8 - chars.size(), L' ') + chars;
  }();

  // Level from which bars use spectrum_peak, 8 to disable
  static const int peakLevel = []()
  {
    bool ok = false;
    const int level = QString::fromStdString(UIColorConfig::GetString("spectrum_peak_level")).toInt(&ok);
    if (ok && (level >= 1) && (level <= 8)) return level;

    Log::Warning("invalid spectrum_peak_level, using 5");
    return 5;
  }();

  const int barAttr = UIColorConfig::GetAttr("spectrum");
  const int peakAttr = UIColorConfig::GetAttr("spectrum_peak", barAttr);
  for (int i = 0; i < 8 && i < m_SpectrumBands.size(); ++i)
  {
    int level = qBound(0, static_cast<int>(m_SpectrumBands[i] * 7.0f), 7);
    wchar_t ch = bars[level];
    const int attr = (level >= peakLevel) ? peakAttr : barAttr;
    wattron(m_PlayerWindow, attr);
    mvwaddnwstr(m_PlayerWindow, 2, 2 + i, &ch, 1);
    wattroff(m_PlayerWindow, attr);
  }
}

void UIView::ExternalEdit()
{
  // Playlist selection is not visible in lyrics view, so edit current track
  emit ExternalEdit(IsLyricsViewActive() ? m_PlaylistPosition : m_PlaylistSelected);
}

void UIView::Enqueue()
{
  if ((m_UIState & (UISTATE_PLAYER | UISTATE_PLAYLIST)) && !IsLyricsViewActive())
  {
    emit EnqueueTrack(m_PlaylistSelected);
  }
}

void UIView::Unenqueue()
{
  if ((m_UIState & (UISTATE_PLAYER | UISTATE_PLAYLIST)) && !IsLyricsViewActive())
  {
    emit UnenqueueTrack(m_PlaylistSelected);
  }
}

void UIView::QueueUpdated(const QVector<int>& p_Queue)
{
  m_Queue = p_Queue;
  Refresh();
}

std::wstring UIView::GetQueueMarker(int p_TrackIndex) const
{
  QStringList positions;
  for (int i = 0; i < m_Queue.size(); ++i)
  {
    if (m_Queue.at(i) == p_TrackIndex)
    {
      positions.append(QString::number(i + 1));
    }
  }
  if (positions.isEmpty()) return std::wstring();
  QString marker = " [" + positions.join(",") + "]";
  return Util::ToWString(marker.toStdString());
}
