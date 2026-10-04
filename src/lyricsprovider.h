// lyricsprovider.h
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include "lyrics.h"

class LyricsProvider : public QObject
{
  Q_OBJECT

public:
  LyricsProvider(QObject* p_Parent = nullptr);

public slots:
  void TrackChanged(const QString& p_TrackPath);
  void SetEnabled(bool p_Enabled);

signals:
  void LyricsReady(const LyricsData& p_Lyrics);
  void LyricsCleared();
  void LyricsLoading();

private:
  void DoLookup();
  void EmitLyricsReady(const LyricsData& p_Lyrics);
  bool TryLoadSidecarLrc(const QString& p_TrackPath);
  bool TryLoadEmbeddedLyrics(const QString& p_TrackPath);
  void FetchFromLrclibGet(const QString& p_Artist, const QString& p_Title, int p_DurationSec);
  void FetchFromLrclibSearch(const QString& p_Artist, const QString& p_Title, int p_DurationSec);
  static bool ExtractLyricsFromEntry(const QJsonObject& p_Entry, LyricsData& p_OutLyrics);

  static LyricsData ParseLrc(const QString& p_LrcText);
  static LyricsData ParsePlainText(const QString& p_Text);

  void ReadTags(const QString& p_TrackPath, QString& p_Artist, QString& p_Title,
                int& p_DurationSec);

  QNetworkAccessManager* m_NetworkManager = nullptr;
  QString m_CurrentTrackPath;
  QString m_PendingTrackPath;
  QString m_CachedTrackPath;
  LyricsData m_CachedLyrics;
  bool m_Enabled = false;
  QTimer m_DebounceTimer;
};
