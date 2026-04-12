// lyricswindow.h
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QTimer>
#include <QWidget>

#include "lyricsprovider.h"

class LyricsWindow : public QWidget
{
  Q_OBJECT

public:
  LyricsWindow(QWidget* p_Parent = nullptr);
  void SetEnabled(bool p_Enabled);
  void GetEnabled(bool& p_Enabled);
  void SetFontScale(float p_Scale);
  void GetFontScale(float& p_Scale);
  float GetDefaultFontScale() const;

public slots:
  void SetLyrics(const LyricsData& p_Lyrics);
  void ClearLyrics();
  void LyricsLoading();
  void PositionChanged(qint64 p_PositionMs);
  void DurationChanged(qint64 p_DurationMs);
  void ToggleLyrics();
  void ToggleFullScreen();
  void ZoomIn();
  void ZoomOut();
  void ZoomReset();

signals:
  void KeyReceived();
  void EnabledChanged(bool p_Enabled);

protected:
  void paintEvent(QPaintEvent* p_Event) override;
  void closeEvent(QCloseEvent* p_Event) override;
  void showEvent(QShowEvent* p_Event) override;
  void hideEvent(QHideEvent* p_Event) override;
  void keyPressEvent(QKeyEvent* p_Event) override;
  void resizeEvent(QResizeEvent* p_Event) override;

private:
  int FindCurrentLine(qint64 p_PositionMs) const;
  float ComputeScrollTarget() const;
  float ComputeUnsyncedScrollTarget() const;
  void AssignSyntheticTimestamps();

  LyricsData m_Lyrics;
  bool m_HasLyrics = false;
  bool m_Enabled = false;
  int m_CurrentLine = -1;
  qint64 m_DurationMs = 0;
  qint64 m_PositionMs = 0;
  qint64 m_UnsyncedRatchetPosMs = -1;
  bool m_UsesSyntheticTimestamps = false;
  float m_ScrollTarget = 0.0f;
  float m_ScrollCurrent = 0.0f;
  float m_FontScale = 1.5f;
  float m_FontScaleDefault = 1.5f;
  QTimer m_ScrollTimer;
};
