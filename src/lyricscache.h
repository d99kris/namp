// lyricscache.h
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QJsonObject>
#include <QString>

// Disk cache of LRCLIB lookups, keyed by the query (artist, title, duration).
// Each entry is a JSON file holding the matched LRCLIB entry verbatim, or a
// not-found marker which expires so that newly contributed lyrics are found.
class LyricsCache
{
public:
  static bool Lookup(const QString& p_Artist, const QString& p_Title, int p_DurationSec,
                     QJsonObject& p_Entry, bool& p_NotFound);
  static void StoreFound(const QString& p_Artist, const QString& p_Title, int p_DurationSec,
                         const QJsonObject& p_Entry);
  static void StoreNotFound(const QString& p_Artist, const QString& p_Title, int p_DurationSec);
  static QString GetDir();
  static bool Clear();

private:
  static void Store(const QString& p_Artist, const QString& p_Title, int p_DurationSec,
                    const QJsonObject* p_Entry);
  static QJsonObject MakeQuery(const QString& p_Artist, const QString& p_Title, int p_DurationSec);
  static QString GetPath(const QJsonObject& p_Query);
  static QString Slugify(const QString& p_Str);
};
