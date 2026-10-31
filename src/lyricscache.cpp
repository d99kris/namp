// lyricscache.cpp
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

#include "log.h"
#include "lyricscache.h"

static const int s_Version = 1;
static const int s_NotFoundTtlDays = 30;
static const int s_SlugMaxLen = 40;
static const int s_HashLen = 8;

bool LyricsCache::Lookup(const QString& p_Artist, const QString& p_Title, int p_DurationSec,
                         QJsonObject& p_Entry, bool& p_NotFound)
{
  const QJsonObject query = MakeQuery(p_Artist, p_Title, p_DurationSec);
  const QString path = GetPath(query);
  if (path.isEmpty()) return false;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return false;

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (!doc.isObject()) return false;

  // The filename hash is short, so verify the stored query to rule out a
  // collision with another track.
  const QJsonObject obj = doc.object();
  if ((obj.value("version").toInt() != s_Version) || (obj.value("query").toObject() != query))
    return false;

  const QString status = obj.value("status").toString();
  if (status == "found")
  {
    p_Entry = obj.value("entry").toObject();
    p_NotFound = false;
    return !p_Entry.isEmpty();
  }

  if (status == "notfound")
  {
    // Expire not-found entries, as lyrics may have been contributed since
    const QDateTime fetched = QDateTime::fromString(obj.value("fetched").toString(), Qt::ISODate);
    if (!fetched.isValid() || (fetched.addDays(s_NotFoundTtlDays) < QDateTime::currentDateTimeUtc()))
      return false;

    p_Entry = QJsonObject();
    p_NotFound = true;
    return true;
  }

  return false;
}

void LyricsCache::StoreFound(const QString& p_Artist, const QString& p_Title, int p_DurationSec,
                             const QJsonObject& p_Entry)
{
  Store(p_Artist, p_Title, p_DurationSec, &p_Entry);
}

void LyricsCache::StoreNotFound(const QString& p_Artist, const QString& p_Title, int p_DurationSec)
{
  Store(p_Artist, p_Title, p_DurationSec, nullptr);
}

QString LyricsCache::GetDir()
{
  const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (cacheDir.isEmpty()) return QString();

  return cacheDir + "/lyrics";
}

bool LyricsCache::Clear()
{
  const QString dirPath = GetDir();
  if (dirPath.isEmpty()) return false;

  QDir dir(dirPath);
  if (!dir.exists()) return true;

  return dir.removeRecursively();
}

void LyricsCache::Store(const QString& p_Artist, const QString& p_Title, int p_DurationSec,
                        const QJsonObject* p_Entry)
{
  const QJsonObject query = MakeQuery(p_Artist, p_Title, p_DurationSec);
  const QString path = GetPath(query);
  if (path.isEmpty()) return;

  if (!QDir().mkpath(GetDir()))
  {
    Log::Warning("Failed to create lyrics cache dir: %s", GetDir().toStdString().c_str());
    return;
  }

  QJsonObject obj;
  obj.insert("version", s_Version);
  obj.insert("fetched", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
  obj.insert("query", query);
  obj.insert("status", (p_Entry != nullptr) ? "found" : "notfound");
  if (p_Entry != nullptr)
  {
    obj.insert("entry", *p_Entry);
  }

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      (file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented)) < 0) ||
      !file.commit())
  {
    Log::Warning("Failed to write lyrics cache file: %s", path.toStdString().c_str());
    return;
  }

  Log::Info("Lyrics cache stored (%s): %s", (p_Entry != nullptr) ? "found" : "notfound",
            path.toStdString().c_str());
}

QJsonObject LyricsCache::MakeQuery(const QString& p_Artist, const QString& p_Title, int p_DurationSec)
{
  QJsonObject query;
  query.insert("artist", p_Artist.trimmed().toLower());
  query.insert("title", p_Title.trimmed().toLower());
  query.insert("duration", p_DurationSec);
  return query;
}

QString LyricsCache::GetPath(const QJsonObject& p_Query)
{
  const QString dirPath = GetDir();
  if (dirPath.isEmpty()) return QString();

  const QString artist = p_Query.value("artist").toString();
  const QString title = p_Query.value("title").toString();
  const int durationSec = p_Query.value("duration").toInt();

  // Readable prefix for browsing the cache, with a hash of the full query
  // for uniqueness, e.g. the_beatles-let_it_be-8b02d4f7.json
  const QString key = artist + "\n" + title + "\n" + QString::number(durationSec);
  const QString hash =
    QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(s_HashLen));

  QStringList parts;
  for (const QString& slug : { Slugify(artist), Slugify(title) })
  {
    if (!slug.isEmpty())
    {
      parts.append(slug);
    }
  }
  parts.append(hash);

  return dirPath + "/" + parts.join("-") + ".json";
}

QString LyricsCache::Slugify(const QString& p_Str)
{
  // Decompose accented characters (é -> e + combining accent) and keep only
  // ascii alphanumerics, turning everything else into single underscores.
  const QString decomposed = p_Str.normalized(QString::NormalizationForm_KD).toLower();
  QString slug;
  for (const QChar ch : decomposed)
  {
    if (ch.category() == QChar::Mark_NonSpacing) continue;

    const ushort code = ch.unicode();
    if (((code >= 'a') && (code <= 'z')) || ((code >= '0') && (code <= '9')))
    {
      slug += ch;
    }
    else if (!slug.isEmpty() && !slug.endsWith('_'))
    {
      slug += '_';
    }
  }

  slug.truncate(s_SlugMaxLen);
  while (slug.endsWith('_'))
  {
    slug.chop(1);
  }

  return slug;
}
