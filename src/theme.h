// theme.h
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <cstddef>
#include <string>

class Theme
{
public:
  // Install bundled theme p_Name as color config, or prompt the user to
  // select one if p_Name is empty. Returns true on success.
  static bool Install(const std::string& p_Name);

private:
  static bool Select(size_t& p_Index);
  static bool Find(const std::string& p_Name, size_t& p_Index);
};
