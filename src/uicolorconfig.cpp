// uicolorconfig.cpp
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include "uicolorconfig.h"

#include <set>

#include <ncurses.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

#include "log.h"
#include "util.h"

std::map<std::string, std::string> UIColorConfig::s_Config;
std::map<std::string, int> UIColorConfig::s_Attrs;
std::map<std::string, bool> UIColorConfig::s_IsSet;
int UIColorConfig::s_ColorPairId = 0;

void UIColorConfig::Init()
{
  // Defaults reproduce the classic namp look (terminal default colors)
  static const std::map<std::string, std::string> s_Defaults =
  {
    { "default_color_bg", "" },
    { "default_color_fg", "" },

    { "border_attr", "" },
    { "border_color_bg", "" },
    { "border_color_fg", "" },
    { "border_focus_attr", "" },
    { "border_focus_color_bg", "" },
    { "border_focus_color_fg", "" },
    { "title_attr", "" },
    { "title_color_bg", "" },
    { "title_color_fg", "" },
    { "title_focus_attr", "bold" },
    { "title_focus_color_bg", "" },
    { "title_focus_color_fg", "" },

    { "position_attr", "" },
    { "position_color_bg", "" },
    { "position_color_fg", "" },
    { "track_attr", "" },
    { "track_color_bg", "" },
    { "track_color_fg", "" },
    { "spectrum_attr", "" },
    { "spectrum_color_bg", "" },
    { "spectrum_color_fg", "" },
    { "spectrum_peak_attr", "" },
    { "spectrum_peak_color_bg", "" },
    { "spectrum_peak_color_fg", "" },
    { "volume_attr", "" },
    { "volume_color_bg", "" },
    { "volume_color_fg", "" },
    { "volume_rail_attr", "" },
    { "volume_rail_color_bg", "" },
    { "volume_rail_color_fg", "" },
    { "progress_attr", "" },
    { "progress_color_bg", "" },
    { "progress_color_fg", "" },
    { "progress_rail_attr", "" },
    { "progress_rail_color_bg", "" },
    { "progress_rail_color_fg", "" },
    { "controls_attr", "" },
    { "controls_color_bg", "" },
    { "controls_color_fg", "" },
    { "toggle_attr", "" },
    { "toggle_color_bg", "" },
    { "toggle_color_fg", "" },
    { "scrobble_attr", "" },
    { "scrobble_color_bg", "" },
    { "scrobble_color_fg", "" },

    { "playlist_attr", "" },
    { "playlist_color_bg", "" },
    { "playlist_color_fg", "" },
    { "playlist_selected_attr", "reverse" },
    { "playlist_selected_color_bg", "" },
    { "playlist_selected_color_fg", "" },
    { "playlist_playing_attr", "" },
    { "playlist_playing_color_bg", "" },
    { "playlist_playing_color_fg", "" },
    { "queue_marker_attr", "" },
    { "queue_marker_color_bg", "" },
    { "queue_marker_color_fg", "" },
    { "folder_attr", "dim" },
    { "folder_color_bg", "" },
    { "folder_color_fg", "" },
    { "search_attr", "bold" },
    { "search_color_bg", "" },
    { "search_color_fg", "" },

    { "lyrics_attr", "" },
    { "lyrics_color_bg", "" },
    { "lyrics_color_fg", "" },
    { "lyrics_past_attr", "dim" },
    { "lyrics_past_color_bg", "" },
    { "lyrics_past_color_fg", "" },
    { "lyrics_current_attr", "bold" },
    { "lyrics_current_color_bg", "" },
    { "lyrics_current_color_fg", "" },
    { "lyrics_status_attr", "dim" },
    { "lyrics_status_color_bg", "" },
    { "lyrics_status_color_fg", "" },

    { "border_style", "square" },
    { "spectrum_chars", "\u2581\u2582\u2583\u2584\u2585\u2586\u2587" },
    { "spectrum_peak_level", "5" },
    { "bar_fill_char", "" },
    { "bar_rail_char", "" },
    { "folder_sep_char", "=" },
  };

  s_Config = s_Defaults;
  s_Attrs.clear();
  s_IsSet.clear();

  const std::string path = GetPath();
  bool needSave = true;
  QFile file(QString::fromStdString(path));
  if (file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    std::set<std::string> keys;
    while (!file.atEnd())
    {
      const QString line = QString::fromUtf8(file.readLine()).trimmed();
      if (line.isEmpty() || line.startsWith('#')) continue;

      const int sep = line.indexOf('=');
      if (sep <= 0) continue;

      const std::string key = line.left(sep).trimmed().toStdString();
      const std::string value = line.mid(sep + 1).trimmed().toStdString();
      s_Config[key] = value;
      keys.insert(key);
    }

    file.close();

    needSave = false;
    for (const auto& entry : s_Defaults)
    {
      if (keys.find(entry.first) == keys.end())
      {
        needSave = true;
        break;
      }
    }
  }

  // Write config with all keys, so they are easy to find and edit
  if (needSave)
  {
    Save(path);
  }

  if (has_colors())
  {
    start_color();
    const int fg = ParseColor(GetString("default_color_fg"));
    const int bg = ParseColor(GetString("default_color_bg"));
    assume_default_colors(fg, bg);
  }
}

int UIColorConfig::GetAttr(const std::string& p_Element)
{
  auto it = s_Attrs.find(p_Element);
  if (it != s_Attrs.end()) return it->second;

  int attr = ParseAttributes(GetString(p_Element + "_attr"));
  if (has_colors())
  {
    const int fg = ParseColor(GetString(p_Element + "_color_fg"));
    const int bg = ParseColor(GetString(p_Element + "_color_bg"));
    if ((fg != -1) || (bg != -1))
    {
      if ((s_ColorPairId + 1) < COLOR_PAIRS)
      {
        ++s_ColorPairId;
        init_pair(s_ColorPairId, fg, bg);
        attr |= COLOR_PAIR(s_ColorPairId);
      }
      else
      {
        Log::Warning("no free color pairs for \"%s\"", p_Element.c_str());
      }
    }
  }

  s_Attrs[p_Element] = attr;
  return attr;
}

int UIColorConfig::GetAttr(const std::string& p_Element, int p_Fallback)
{
  return IsSet(p_Element) ? GetAttr(p_Element) : p_Fallback;
}

bool UIColorConfig::IsSet(const std::string& p_Element)
{
  auto it = s_IsSet.find(p_Element);
  if (it != s_IsSet.end()) return it->second;

  const bool isSet = !GetString(p_Element + "_attr").empty() ||
    !GetString(p_Element + "_color_bg").empty() ||
    !GetString(p_Element + "_color_fg").empty();
  s_IsSet[p_Element] = isSet;
  return isSet;
}

std::string UIColorConfig::GetString(const std::string& p_Key)
{
  auto it = s_Config.find(p_Key);
  return (it != s_Config.end()) ? it->second : std::string();
}

std::wstring UIColorConfig::GetWString(const std::string& p_Key)
{
  return Util::ToWString(GetString(p_Key));
}

std::string UIColorConfig::GetPath()
{
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  return (dir + "/color.conf").toStdString();
}

int UIColorConfig::ParseAttributes(const std::string& p_Str)
{
  static const std::map<std::string, int> s_Attributes =
  {
    { "normal", A_NORMAL },
    { "bold", A_BOLD },
    { "dim", A_DIM },
    { "reverse", A_REVERSE },
    { "underline", A_UNDERLINE },
    { "standout", A_STANDOUT },
#ifdef A_ITALIC
    { "italic", A_ITALIC },
#endif
  };

  // Multiple attributes may be combined, e.g. "bold,underline"
  int attr = A_NORMAL;
  std::string name;
  const std::string str = p_Str + ",";
  for (const char ch : str)
  {
    if ((ch != ',') && (ch != '|') && (ch != ' '))
    {
      name += ch;
      continue;
    }

    if (name.empty()) continue;

    auto it = s_Attributes.find(name);
    if (it != s_Attributes.end())
    {
      attr |= it->second;
    }
    else
    {
      Log::Warning("unsupported attribute \"%s\"", name.c_str());
    }

    name.clear();
  }

  return attr;
}

int UIColorConfig::ParseColor(const std::string& p_Str)
{
  if (p_Str.empty()) return -1;

  static const std::map<std::string, int> s_BasicColors =
  {
    { "black", COLOR_BLACK },
    { "red", COLOR_RED },
    { "green", COLOR_GREEN },
    { "yellow", COLOR_YELLOW },
    { "blue", COLOR_BLUE },
    { "magenta", COLOR_MAGENTA },
    { "cyan", COLOR_CYAN },
    { "white", COLOR_WHITE },
  };

  auto basic = s_BasicColors.find(p_Str);
  if (basic != s_BasicColors.end()) return basic->second;

  if (COLORS > 8)
  {
    static const int BRIGHT = 8;
    static const std::map<std::string, int> s_BrightColors =
    {
      { "gray", BRIGHT | COLOR_BLACK },
      { "grey", BRIGHT | COLOR_BLACK },
      { "bright_black", BRIGHT | COLOR_BLACK },
      { "bright_red", BRIGHT | COLOR_RED },
      { "bright_green", BRIGHT | COLOR_GREEN },
      { "bright_yellow", BRIGHT | COLOR_YELLOW },
      { "bright_blue", BRIGHT | COLOR_BLUE },
      { "bright_magenta", BRIGHT | COLOR_MAGENTA },
      { "bright_cyan", BRIGHT | COLOR_CYAN },
      { "bright_white", BRIGHT | COLOR_WHITE },
    };

    auto bright = s_BrightColors.find(p_Str);
    if (bright != s_BrightColors.end()) return bright->second;
  }

  bool ok = false;
  const int id = QString::fromStdString(p_Str).toInt(&ok);
  if (ok)
  {
    if ((id >= 0) && (id < COLORS)) return id;

    Log::Warning("color %d not supported by terminal (max %d)", id, COLORS - 1);
    return -1;
  }

  Log::Warning("unsupported color \"%s\"", p_Str.c_str());
  return -1;
}

void UIColorConfig::Save(const std::string& p_Path)
{
  const QString path = QString::fromStdString(p_Path);
  QDir().mkpath(QFileInfo(path).absolutePath());

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    Log::Warning("failed to write color config %s", p_Path.c_str());
    return;
  }

  for (const auto& entry : s_Config)
  {
    file.write(QString::fromStdString(entry.first + "=" + entry.second + "\n").toUtf8());
  }

  file.close();
}
