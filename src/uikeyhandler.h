// uikeyhandler.h
//
// Copyright (C) 2017-2023 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QObject>
#include <QTimer>

#include <ncurses.h>

#include "common.h"

class UIKeyhandler : public QObject
{
  Q_OBJECT

public:
  UIKeyhandler(QObject *p_Parent = NULL);
  ~UIKeyhandler();

public slots:
  void ProcessKeyEvent();
  void ProcessMouseEvent(const UIMouseEvent& p_UIMouseEvent);
  void UIStateUpdated(UIState);

signals:
  void Quit();
  void Previous();
  void Play();
  void Pause();
  void Stop();
  void Next();
  void VolumeUp();
  void VolumeDown();
  void SkipBackward();
  void SkipForward();
  void ToggleShuffle();
  void Search();
  void SelectPrevious();
  void SelectNext();
  void PagePrevious();
  void PageNext();
  void Home();
  void End();
  void PlaySelected();
  void ToggleWindow();
  void SetVolume(int p_VolumePercentage);
  void SetPosition(int p_PositionPercentage);
  void KeyPress(int p_Key);
  void MouseEventRequest(int p_X, int p_Y, uint32_t p_Button);

private:
  void DoMouseEventRequest();

private:
  WINDOW* m_KeyhandlerWindow;
  UIState m_UIState;
};

