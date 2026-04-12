// cdgwindow.cpp
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <ncurses.h>
#undef scroll
#undef clear
#undef move
#undef refresh
#undef erase
#undef timeout

#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QPainter>

#include "cdgwindow.h"
#include "log.h"

#ifdef __APPLE__
#include "macdock.h"
#endif

static const bool s_SmoothScaling = true;

CdgWindow::CdgWindow(QWidget* p_Parent)
  : QWidget(p_Parent)
  , m_Image(CDG_WIDTH, CDG_HEIGHT, QImage::Format_RGB32)
{
  m_Image.fill(Qt::black);
  setWindowTitle("namp cdg");
  resize(CDG_WIDTH * 2, CDG_HEIGHT * 2);
  setMinimumSize(CDG_WIDTH, CDG_HEIGHT);
}

void CdgWindow::TrackChanged(const QString& p_TrackPath)
{
  // Reset state
  m_Decoder = CDG();
  m_CdgData.clear();
  m_PacketCount = 0;
  m_ProcessedPackets = 0;
  m_HasCdg = false;
  m_Image.fill(Qt::black);

  // Look for .cdg sidecar file
  QFileInfo fileInfo(p_TrackPath);
  QString basePath = fileInfo.path() + "/" + fileInfo.completeBaseName();
  QString cdgPath;

  if (QFile::exists(basePath + ".cdg"))
    cdgPath = basePath + ".cdg";
  else if (QFile::exists(basePath + ".CDG"))
    cdgPath = basePath + ".CDG";

  if (cdgPath.isEmpty())
  {
    hide();
    update();
    return;
  }

  // Load CDG file
  QFile cdgFile(cdgPath);
  if (!cdgFile.open(QIODevice::ReadOnly))
  {
    Log::Warning("Failed to open CDG file: %s", cdgPath.toStdString().c_str());
    hide();
    return;
  }

  m_CdgData = cdgFile.readAll();
  cdgFile.close();

  m_PacketCount = m_CdgData.size() / static_cast<int>(sizeof(SubCode));
  m_HasCdg = true;

  Log::Info("Loaded CDG file: %s (%d packets)", cdgPath.toStdString().c_str(), m_PacketCount);

  if (m_Enabled)
  {
    show();
    raise();
  }
}

void CdgWindow::PositionChanged(qint64 p_PositionMs)
{
  if (!m_HasCdg || !isVisible()) return;

  int targetPacket = static_cast<int>(p_PositionMs * 300 / 1000);
  targetPacket = qBound(0, targetPacket, m_PacketCount);

  if (targetPacket < m_ProcessedPackets)
  {
    // Seeking backward - reset and replay
    m_Decoder = CDG();
    m_ProcessedPackets = 0;
  }

  bool needsRender = false;
  const SubCode* packets = reinterpret_cast<const SubCode*>(m_CdgData.constData());
  while (m_ProcessedPackets < targetPacket)
  {
    int dirty = 0;
    m_Decoder.execNext(packets[m_ProcessedPackets], dirty);
    if (dirty > 0) needsRender = true;
    m_ProcessedPackets++;
  }

  if (needsRender)
  {
    RenderFrame();
    update();
  }
}

void CdgWindow::SetEnabled(bool p_Enabled)
{
  m_Enabled = p_Enabled;
}

void CdgWindow::GetEnabled(bool& p_Enabled)
{
  p_Enabled = m_Enabled;
}

void CdgWindow::ToggleCdg()
{
  if (!m_HasCdg) return;

  if (isVisible())
  {
    m_Enabled = false;
    hide();
  }
  else
  {
    m_Enabled = true;
    show();
    raise();
  }
}

void CdgWindow::ToggleFullScreen()
{
  if (!isVisible()) return;
  if (isFullScreen())
    showNormal();
  else
    showFullScreen();
}

void CdgWindow::RenderFrame()
{
  uint8_t pv, ph;
  m_Decoder.getPointers(pv, ph);
  uint8_t borderIdx = m_Decoder.getBorderColor();

  uint8_t br, bg, bb, ba;
  m_Decoder.getColor(borderIdx, br, bg, bb, ba);

  for (int y = 0; y < CDG_HEIGHT; y++)
  {
    QRgb* line = reinterpret_cast<QRgb*>(m_Image.scanLine(y));
    for (int x = 0; x < CDG_WIDTH; x++)
    {
      uint8_t r, g, b, a;
      if (x < COL_MULT || x >= CDG_WIDTH - COL_MULT ||
          y < ROW_MULT || y >= CDG_HEIGHT - ROW_MULT)
      {
        r = br; g = bg; b = bb;
      }
      else
      {
        int sx = (x + ph) % CDG_WIDTH;
        int sy = (y + pv) % CDG_HEIGHT;
        uint8_t idx = m_Decoder.getPixel(sx, sy);
        m_Decoder.getColor(idx, r, g, b, a);
      }
      line[x] = qRgb(r, g, b);
    }
  }
}

void CdgWindow::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, s_SmoothScaling);
  painter.fillRect(rect(), Qt::black);

  QSize scaledSize = m_Image.size().scaled(size(), Qt::KeepAspectRatio);
  QPoint topLeft((width() - scaledSize.width()) / 2,
                 (height() - scaledSize.height()) / 2);
  painter.drawImage(QRect(topLeft, scaledSize), m_Image);
}

void CdgWindow::closeEvent(QCloseEvent* p_Event)
{
  hide();
  p_Event->ignore();
}

void CdgWindow::showEvent(QShowEvent* p_Event)
{
  QWidget::showEvent(p_Event);
#ifdef __APPLE__
  ShowDockIcon();
#endif
}

void CdgWindow::hideEvent(QHideEvent* p_Event)
{
  QWidget::hideEvent(p_Event);
#ifdef __APPLE__
  HideDockIcon();
#endif
}

void CdgWindow::keyPressEvent(QKeyEvent* p_Event)
{
  int key = -1;

  switch (p_Event->key())
  {
    case Qt::Key_Up:        key = KEY_UP; break;
    case Qt::Key_Down:      key = KEY_DOWN; break;
    case Qt::Key_Left:      key = KEY_LEFT; break;
    case Qt::Key_Right:     key = KEY_RIGHT; break;
    case Qt::Key_PageUp:    key = KEY_PPAGE; break;
    case Qt::Key_PageDown:  key = KEY_NPAGE; break;
    case Qt::Key_Home:      key = KEY_HOME; break;
    case Qt::Key_End:       key = KEY_END; break;
    case Qt::Key_Return:
    case Qt::Key_Enter:     key = '\n'; break;
    case Qt::Key_Escape:    key = 27; break;
    case Qt::Key_Tab:       key = '\t'; break;
    case Qt::Key_Backspace: key = KEY_BACKSPACE; break;
    default:
    {
      QString text = p_Event->text();
      if (text.length() == 1 && text.at(0).unicode() < 128)
        key = text.at(0).toLatin1();
      break;
    }
  }

  if (key != -1)
  {
    ungetch(key);
    emit KeyReceived();
  }
}
