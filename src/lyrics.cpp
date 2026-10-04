// lyrics.cpp
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include "lyrics.h"

int Lyrics::FindCurrentLine(const LyricsData& p_Lyrics, qint64 p_PositionMs)
{
  int result = -1;
  for (int i = 0; i < p_Lyrics.lines.size(); i++)
  {
    if (p_Lyrics.lines[i].timeMs <= p_PositionMs)
      result = i;
    else
      break;
  }
  return result;
}

bool Lyrics::AssignSyntheticTimestamps(LyricsData& p_Lyrics, qint64 p_DurationMs)
{
  if (p_Lyrics.lines.isEmpty() || p_DurationMs <= 0) return false;

  // Weight each line by character count so long verses get more time than
  // short chorus markers. +1 keeps empty lines from being instantaneous.
  long long totalWeight = 0;
  QVector<long long> weights(p_Lyrics.lines.size());
  for (int i = 0; i < p_Lyrics.lines.size(); i++)
  {
    weights[i] = p_Lyrics.lines[i].text.length() + 1;
    totalWeight += weights[i];
  }
  if (totalWeight <= 0) return false;

  long long accum = 0;
  for (int i = 0; i < p_Lyrics.lines.size(); i++)
  {
    p_Lyrics.lines[i].timeMs = (p_DurationMs * accum) / totalWeight;
    accum += weights[i];
  }

  p_Lyrics.synced = true;
  return true;
}
