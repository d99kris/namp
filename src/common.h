// common.h
//
// Copyright (C) 2017-2019 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

enum UIState
{
  UISTATE_PLAYER    = (1u << 0),
  UISTATE_PLAYLIST  = (1u << 1),
  UISTATE_SEARCH    = (1u << 2),
};

enum UIElem
{
  UIELEM_VOLUME,
  UIELEM_POSITION,
  UIELEM_PREVIOUS,
  UIELEM_PLAY,
  UIELEM_PAUSE,
  UIELEM_STOP,
  UIELEM_NEXT,
  UIELEM_SHUFFLE,
  UIELEM_VOLUMEUP,
  UIELEM_VOLUMEDOWN,
};

struct UIMouseEvent
{
  UIMouseEvent()
  {
  }

  UIMouseEvent(const UIElem& p_Elem, int p_Value)
    : elem(p_Elem)
    , value(p_Value)
  {
  }

  UIElem elem;
  int value;
};

