// uikeyhandler.cpp
//
// Copyright (C) 2023 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QElapsedTimer>

#include <mutex>
#include <queue>

#include <signal.h>

#include "uikeyhandler.h"

enum Cmd
{
  CMD_QUIT = 0,
  CMD_NEXT,
};

static std::mutex s_Mutex;
static std::queue<int> s_Queue;
static QElapsedTimer s_SignalTimer;

static void s_SignalHandler(int /*p_Signal*/)
{
  const bool isWithinOneSecondSinceLastSignal = (s_SignalTimer.elapsed() < 1000);
  s_SignalTimer.restart();

  std::lock_guard<std::mutex> lock(s_Mutex);
  s_Queue.push(isWithinOneSecondSinceLastSignal ? CMD_QUIT : CMD_NEXT);
}

UIKeyhandler::UIKeyhandler(QObject *p_Parent /* = NULL */)
  : QObject(p_Parent)
{
  signal(SIGINT, s_SignalHandler);
  s_SignalTimer.start();

  static QTimer s_EventTimer;
  connect(&s_EventTimer, &QTimer::timeout, this, &UIKeyhandler::ProcessKeyEvent);
  s_EventTimer.setInterval(100);
  s_EventTimer.start();
}

UIKeyhandler::~UIKeyhandler()
{
}

void UIKeyhandler::ProcessKeyEvent()
{
  std::lock_guard<std::mutex> lock(s_Mutex);
  if (s_Queue.empty()) return;

  int cmd = s_Queue.front();
  s_Queue.pop();
  if (cmd == CMD_QUIT)
  {
    emit Quit();
  }
  else if (cmd == CMD_NEXT)
  {
    emit Next();
  }
}

void UIKeyhandler::UIStateUpdated(UIState p_UIState)
{
  m_UIState = p_UIState;
}

void UIKeyhandler::DoMouseEventRequest()
{
}

void UIKeyhandler::ProcessMouseEvent(const UIMouseEvent& /*p_UIMouseEvent*/)
{
}
