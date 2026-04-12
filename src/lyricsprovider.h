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
  bool m_Enabled = false;
  QTimer m_DebounceTimer;
};
