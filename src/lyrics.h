// lyrics.h
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

struct LyricsLine
{
  qint64 timeMs = -1;
  QString text;
};

struct LyricsData
{
  bool synced = false;
  QVector<LyricsLine> lines;
};

Q_DECLARE_METATYPE(LyricsData)

class Lyrics
{
public:
  static int FindCurrentLine(const LyricsData& p_Lyrics, qint64 p_PositionMs);
  static bool AssignSyntheticTimestamps(LyricsData& p_Lyrics, qint64 p_DurationMs);
};
