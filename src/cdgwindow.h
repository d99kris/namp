// cdgwindow.h
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QByteArray>
#include <QImage>
#include <QWidget>

#include "cdg.h"

class CdgWindow : public QWidget
{
  Q_OBJECT

public:
  CdgWindow(QWidget* p_Parent = nullptr);

public slots:
  void TrackChanged(const QString& p_TrackPath);
  void PositionChanged(qint64 p_PositionMs);
  void ToggleCdg();

signals:
  void KeyReceived();

protected:
  void paintEvent(QPaintEvent* p_Event) override;
  void closeEvent(QCloseEvent* p_Event) override;
  void showEvent(QShowEvent* p_Event) override;
  void hideEvent(QHideEvent* p_Event) override;
  void keyPressEvent(QKeyEvent* p_Event) override;

private:
  void RenderFrame();

  CDG m_Decoder;
  QByteArray m_CdgData;
  int m_PacketCount = 0;
  int m_ProcessedPackets = 0;
  QImage m_Image;
  bool m_HasCdg = false;
};
