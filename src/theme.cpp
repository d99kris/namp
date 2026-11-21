// theme.cpp
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include "theme.h"

#include <iostream>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include "themes.h"
#include "uicolorconfig.h"

static const size_t themeCount = sizeof(EmbeddedThemes) / sizeof(EmbeddedThemes[0]);

bool Theme::Install(const std::string& p_Name)
{
  size_t index = 0;
  if (p_Name.empty())
  {
    if (!Select(index)) return false;
  }
  else if (!Find(p_Name, index))
  {
    std::cerr << "error: theme \"" << p_Name << "\" does not exist, run \"namp --theme\" for a list\n"
      "       of available themes.\n";
    return false;
  }

  const EmbeddedTheme& theme = EmbeddedThemes[index];
  const std::string path = UIColorConfig::GetPath();
  const QString qpath = QString::fromStdString(path);
  QDir().mkpath(QFileInfo(qpath).absolutePath());

  QFile file(qpath);
  const qint64 size = static_cast<qint64>(theme.size);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      (file.write(reinterpret_cast<const char*>(theme.data), size) != size))
  {
    std::cerr << "error: failed to write " << path << "\n";
    return false;
  }

  file.close();
  std::cout << "Installed theme \"" << theme.name << "\" to " << path << "\n";
  return true;
}

bool Theme::Select(size_t& p_Index)
{
  // first theme in index.txt is the default selection
  std::cout << "Themes:\n";
  for (size_t i = 0; i < themeCount; ++i)
  {
    std::cout << (i + 1) << ". " << EmbeddedThemes[i].name << "\n";
  }

  std::cout << "Press ctrl-c to cancel.\n";

  std::cout << "Select theme (1): ";
  std::string line;
  if (!std::getline(std::cin, line))
  {
    std::cout << "\n";
    return false;
  }

  line = QString::fromStdString(line).trimmed().toStdString();
  if (line.empty())
  {
    p_Index = 0;
    return true;
  }

  const bool isNumber = (line.find_first_not_of("0123456789") == std::string::npos) && (line.size() < 6);
  const size_t number = isNumber ? std::stoul(line) : 0;
  if ((number < 1) || (number > themeCount))
  {
    std::cerr << "error: invalid selection \"" << line << "\".\n";
    return false;
  }

  p_Index = number - 1;
  return true;
}

bool Theme::Find(const std::string& p_Name, size_t& p_Index)
{
  for (size_t i = 0; i < themeCount; ++i)
  {
    if (p_Name == EmbeddedThemes[i].name)
    {
      p_Index = i;
      return true;
    }
  }

  return false;
}
