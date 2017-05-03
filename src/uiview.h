// uiview.h
//
// Copyright (C) 2017 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>

#include <ncursesw/ncurses.h>

#include "common.h"

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
  QString name;
  bool loaded;
  int duration;
  int index;
};

class UIView : public QObject
{
  Q_OBJECT

public:
  UIView(QObject *p_Parent = NULL);
  ~UIView();

  void SetPlaylist(const QVector<QString>& p_Playlist);

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
  void PlaySelected();
  void ToggleWindow();
  void KeyPress(int p_Key);
  void MouseEventRequest(int p_X, int p_Y, uint32_t p_Button);

private slots:
  void Timer();

signals:
  void UIStateUpdated(UIState);
  void SetCurrentIndex(int);
  void ProcessMouseEvent(const UIMouseEvent& p_UIMouseEvent);

private:
  void SetUIState(UIState p_UIState);
  void Refresh();
  void UpdateScreen();
  void DeleteWindows();
  void CreateWindows();
  void DrawPlayer();
  QString GetPlayerTrackName(int p_MaxLength);
  void DrawPlaylist();
  void LoadTracksData();

private:
  int m_TerminalWidth;
  int m_TerminalHeight;

  WINDOW* m_PlayerWindow;
  WINDOW* m_PlaylistWindow;

  const int m_PlayerWindowWidth;
  const int m_PlayerWindowHeight;
  const int m_PlayerWindowX;
  const int m_PlayerWindowY;

  const int m_PlaylistWindowWidth;
  const int m_PlaylistWindowMinHeight;
  int m_PlaylistWindowHeight;
  int m_PlaylistWindowX;
  int m_PlaylistWindowY;

  QVector<TrackInfo> m_Playlist;
  QVector<TrackInfo> m_Resultlist;

  bool m_PlaylistLoaded;
  int m_TrackPositionSec;
  int m_TrackDurationSec;
  int m_PlaylistPosition;
  int m_PlaylistSelected;
  int m_PlaylistOffset;
  int m_VolumePercentage;
  bool m_Shuffle;
  UIState m_UIState;
  UIState m_PreviousUIState;
  QString m_SearchString;
  int m_SearchStringPos;
  QTimer* m_Timer;
};

