// lyricsprovider.cpp
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <algorithm>
#include <climits>
#include <cstdlib>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>

#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>

#include <mpegfile.h>
#include <id3v2tag.h>
#include <unsynchronizedlyricsframe.h>
#include <synchronizedlyricsframe.h>

#include "log.h"
#include "lyricsprovider.h"
#include "version.h"

static const int s_DebounceMs = 1000;

LyricsProvider::LyricsProvider(QObject* p_Parent)
  : QObject(p_Parent)
  , m_NetworkManager(new QNetworkAccessManager(this))
{
  m_DebounceTimer.setSingleShot(true);
  connect(&m_DebounceTimer, &QTimer::timeout, this, &LyricsProvider::DoLookup);
}

void LyricsProvider::TrackChanged(const QString& p_TrackPath)
{
  m_PendingTrackPath = p_TrackPath;

  // Skip lookups entirely when the lyrics window is disabled — no point
  // hitting LRCLIB (or parsing tags) if nothing will display the result.
  if (!m_Enabled)
  {
    m_CurrentTrackPath.clear();
    m_DebounceTimer.stop();
    return;
  }

  m_CurrentTrackPath = p_TrackPath;

  // If a CDG file exists for this track, hide lyrics window immediately —
  // CDG window will take over.
  QFileInfo fileInfo(p_TrackPath);
  QString basePath = fileInfo.path() + "/" + fileInfo.completeBaseName();
  if (QFile::exists(basePath + ".cdg") || QFile::exists(basePath + ".CDG"))
  {
    emit LyricsCleared();
    return;
  }

  // Clear stale content but keep the window visible during the lookup —
  // we'll either fill it with new lyrics or hide it once we know.
  emit LyricsLoading();

  // (Re)start debounce — actual lookup happens after the user settles on a
  // track for s_DebounceMs.
  m_DebounceTimer.start(s_DebounceMs);
}

void LyricsProvider::SetEnabled(bool p_Enabled)
{
  if (p_Enabled == m_Enabled) return;
  m_Enabled = p_Enabled;

  if (!m_Enabled)
  {
    // Cancel any pending debounce and invalidate in-flight replies
    // (the reply lambdas gate on m_CurrentTrackPath matching).
    m_DebounceTimer.stop();
    m_CurrentTrackPath.clear();
    Log::Info("Lyrics lookup disabled");
    return;
  }

  Log::Info("Lyrics lookup enabled");

  // Re-enabled: kick off a lookup for whatever track is currently loaded.
  if (!m_PendingTrackPath.isEmpty())
    TrackChanged(m_PendingTrackPath);
}

void LyricsProvider::DoLookup()
{
  QString trackPath = m_CurrentTrackPath;
  if (trackPath.isEmpty()) return;

  Log::Info("Lyrics lookup starting for: %s", trackPath.toStdString().c_str());

  // CDG takes priority - if .cdg sidecar exists, skip lyrics
  QFileInfo fileInfo(trackPath);
  QString basePath = fileInfo.path() + "/" + fileInfo.completeBaseName();
  if (QFile::exists(basePath + ".cdg") || QFile::exists(basePath + ".CDG"))
  {
    Log::Info("Lyrics lookup skipped: CDG sidecar present");
    emit LyricsCleared();
    return;
  }

  // Source 1: sidecar .lrc file
  if (TryLoadSidecarLrc(trackPath))
    return;
  Log::Info("Lyrics source 1/3 (sidecar .lrc): no match");

  // Source 2: embedded lyrics in tags
  if (TryLoadEmbeddedLyrics(trackPath))
    return;
  Log::Info("Lyrics source 2/3 (embedded tags): no match");

  // Source 3: LRCLIB online fetch
  QString artist, title;
  int durationSec = 0;
  ReadTags(trackPath, artist, title, durationSec);
  if (!artist.isEmpty() && !title.isEmpty())
  {
    FetchFromLrclibGet(artist, title, durationSec);
    return;
  }

  Log::Info("Lyrics source 3/3 (LRCLIB): skipped — missing artist/title tags");
  emit LyricsCleared();
}

bool LyricsProvider::TryLoadSidecarLrc(const QString& p_TrackPath)
{
  QFileInfo fileInfo(p_TrackPath);
  QString basePath = fileInfo.path() + "/" + fileInfo.completeBaseName();
  QString lrcPath;

  if (QFile::exists(basePath + ".lrc"))
    lrcPath = basePath + ".lrc";
  else if (QFile::exists(basePath + ".LRC"))
    lrcPath = basePath + ".LRC";

  if (lrcPath.isEmpty())
    return false;

  QFile file(lrcPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    Log::Warning("Failed to open LRC file: %s", lrcPath.toStdString().c_str());
    return false;
  }

  QString content = QTextStream(&file).readAll();
  file.close();

  LyricsData lyrics = ParseLrc(content);
  if (lyrics.lines.isEmpty())
    return false;

  Log::Info("Loaded LRC file: %s (%d lines, synced=%d)",
            lrcPath.toStdString().c_str(), lyrics.lines.size(), lyrics.synced);
  emit LyricsReady(lyrics);
  return true;
}

bool LyricsProvider::TryLoadEmbeddedLyrics(const QString& p_TrackPath)
{
  // Try MPEG/ID3v2 specific frames first (SYLT and USLT)
  TagLib::MPEG::File mpegFile(p_TrackPath.toStdString().c_str());
  if (mpegFile.isValid() && mpegFile.ID3v2Tag())
  {
    TagLib::ID3v2::Tag* id3v2 = mpegFile.ID3v2Tag();

    // Check for synchronized lyrics (SYLT) first
    const auto& syltFrames = id3v2->frameListMap()["SYLT"];
    if (!syltFrames.isEmpty())
    {
      auto* sylt = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame*>(syltFrames.front());
      if (sylt)
      {
        const auto& syncText = sylt->synchedText();
        if (!syncText.isEmpty())
        {
          LyricsData lyrics;
          lyrics.synced = true;
          for (const auto& entry : syncText)
          {
            LyricsLine line;
            line.timeMs = entry.time;
            line.text = QString::fromStdString(entry.text.to8Bit(true));
            if (!line.text.trimmed().isEmpty())
              lyrics.lines.append(line);
          }
          if (!lyrics.lines.isEmpty())
          {
            Log::Info("Loaded SYLT lyrics from: %s (%d lines)",
                      p_TrackPath.toStdString().c_str(), lyrics.lines.size());
            emit LyricsReady(lyrics);
            return true;
          }
        }
      }
    }

    // Check for unsynchronized lyrics (USLT)
    const auto& usltFrames = id3v2->frameListMap()["USLT"];
    if (!usltFrames.isEmpty())
    {
      auto* uslt = dynamic_cast<TagLib::ID3v2::UnsynchronizedLyricsFrame*>(usltFrames.front());
      if (uslt)
      {
        QString text = QString::fromStdString(uslt->text().to8Bit(true));
        if (!text.trimmed().isEmpty())
        {
          // USLT content may be LRC-formatted
          LyricsData lyrics = ParseLrc(text);
          if (lyrics.lines.isEmpty())
            lyrics = ParsePlainText(text);
          if (!lyrics.lines.isEmpty())
          {
            Log::Info("Loaded USLT lyrics from: %s (%d lines, synced=%d)",
                      p_TrackPath.toStdString().c_str(), lyrics.lines.size(), lyrics.synced);
            emit LyricsReady(lyrics);
            return true;
          }
        }
      }
    }
  }

  // Generic approach via PropertyMap (works for FLAC, Ogg, MP4, etc.)
  TagLib::FileRef fileRef(p_TrackPath.toStdString().c_str());
  if (!fileRef.isNull() && fileRef.tag())
  {
    TagLib::PropertyMap props = fileRef.file()->properties();
    TagLib::StringList lyricsValues;

    if (props.contains("LYRICS"))
      lyricsValues = props["LYRICS"];
    else if (props.contains("UNSYNCED LYRICS"))
      lyricsValues = props["UNSYNCED LYRICS"];

    if (!lyricsValues.isEmpty())
    {
      QString text = QString::fromStdString(lyricsValues.front().to8Bit(true));
      if (!text.trimmed().isEmpty())
      {
        LyricsData lyrics = ParseLrc(text);
        if (lyrics.lines.isEmpty())
          lyrics = ParsePlainText(text);
        if (!lyrics.lines.isEmpty())
        {
          Log::Info("Loaded embedded lyrics from: %s (%d lines, synced=%d)",
                    p_TrackPath.toStdString().c_str(), lyrics.lines.size(), lyrics.synced);
          emit LyricsReady(lyrics);
          return true;
        }
      }
    }
  }

  return false;
}

void LyricsProvider::FetchFromLrclibGet(const QString& p_Artist, const QString& p_Title,
                                        int p_DurationSec)
{
  // Intentionally omits album_name: album tags are frequently missing or wrong
  // (remaster/compilation variants), which causes LRCLIB /api/get to 404.
  QUrl url("https://lrclib.net/api/get");
  QUrlQuery query;
  query.addQueryItem("artist_name", p_Artist);
  query.addQueryItem("track_name", p_Title);
  if (p_DurationSec > 0)
    query.addQueryItem("duration", QString::number(p_DurationSec));
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setRawHeader("User-Agent", "namp/" VERSION " (https://github.com/d99kris/namp)");

  Log::Info("Lyrics source 3a/3 (LRCLIB /api/get): querying '%s' - '%s' (dur=%ds)",
            p_Artist.toStdString().c_str(), p_Title.toStdString().c_str(), p_DurationSec);

  QNetworkReply* reply = m_NetworkManager->get(request);
  QString trackPath = m_CurrentTrackPath;

  connect(reply, &QNetworkReply::finished, this, [this, reply, trackPath, p_Artist, p_Title, p_DurationSec]()
  {
    reply->deleteLater();

    // Ignore if track changed while we were fetching
    if (trackPath != m_CurrentTrackPath)
      return;

    if (reply->error() == QNetworkReply::NoError)
    {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject())
      {
        LyricsData lyrics;
        if (ExtractLyricsFromEntry(doc.object(), lyrics))
        {
          Log::Info("Lyrics source 3a/3 (LRCLIB /api/get): matched (%d lines, synced=%d)",
                    lyrics.lines.size(), lyrics.synced);
          emit LyricsReady(lyrics);
          return;
        }
      }
      Log::Info("Lyrics source 3a/3 (LRCLIB /api/get): no match — falling back to /api/search");
    }
    else
    {
      Log::Info("Lyrics source 3a/3 (LRCLIB /api/get): request failed (%s) — falling back to /api/search",
                reply->errorString().toStdString().c_str());
    }

    // Fall back to fuzzy search
    FetchFromLrclibSearch(p_Artist, p_Title, p_DurationSec);
  });
}

void LyricsProvider::FetchFromLrclibSearch(const QString& p_Artist, const QString& p_Title,
                                           int p_DurationSec)
{
  QUrl url("https://lrclib.net/api/search");
  QUrlQuery query;
  query.addQueryItem("artist_name", p_Artist);
  query.addQueryItem("track_name", p_Title);
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setRawHeader("User-Agent", "namp/" VERSION " (https://github.com/d99kris/namp)");

  Log::Info("Lyrics source 3b/3 (LRCLIB /api/search): querying '%s' - '%s'",
            p_Artist.toStdString().c_str(), p_Title.toStdString().c_str());

  QNetworkReply* reply = m_NetworkManager->get(request);
  QString trackPath = m_CurrentTrackPath;

  connect(reply, &QNetworkReply::finished, this, [this, reply, trackPath, p_DurationSec]()
  {
    reply->deleteLater();

    if (trackPath != m_CurrentTrackPath)
      return;

    if (reply->error() != QNetworkReply::NoError)
    {
      Log::Info("Lyrics source 3b/3 (LRCLIB /api/search): request failed (%s) — giving up",
                reply->errorString().toStdString().c_str());
      emit LyricsCleared();
      return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
    {
      Log::Info("Lyrics source 3b/3 (LRCLIB /api/search): invalid response — giving up");
      emit LyricsCleared();
      return;
    }

    QJsonArray arr = doc.array();
    if (arr.isEmpty())
    {
      Log::Info("Lyrics source 3b/3 (LRCLIB /api/search): zero results — giving up");
      emit LyricsCleared();
      return;
    }

    // Rank: prefer entries closest to our duration (when known), with a
    // tiebreaker preferring entries that actually have synced lyrics.
    int bestIdx = -1;
    int bestDurDelta = INT_MAX;
    bool bestHasSynced = false;
    for (int i = 0; i < arr.size(); ++i)
    {
      if (!arr[i].isObject()) continue;
      QJsonObject entry = arr[i].toObject();
      int entryDur = entry.value("duration").toInt(-1);
      int delta = (p_DurationSec > 0 && entryDur > 0) ? std::abs(entryDur - p_DurationSec) : 0;
      bool hasSynced = !entry.value("syncedLyrics").toString().isEmpty();

      bool better = false;
      if (bestIdx < 0) better = true;
      else if (delta < bestDurDelta) better = true;
      else if (delta == bestDurDelta && hasSynced && !bestHasSynced) better = true;

      if (better)
      {
        bestIdx = i;
        bestDurDelta = delta;
        bestHasSynced = hasSynced;
      }
    }

    if (bestIdx >= 0)
    {
      LyricsData lyrics;
      if (ExtractLyricsFromEntry(arr[bestIdx].toObject(), lyrics))
      {
        Log::Info("Lyrics source 3b/3 (LRCLIB /api/search): matched %d/%d result (%d lines, synced=%d, durDelta=%ds)",
                  bestIdx + 1, (int)arr.size(), lyrics.lines.size(), lyrics.synced, bestDurDelta);
        emit LyricsReady(lyrics);
        return;
      }
    }

    Log::Info("Lyrics source 3b/3 (LRCLIB /api/search): %d results but no usable lyrics — giving up",
              (int)arr.size());
    emit LyricsCleared();
  });
}

bool LyricsProvider::ExtractLyricsFromEntry(const QJsonObject& p_Entry, LyricsData& p_OutLyrics)
{
  // Prefer synced lyrics
  QString syncedLyrics = p_Entry.value("syncedLyrics").toString();
  if (!syncedLyrics.isEmpty())
  {
    LyricsData lyrics = ParseLrc(syncedLyrics);
    if (!lyrics.lines.isEmpty())
    {
      p_OutLyrics = lyrics;
      return true;
    }
  }

  // Fall back to plain lyrics
  QString plainLyrics = p_Entry.value("plainLyrics").toString();
  if (!plainLyrics.isEmpty())
  {
    LyricsData lyrics = ParsePlainText(plainLyrics);
    if (!lyrics.lines.isEmpty())
    {
      p_OutLyrics = lyrics;
      return true;
    }
  }

  return false;
}

LyricsData LyricsProvider::ParseLrc(const QString& p_LrcText)
{
  LyricsData lyrics;
  static QRegularExpression timestampRe("\\[(\\d{1,3}):(\\d{2})(?:[.:](\\d{1,3}))?\\]");

  const QStringList rawLines = p_LrcText.split('\n');
  bool hasTimestamps = false;

  for (const QString& rawLine : rawLines)
  {
    QString line = rawLine.trimmed();
    if (line.isEmpty())
      continue;

    // Skip metadata tags like [ti:...], [ar:...], [al:...], [by:...]
    static QRegularExpression metaRe("^\\[[a-zA-Z]{2,}:.*\\]$");
    if (metaRe.match(line).hasMatch())
      continue;

    // Extract all timestamps from the line
    QVector<qint64> timestamps;
    QRegularExpressionMatchIterator it = timestampRe.globalMatch(line);
    int lastMatchEnd = 0;

    while (it.hasNext())
    {
      QRegularExpressionMatch match = it.next();
      int minutes = match.captured(1).toInt();
      int seconds = match.captured(2).toInt();
      int fraction = 0;
      QString fractionStr = match.captured(3);
      if (!fractionStr.isEmpty())
      {
        // Normalize to milliseconds: "12" -> 120, "123" -> 123, "1" -> 100
        fraction = fractionStr.toInt();
        if (fractionStr.length() == 1)
          fraction *= 100;
        else if (fractionStr.length() == 2)
          fraction *= 10;
      }

      qint64 timeMs = (minutes * 60 + seconds) * 1000 + fraction;
      timestamps.append(timeMs);
      lastMatchEnd = match.capturedEnd(0);
      hasTimestamps = true;
    }

    if (!timestamps.isEmpty())
    {
      QString text = line.mid(lastMatchEnd).trimmed();
      for (qint64 ts : timestamps)
      {
        LyricsLine lyricsLine;
        lyricsLine.timeMs = ts;
        lyricsLine.text = text;
        lyrics.lines.append(lyricsLine);
      }
    }
  }

  if (!hasTimestamps)
    return LyricsData();

  // Sort by timestamp
  std::sort(lyrics.lines.begin(), lyrics.lines.end(),
            [](const LyricsLine& a, const LyricsLine& b) { return a.timeMs < b.timeMs; });

  lyrics.synced = true;
  return lyrics;
}

LyricsData LyricsProvider::ParsePlainText(const QString& p_Text)
{
  LyricsData lyrics;
  lyrics.synced = false;

  const QStringList rawLines = p_Text.split('\n');
  for (const QString& rawLine : rawLines)
  {
    LyricsLine line;
    line.timeMs = -1;
    line.text = rawLine.trimmed();
    lyrics.lines.append(line);
  }

  // Remove trailing empty lines
  while (!lyrics.lines.isEmpty() && lyrics.lines.last().text.isEmpty())
    lyrics.lines.removeLast();

  return lyrics;
}

void LyricsProvider::ReadTags(const QString& p_TrackPath, QString& p_Artist, QString& p_Title,
                              int& p_DurationSec)
{
  TagLib::FileRef fileRef(p_TrackPath.toStdString().c_str());
  if (!fileRef.isNull() && fileRef.tag())
  {
    p_Artist = QString::fromStdString(fileRef.tag()->artist().to8Bit(true));
    p_Title = QString::fromStdString(fileRef.tag()->title().to8Bit(true));
  }
  if (!fileRef.isNull() && fileRef.audioProperties())
  {
    p_DurationSec = fileRef.audioProperties()->lengthInSeconds();
  }
}
