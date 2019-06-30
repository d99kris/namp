// scrobbler.cpp
//
// Copyright (C) 2019 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QCryptographicHash>
#include <QEventLoop>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>

#include <iostream>
#include <sstream>

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "log.h"
#include "scrobbler.h"

Scrobbler::Scrobbler(QObject *p_Parent, const std::string& p_User, const std::string& p_Pass)
  : QObject(p_Parent)
  , m_User(p_User)
  , m_Pass(p_Pass)
  , m_App("NAMP")
  , m_AppVer("1.0")
  , m_NetworkManager(new QNetworkAccessManager(this))
{
  connect(m_NetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(OnFinished(QNetworkReply*)));
  Connect();
}

Scrobbler::~Scrobbler()
{
  delete m_NetworkManager;
}

void Scrobbler::Connect()
{
  unsigned int now = time(NULL);
  std::string auth = MD5(m_Pass + std::to_string(now));
  std::stringstream ss;
  ss << "http://post.audioscrobbler.com/";
  ss << "?hs=true&p=1.2.1&c=" << m_App << "&v=" << m_AppVer << "&u=";
  ss << m_User << "&t=" << now << "&a=" << auth;
  
  HttpRequest(ss.str());
}

void Scrobbler::Playing(const QString& p_Artist, const QString& p_Title, int p_Duration)
{
  if (p_Artist.isEmpty() || p_Title.isEmpty())
  {
    return;
  }
  
  const QByteArray& artist = QUrl::toPercentEncoding(p_Artist);
  std::string stdStrArtist(artist.constData(), artist.length());

  const QByteArray& title = QUrl::toPercentEncoding(p_Title);
  std::string stdStrTitle(title.constData(), title.length());

  std::string post =
    "c=" + m_App + "&v=" + m_AppVer +
    "&s=" + m_SessionId + "&a=" + stdStrArtist +
    "&t=" + stdStrTitle + "&l=" + std::to_string(p_Duration) + "&b=&n=&m=";

  HttpRequest(m_PlayingUrl, post);  
}

void Scrobbler::Played(const QString& p_Artist, const QString& p_Title, int p_Duration)
{
  if (p_Artist.isEmpty() || p_Title.isEmpty())
  {
    return;
  }
  
  const QByteArray& artist = QUrl::toPercentEncoding(p_Artist);
  std::string stdStrArtist(artist.constData(), artist.length());

  const QByteArray& title = QUrl::toPercentEncoding(p_Title);
  std::string stdStrTitle(title.constData(), title.length());

  unsigned int now = time(NULL);
  
  std::string post =
    "c=" + m_App + "&v=" + m_AppVer +
    "&s=" + m_SessionId + "&a[0]=" + stdStrArtist +
    "&t[0]=" + stdStrTitle + "&i[0]=" + std::to_string(now) +
    "&o[0]=P&r[0]=" +
    "&l[0]=" + std::to_string(p_Duration) + "&b[0]=&n[0]=&m[0]=";

  HttpRequest(m_PlayedUrl, post);  
}

std::string Scrobbler::GetPass()
{
  std::string pass;
  struct termios told, tnew;

  if (tcgetattr(STDIN_FILENO, &told) == 0)
  {
    memcpy(&tnew, &told, sizeof(struct termios));
    tnew.c_lflag &= ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tnew) == 0)
    {
      std::getline(std::cin, pass);
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &told);
      std::cout << std::endl;
    }
  }

  return pass;  
}

std::string Scrobbler::MD5(const std::string& p_Str)
{
  QString md5 =
    QString("%1").arg(QString(QCryptographicHash::hash(QString::fromStdString(p_Str).toUtf8(),
                                                       QCryptographicHash::Md5).toHex()));
  return md5.toStdString();
}

void Scrobbler::HttpRequest(const std::string& p_Url, const std::string& p_Post /* = "" */)
{
  QString urlstr = QString::fromStdString(p_Url);
  QUrl url(urlstr);
  
  if (!p_Post.empty())
  {
    const QByteArray postData(p_Post.c_str(), p_Post.length());
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));
    m_NetworkManager->post(req, postData);
  }
  else
  {
    QNetworkRequest req(url);
    m_NetworkManager->get(req);
  }
}

void Scrobbler::OnFinished(QNetworkReply* p_Reply)
{
  QUrl url = p_Reply->url();
  std::string surl = url.toString().toStdString();
  QByteArray byteArray = p_Reply->readAll();
  std::string stdString(byteArray.constData(), byteArray.length());

  if (surl.find("http://post.audioscrobbler.com/?hs=true") == 0) // auth
  {
    std::vector<std::string> result = Split(stdString);
    if (result.size() < 4)
    {
      Log::Warning("Unexpected scrobbler response size");
    }
    else if (result[0] != "OK")
    {
      Log::Warning("Unexpected scrobbler response %s", result[0].c_str());
    }
    else
    {
      m_SessionId = result[1];
      m_PlayingUrl = result[2];
      m_PlayedUrl = result[3];
      m_Connected = true;
    }
  }
  else if (surl.find(m_PlayingUrl) == 0) // playing
  {
    if (stdString.find("OK\n") != 0)
    {
      Log::Warning("Unexpected scrobbler response %s", stdString.c_str());
    }
  }
  else if (surl.find(m_PlayedUrl) == 0) // played
  {
    if (stdString.find("OK\n") != 0)
    {
      Log::Warning("Unexpected scrobbler response %s", stdString.c_str());
    }
  }
}

std::vector<std::string> Scrobbler::Split(const std::string &p_Str, char p_Sep /* = '\n' */)
{
  std::vector<std::string> vec;
  if (!p_Str.empty())
  {
    std::stringstream ss(p_Str);
    while (ss.good())
    {
      std::string str;
      getline(ss, str, p_Sep);
      vec.push_back(str);
    }
  }
  return vec;
}
