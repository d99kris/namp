// uiview.h
//
// Copyright (C) 2017-2025 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTime>
#include <QTimer>
#include <QVector>

#include <ncurses.h>

#include "common.h"
#include "scrobbler.h"

struct TrackInfo
{
  TrackInfo()
  {
  }

  TrackInfo(const QString& p_Path, const QString& p_Name, bool p_Loaded, int p_Duration, int p_Index)
    : path(p_Path)
    , name(p_Name)
    , loaded(p_Loaded)
    , duration(p_Duration)
    , index(p_Index)
  {
  }

  QString path;
  QString name; // artist - title
  QString artist;
  QString title;
  bool loaded;
  int duration;
  int index;
};

class UIView : public QObject
{
  Q_OBJECT

public:
  UIView(QObject* p_Parent, Scrobbler* p_Scrobbler);
  ~UIView();

  void SetPlaylist(const QVector<QString>& p_Playlist);
  void GetScrollTitle(bool& p_ScrollTitle);
  void SetScrollTitle(const bool& p_ScrollTitle);
  void GetViewPosition(bool& p_ViewPosition);
  void SetViewPosition(const bool& p_ViewPosition);

public slots:
  void PlaylistUpdated(const QVector<QString>& p_Paths);
  void PositionChanged(qint64 p_Position);
  void DurationChanged(qint64 p_Position);
  void CurrentIndexChanged(int p_Position);
  void VolumeChanged(int p_Volume);
  void PlaybackModeUpdated(bool p_Shuffle);
  void Search();
  void SelectPrevious();
  void SelectNext();
  void PagePrevious();
  void PageNext();
  void Home();
  void End();
  void PlaySelected();
  void ToggleWindow();
  void KeyPress(int p_Key);
  void MouseEventRequest(int p_X, int p_Y, uint32_t p_Button);
  void RefreshTrackData(int p_TrackIndex);
  void ExternalEdit();

private slots:
  void Timer();

signals:
  void UIStateUpdated(UIState);
  void SetCurrentIndex(int);
  void ProcessMouseEvent(const UIMouseEvent& p_UIMouseEvent);
  void Play();
  void ExternalEdit(int);

private:
  void SetUIState(UIState p_UIState);
  void Refresh();
  void UpdateScreen(bool p_Force = false);
  void DeleteWindows();
  void CreateWindows();
  void DrawPlayer();
  QString GetPlayerTrackName(int p_MaxLength);
  void DrawPlaylist();
  void LoadTracksData();
  void SetPlaylistSelected(int p_SelectedTrack, bool p_UpdateOffset);

private:
  Scrobbler* m_Scrobbler = nullptr;

  int m_TerminalWidth = -1;
  int m_TerminalHeight = -1;

  WINDOW* m_PlayerWindow = nullptr;
  WINDOW* m_PlaylistWindow = nullptr;

  int m_PlayerWindowWidth = 0;
  const int m_PlayerWindowHeight = 0;
  const int m_PlayerWindowX = 0;
  const int m_PlayerWindowY = 0;

  int m_PlaylistWindowWidth = 0;
  const int m_PlaylistWindowMinHeight = 0;
  int m_PlaylistWindowHeight = -1;
  int m_PlaylistWindowX = -1;
  int m_PlaylistWindowY = -1;

  int m_TitleWidth = 0;
  int m_VolumeWidth = 0;
  int m_PositionWidth = 0;

  QVector<TrackInfo> m_Playlist;
  QVector<TrackInfo> m_Resultlist;

  bool m_PlaylistLoaded = true;
  int m_TrackPositionSec = 0;
  int m_TrackDurationSec = 0;
  int m_PlaylistPosition = 0;
  int m_PlaylistSelected = 0;
  int m_PlaylistOffset = 0;
  int m_VolumePercentage = 100;
  bool m_Shuffle = true;
  bool m_ScrollTitle = false;
  bool m_ViewPosition = true;
  UIState m_UIState = UISTATE_PLAYER;
  UIState m_PreviousUIState = UISTATE_PLAYER;
  QString m_SearchString;
  int m_SearchStringPos = 0;
  QTimer* m_Timer = nullptr;
  QElapsedTimer m_PlayTime;

  bool m_SetPlaying = false;
  bool m_SetPlayed = false;
};

