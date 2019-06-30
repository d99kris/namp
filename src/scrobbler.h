// scrobbler.h
//
// Copyright (C) 2019 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QNetworkReply>
#include <QObject>

#include <thread>

class Scrobbler : public QObject
{
  Q_OBJECT

public:
  Scrobbler(QObject* p_Parent, const std::string& p_User, const std::string& p_Pass);
  ~Scrobbler();

  void Connect();
  void Playing(const QString& p_Artist, const QString& p_Title, int p_Duration);
  void Played(const QString& p_Artist, const QString& p_Title, int p_Duration);

  static std::string GetPass();
  static std::string MD5(const std::string& p_Str);

private:
  void HttpRequest(const std::string& p_Url, const std::string& p_Post = "");
  std::vector<std::string> Split(const std::string& p_Str, char p_Sep = '\n');

private slots:
  void OnFinished(QNetworkReply* p_Reply);

private:
  std::string m_User;
  std::string m_Pass;

  std::string m_SessionId;
  std::string m_PlayingUrl;
  std::string m_PlayedUrl;

  std::string m_App;
  std::string m_AppVer;
  
  bool m_Running = false;
  bool m_Connected = false;
  QNetworkAccessManager* m_NetworkManager = NULL;
};
