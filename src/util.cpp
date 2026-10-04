// util.cpp
//
// Copyright (c) 2022-2025 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include "util.h"

#include <algorithm>

#include <ncurses.h>

#include <QString>

bool Util::RunProgram(const std::string& p_Cmd)
{
  endwin();

  int rv = system(p_Cmd.c_str());

  refresh();
  wint_t key = 0;
  while (get_wch(&key) != ERR)
  {
    // Discard any remaining input
  }

  return (rv == 0);
}

std::string Util::ToString(const std::wstring& p_WStr)
{
  return QString::fromStdWString(p_WStr).toStdString();
}

std::wstring Util::ToWString(const std::string& p_Str)
{
  return QString::fromStdString(p_Str).toStdWString();
}

std::wstring Util::TrimPadWString(const std::wstring& p_Str, int p_Len)
{
  p_Len = std::max(p_Len, 0);
  std::wstring str = p_Str;
  if (WStringWidth(str) > p_Len)
  {
    str = str.substr(0, p_Len);
    int subLen = p_Len;
    while (WStringWidth(str) > p_Len)
    {
      str = str.substr(0, --subLen);
    }
  }
  else if (WStringWidth(str) < p_Len)
  {
    str = str + std::wstring(p_Len - WStringWidth(str), ' ');
  }
  return str;
}

int Util::WStringWidth(const std::wstring& p_WStr)
{
  int width = wcswidth(p_WStr.c_str(), p_WStr.size());
  return (width != -1) ? width : p_WStr.size();
}
