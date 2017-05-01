// uikeyhandler.cpp
//
// Copyright (C) 2017 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QObject>
#include <QTimer>

#include <ncursesw/ncurses.h>

#include "uikeyhandler.h"

UIKeyhandler::UIKeyhandler(QObject *p_Parent /* = NULL */)
  : QObject(p_Parent)
  , m_KeyhandlerWindow(newwin(1, 1, 1, 1))
  , m_Timer(new QTimer(p_Parent))
  , m_UIState(UISTATE_PLAYER)
{
  cbreak();
  curs_set(0);
  wtimeout(m_KeyhandlerWindow, 0);
  keypad(m_KeyhandlerWindow, TRUE);
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
  mouseinterval(200);

  connect(m_Timer, SIGNAL(timeout()), this, SLOT(Poll()));
  m_Timer->setInterval(10);
  m_Timer->start();
}

UIKeyhandler::~UIKeyhandler()
{
  if (m_KeyhandlerWindow != NULL)
  {
    delwin(m_KeyhandlerWindow);
    m_KeyhandlerWindow = NULL;
  }

  if (m_Timer != NULL)
  {
    m_Timer->stop();
    delete m_Timer;
    m_Timer = NULL;
  }
}

void UIKeyhandler::Poll()
{
  int key = wgetch(m_KeyhandlerWindow);

  if (m_UIState & UISTATE_SEARCH)
  {
    emit KeyPress(key);
    return;
  }
        
  switch (key)
  {
    case 'z':
    case 'Z':
      emit Previous();
      break;

    case 'x':
    case 'X':
      emit Play();
      break;

    case 'c':
    case 'C':
      emit Pause();
      break;

    case 'v':
    case 'V':
      emit Stop();
      break;

    case 'b':
    case 'B':
      emit Next();
      break;

    case 27:
    case 'q':
    case 'Q':
      emit Quit();
      break;

    case '/':
    case 'j':
    case 'J':
      emit Search();
      break;

    case KEY_UP:
      if (m_UIState & UISTATE_PLAYER) emit VolumeUp();
      if (m_UIState & UISTATE_PLAYLIST) emit SelectPrevious();
      break;

    case KEY_DOWN:
      if (m_UIState & UISTATE_PLAYER) emit VolumeDown();
      if (m_UIState & UISTATE_PLAYLIST) emit SelectNext();
      break;

    case KEY_LEFT:
      emit SkipBackward();
      break;

    case KEY_RIGHT:
      emit SkipForward();
      break;

    case KEY_PPAGE:
      if (m_UIState & UISTATE_PLAYLIST) emit PagePrevious();
      break;

    case KEY_NPAGE:
      if (m_UIState & UISTATE_PLAYLIST) emit PageNext();
      break;

    case '\n':
      if (m_UIState & UISTATE_PLAYLIST) emit PlaySelected();
      break;

    case '+':
      emit VolumeUp();
      break;

    case '-':
      emit VolumeDown();
      break;

    case 's':
    case 'S':
      emit ToggleShuffle();
      break;

    case KEY_MOUSE:
      DoMouseEventRequest();
      break;

    case '\t':
      emit ToggleWindow();
      break;

    default:
      break;
  }
}

void UIKeyhandler::UIStateUpdated(UIState p_UIState)
{
  m_UIState = p_UIState;
}

void UIKeyhandler::DoMouseEventRequest()
{
  MEVENT event;
  if (getmouse(&event) == OK) emit MouseEventRequest(event.x, event.y, event.bstate);
}

void UIKeyhandler::ProcessMouseEvent(const UIMouseEvent& p_UIMouseEvent)
{
  switch (p_UIMouseEvent.elem)
  {
    case UIELEM_VOLUME:
      emit SetVolume(p_UIMouseEvent.value);
      break;

    case UIELEM_POSITION:
      emit SetPosition(p_UIMouseEvent.value);
      break;

    case UIELEM_PREVIOUS:
      emit Previous();
      break;

    case UIELEM_PLAY:
      emit Play();
      break;

    case UIELEM_PAUSE:
      emit Pause();
      break;

    case UIELEM_STOP:
      emit Stop();
      break;

    case UIELEM_NEXT:
      emit Next();
      break;

    case UIELEM_SHUFFLE:
      emit ToggleShuffle();
      break;

    case UIELEM_VOLUMEUP:
      emit VolumeUp();
      break;

    case UIELEM_VOLUMEDOWN:
      emit VolumeDown();
      break;

    default:
      break;
  }
}

