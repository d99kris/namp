// uicolorconfig.h
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <map>
#include <string>

class UIColorConfig
{
public:
  // Load color config (creating it with defaults if missing) and init curses
  // colors. Must be called after initscr().
  static void Init();

  // Curses attributes (including color pair) for a UI element, based on its
  // <element>_attr, <element>_color_bg and <element>_color_fg settings.
  static int GetAttr(const std::string& p_Element);

  // As above, but returns p_Fallback if the element has no settings.
  static int GetAttr(const std::string& p_Element, int p_Fallback);

  // Whether any of the element's attr/color settings are non-empty.
  static bool IsSet(const std::string& p_Element);

  static std::string GetString(const std::string& p_Key);
  static std::wstring GetWString(const std::string& p_Key);
  static std::string GetPath();

private:
  static int ParseAttributes(const std::string& p_Str);
  static int ParseColor(const std::string& p_Str);
  static void Save(const std::string& p_Path);

private:
  static std::map<std::string, std::string> s_Config;
  static std::map<std::string, int> s_Attrs;
  static std::map<std::string, bool> s_IsSet;
  static int s_ColorPairId;
};
