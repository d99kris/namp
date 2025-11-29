// util.h
//
// Copyright (c) 2022-2025 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <string>

class Util
{
public:
  static bool RunProgram(const std::string& p_Cmd);
  static std::string ToString(const std::wstring& p_WStr);
  static std::wstring ToWString(const std::string& p_Str);
  static std::wstring TrimPadWString(const std::wstring& p_Str, int p_Len);
  static int WStringWidth(const std::wstring& p_WStr);
};
