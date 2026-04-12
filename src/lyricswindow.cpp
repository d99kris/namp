// lyricswindow.cpp
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

#include <cmath>

#include <QCloseEvent>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QScreen>

#include "lyricswindow.h"
#include "log.h"

#ifdef __APPLE__
#include "macdock.h"
#endif

static const int s_WindowWidth = 500;
static const int s_WindowHeight = 400;
static const int s_Margin = 20;
static const float s_ScrollSpeed = 0.15f;
static const float s_ScrollThreshold = 0.5f;
static const int s_ScrollIntervalMs = 16;
static const int s_BaseFontSize = 14;
static const int s_BaseBoldFontSize = 16;
static const int s_LinePadding = 6;
static const bool s_CurrentLineLarger = false;
static const bool s_CenterText = true;
static const float s_FontScaleStep = 0.1f;
static const float s_FontScaleMin = 0.1f;
static const float s_FontScaleMax = 10.0f;
#ifdef __APPLE__
static const float s_FontScaleDefault = 1.5f;
#else
static const float s_FontScaleDefault = 1.4f;
#endif
// Anchor position for non-timestamped (synthetic) lyrics: fraction of the
// window height that lies BELOW the current line's anchor point.
// 0.5 = centered (matches synced lyrics), 0.75 = anchor near the upper
// quarter so the first line starts higher with more lyrics visible below.
static const float s_SyntheticAnchorFraction = 0.70f;

static int WrapHeight(const QFontMetrics& p_Fm, int p_ViewWidth, const QString& p_Text)
{
  if (p_Text.isEmpty())
    return p_Fm.height() + s_LinePadding;
  return p_Fm.boundingRect(QRect(0, 0, p_ViewWidth, 0),
                           Qt::TextWordWrap | Qt::AlignLeft, p_Text).height() + s_LinePadding;
}

LyricsWindow::LyricsWindow(QWidget* p_Parent)
  : QWidget(p_Parent)
{
  // Adjust default font scale for display DPI. The base default (1.5) was
  // tuned on macOS which uses 72 logical DPI; Qt converts point sizes to
  // pixels via logicalDPI/72, so on Linux (typically 96 DPI) the same point
  // size renders ~33% larger. Compensate so text looks the same relative
  // size everywhere.
  qreal dpi = 72.0;
  QScreen* screen = QGuiApplication::primaryScreen();
  if (screen)
    dpi = screen->logicalDotsPerInch();
  m_FontScaleDefault = s_FontScaleDefault * 72.0f / static_cast<float>(dpi);
  m_FontScale = m_FontScaleDefault;

  setWindowTitle("namp lyrics");
  resize(s_WindowWidth, s_WindowHeight);
  setMinimumSize(300, 200);

  connect(&m_ScrollTimer, &QTimer::timeout, this, [this]()
  {
    float diff = m_ScrollTarget - m_ScrollCurrent;
    if (std::abs(diff) < s_ScrollThreshold)
    {
      m_ScrollCurrent = m_ScrollTarget;
      m_ScrollTimer.stop();
    }
    else
    {
      m_ScrollCurrent += diff * s_ScrollSpeed;
    }
    update();
  });
}

void LyricsWindow::SetLyrics(const LyricsData& p_Lyrics)
{
  m_Lyrics = p_Lyrics;
  m_HasLyrics = !p_Lyrics.lines.isEmpty();
  m_CurrentLine = -1;
  m_UnsyncedRatchetPosMs = -1;
  m_UsesSyntheticTimestamps = false;
  m_ScrollTarget = 0.0f;
  m_ScrollCurrent = 0.0f;
  m_ScrollTimer.stop();

  // If the lyrics came in without timestamps, synthesize them across the
  // known duration so the synced-lyrics code path (step-per-line with ease)
  // handles rendering. Avoids chasing a continuously-moving scroll target.
  if (m_HasLyrics && !m_Lyrics.synced && m_DurationMs > 0)
    AssignSyntheticTimestamps();

  if (m_HasLyrics && m_Enabled)
  {
    show();
    raise();
  }

  if (m_HasLyrics && !m_Lyrics.synced)
  {
    // Duration still unknown — fall back to position-based plain scroll,
    // pinned to first-line-center so the opening view matches synced layout.
    m_UnsyncedRatchetPosMs = 0;
    m_ScrollTarget = ComputeUnsyncedScrollTarget();
    m_ScrollCurrent = m_ScrollTarget;
  }

  update();
}

void LyricsWindow::ClearLyrics()
{
  m_Lyrics = LyricsData();
  m_HasLyrics = false;
  m_CurrentLine = -1;
  m_UnsyncedRatchetPosMs = -1;
  m_UsesSyntheticTimestamps = false;
  m_ScrollTarget = 0.0f;
  m_ScrollCurrent = 0.0f;
  m_ScrollTimer.stop();
  hide();
  update();
}

void LyricsWindow::LyricsLoading()
{
  // Clear stale content but keep the window visible if it currently is —
  // we'll either fill it with new lyrics or hide it once the lookup
  // completes.
  m_Lyrics = LyricsData();
  m_HasLyrics = false;
  m_CurrentLine = -1;
  m_UnsyncedRatchetPosMs = -1;
  m_UsesSyntheticTimestamps = false;
  m_ScrollTarget = 0.0f;
  m_ScrollCurrent = 0.0f;
  m_ScrollTimer.stop();
  update();
}

void LyricsWindow::PositionChanged(qint64 p_PositionMs)
{
  m_PositionMs = p_PositionMs;

  if (!m_HasLyrics || !isVisible()) return;

  if (m_Lyrics.synced)
  {
    int newLine = FindCurrentLine(p_PositionMs);
    if (newLine != m_CurrentLine)
    {
      m_CurrentLine = newLine;
      m_ScrollTarget = ComputeScrollTarget();

      if (!m_ScrollTimer.isActive())
        m_ScrollTimer.start(s_ScrollIntervalMs);
    }
  }
  else
  {
    // Ratchet position forward: ignore small backwards steps from Qt's
    // position polling jitter, but accept larger drops as real user seeks.
    static const qint64 seekBackThresholdMs = 2000;
    if (m_UnsyncedRatchetPosMs < 0
        || p_PositionMs >= m_UnsyncedRatchetPosMs
        || p_PositionMs < m_UnsyncedRatchetPosMs - seekBackThresholdMs)
    {
      m_UnsyncedRatchetPosMs = p_PositionMs;
      m_ScrollTarget = ComputeUnsyncedScrollTarget();
      if (!m_ScrollTimer.isActive())
        m_ScrollTimer.start(s_ScrollIntervalMs);
    }
  }
}

float LyricsWindow::ComputeScrollTarget() const
{
  if (m_CurrentLine < 0 || !m_Lyrics.synced) return 0.0f;

  int fontSize = static_cast<int>(s_BaseFontSize * m_FontScale);
  int boldFontSize = s_CurrentLineLarger
                       ? static_cast<int>(s_BaseBoldFontSize * m_FontScale)
                       : fontSize;
  QFont font("Sans", fontSize);
  QFont fontBold("Sans", boldFontSize, QFont::Bold);
  QFontMetrics fm(font);
  QFontMetrics fmBold(fontBold);
  int viewWidth = width() - 2 * s_Margin;

  float targetY = 0;
  for (int i = 0; i < m_CurrentLine; i++)
    targetY += WrapHeight(fm, viewWidth, m_Lyrics.lines[i].text);
  targetY += WrapHeight(fmBold, viewWidth, m_Lyrics.lines[m_CurrentLine].text) / 2.0f;
  return targetY;
}

float LyricsWindow::ComputeUnsyncedScrollTarget() const
{
  if (m_Lyrics.synced || m_Lyrics.lines.isEmpty()) return 0.0f;

  int fontSize = static_cast<int>(s_BaseFontSize * m_FontScale);
  QFont font("Sans", fontSize);
  QFontMetrics fm(font);
  int viewWidth = width() - 2 * s_Margin;

  int totalContentHeight = 0;
  int firstHeight = WrapHeight(fm, viewWidth, m_Lyrics.lines.first().text);
  int lastHeight = WrapHeight(fm, viewWidth, m_Lyrics.lines.last().text);
  for (int i = 0; i < m_Lyrics.lines.size(); i++)
    totalContentHeight += WrapHeight(fm, viewWidth, m_Lyrics.lines[i].text);

  float firstCenter = firstHeight / 2.0f;
  float lastCenter = totalContentHeight - lastHeight / 2.0f;

  float fraction = 0.0f;
  if (m_DurationMs > 0 && m_UnsyncedRatchetPosMs >= 0)
    fraction = qBound(0.0f, static_cast<float>(m_UnsyncedRatchetPosMs) / m_DurationMs, 1.0f);

  return firstCenter + fraction * (lastCenter - firstCenter);
}

void LyricsWindow::DurationChanged(qint64 p_DurationMs)
{
  m_DurationMs = p_DurationMs;

  // If plain lyrics arrived before the duration was known, synthesize
  // timestamps now so they render via the synced code path.
  if (m_HasLyrics && !m_Lyrics.synced && m_DurationMs > 0)
  {
    AssignSyntheticTimestamps();
    m_ScrollTarget = ComputeScrollTarget();
    m_ScrollCurrent = m_ScrollTarget;
    update();
  }
}

void LyricsWindow::AssignSyntheticTimestamps()
{
  if (m_Lyrics.lines.isEmpty() || m_DurationMs <= 0) return;

  // Weight each line by character count so long verses get more time than
  // short chorus markers. +1 keeps empty lines from being instantaneous.
  long long totalWeight = 0;
  QVector<long long> weights(m_Lyrics.lines.size());
  for (int i = 0; i < m_Lyrics.lines.size(); i++)
  {
    weights[i] = m_Lyrics.lines[i].text.length() + 1;
    totalWeight += weights[i];
  }
  if (totalWeight <= 0) return;

  long long accum = 0;
  for (int i = 0; i < m_Lyrics.lines.size(); i++)
  {
    m_Lyrics.lines[i].timeMs = (m_DurationMs * accum) / totalWeight;
    accum += weights[i];
  }

  m_Lyrics.synced = true;
  m_UsesSyntheticTimestamps = true;
}

void LyricsWindow::SetEnabled(bool p_Enabled)
{
  if (p_Enabled == m_Enabled) return;
  m_Enabled = p_Enabled;
  emit EnabledChanged(m_Enabled);
}

void LyricsWindow::GetEnabled(bool& p_Enabled)
{
  p_Enabled = m_Enabled;
}

void LyricsWindow::SetFontScale(float p_Scale)
{
  m_FontScale = qBound(s_FontScaleMin, p_Scale, s_FontScaleMax);
  m_ScrollTarget = (m_HasLyrics && !m_Lyrics.synced)
                     ? ComputeUnsyncedScrollTarget()
                     : ComputeScrollTarget();
  m_ScrollCurrent = m_ScrollTarget;
  m_ScrollTimer.stop();
  update();
}

void LyricsWindow::GetFontScale(float& p_Scale)
{
  p_Scale = m_FontScale;
}

float LyricsWindow::GetDefaultFontScale() const
{
  return m_FontScaleDefault;
}

void LyricsWindow::ZoomIn()
{
  SetFontScale(m_FontScale + s_FontScaleStep);
}

void LyricsWindow::ZoomOut()
{
  SetFontScale(m_FontScale - s_FontScaleStep);
}

void LyricsWindow::ZoomReset()
{
  // Scale the default for the current window width so that reset after a
  // pure resize (no manual zoom) is a no-op.
  float widthRatio = static_cast<float>(width()) / s_WindowWidth;
  SetFontScale(m_FontScaleDefault * widthRatio);
}

void LyricsWindow::ToggleLyrics()
{
  m_Enabled = !m_Enabled;
  if (m_Enabled)
  {
    if (m_HasLyrics)
    {
      show();
      raise();
    }
  }
  else
  {
    hide();
  }
  emit EnabledChanged(m_Enabled);
}

void LyricsWindow::ToggleFullScreen()
{
  if (!isVisible()) return;
  if (isFullScreen())
    showNormal();
  else
    showFullScreen();
}

int LyricsWindow::FindCurrentLine(qint64 p_PositionMs) const
{
  if (m_Lyrics.lines.isEmpty()) return -1;

  int result = -1;
  for (int i = 0; i < m_Lyrics.lines.size(); i++)
  {
    if (m_Lyrics.lines[i].timeMs <= p_PositionMs)
      result = i;
    else
      break;
  }
  return result;
}

void LyricsWindow::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), QColor(24, 24, 24));

  if (!m_HasLyrics || m_Lyrics.lines.isEmpty()) return;

  int fontSize = static_cast<int>(s_BaseFontSize * m_FontScale);
  int boldFontSize = s_CurrentLineLarger
                       ? static_cast<int>(s_BaseBoldFontSize * m_FontScale)
                       : fontSize;
  QFont font("Sans", fontSize);
  QFont fontBold("Sans", boldFontSize, QFont::Bold);
  QFontMetrics fm(font);
  QFontMetrics fmBold(fontBold);

  int viewHeight = height();
  int viewWidth = width() - 2 * s_Margin;

  if (m_Lyrics.synced)
  {
    // Synced lyrics: anchor current line (centered for real timestamps,
    // shifted upward for synthetic so the first line starts high).
    int anchorY = m_UsesSyntheticTimestamps
                    ? static_cast<int>(viewHeight * (1.0f - s_SyntheticAnchorFraction))
                    : (viewHeight / 2);
    float scrollY = anchorY - m_ScrollCurrent;

    for (int i = 0; i < m_Lyrics.lines.size(); i++)
    {
      bool isCurrent = (i == m_CurrentLine);
      QString text = m_Lyrics.lines[i].text;
      if (text.isEmpty())
        text = " ";

      const QFontMetrics& curFm = isCurrent ? fmBold : fm;
      int h = WrapHeight(curFm, viewWidth, text);
      int y = static_cast<int>(scrollY);

      // Skip lines outside viewport
      if (y + h < 0)
      {
        scrollY += h;
        continue;
      }
      if (y > viewHeight)
        break;

      if (m_UsesSyntheticTimestamps)
      {
        // Plain lyrics: no current-line highlight (it would be misleading
        // since timestamps are synthetic); just render uniformly.
        painter.setFont(font);
        painter.setPen(QColor(200, 200, 200));
      }
      else if (isCurrent)
      {
        painter.setFont(fontBold);
        painter.setPen(QColor(255, 255, 255));
      }
      else if (i < m_CurrentLine)
      {
        painter.setFont(font);
        painter.setPen(QColor(120, 120, 120));
      }
      else
      {
        painter.setFont(font);
        painter.setPen(QColor(180, 180, 180));
      }

      QRect textRect(s_Margin, y, viewWidth, h);
      int hAlign = s_CenterText ? Qt::AlignHCenter : Qt::AlignLeft;
      painter.drawText(textRect, Qt::TextWordWrap | hAlign | Qt::AlignVCenter, text);

      scrollY += h;
    }
  }
  else
  {
    // Unsynced lyrics: m_ScrollCurrent is the content-space Y that should
    // be pinned at the window vertical center. It glides toward m_ScrollTarget
    // via the scroll timer, giving the same smoothing as synced lyrics.
    float yStart = viewHeight / 2.0f - m_ScrollCurrent;

    painter.setFont(font);
    painter.setPen(QColor(200, 200, 200));

    float y = yStart;
    for (int i = 0; i < m_Lyrics.lines.size(); i++)
    {
      QString text = m_Lyrics.lines[i].text;
      if (text.isEmpty())
        text = " ";

      int h = WrapHeight(fm, viewWidth, text);
      if (y + h >= 0 && y <= viewHeight)
      {
        QRect textRect(s_Margin, static_cast<int>(y), viewWidth, h);
        int hAlign = s_CenterText ? Qt::AlignHCenter : Qt::AlignLeft;
        painter.drawText(textRect, Qt::TextWordWrap | hAlign | Qt::AlignVCenter, text);
      }
      y += h;
      if (y > viewHeight) break;
    }
  }
}

void LyricsWindow::closeEvent(QCloseEvent* p_Event)
{
  hide();
  p_Event->ignore();
}

void LyricsWindow::showEvent(QShowEvent* p_Event)
{
  QWidget::showEvent(p_Event);
#ifdef __APPLE__
  ShowDockIcon();
#endif
}

void LyricsWindow::hideEvent(QHideEvent* p_Event)
{
  QWidget::hideEvent(p_Event);
#ifdef __APPLE__
  HideDockIcon();
#endif
}

void LyricsWindow::resizeEvent(QResizeEvent* p_Event)
{
  QWidget::resizeEvent(p_Event);

  int oldW = p_Event->oldSize().width();
  int newW = p_Event->size().width();

  // Skip the first resize after show (oldSize is invalid) so the restored
  // geometry doesn't trigger scaling.
  if (oldW > 0 && newW > 0 && oldW != newW)
  {
    float newScale = qBound(s_FontScaleMin,
                            m_FontScale * static_cast<float>(newW) / oldW,
                            s_FontScaleMax);
    m_FontScale = newScale;
    m_ScrollTarget = (m_HasLyrics && !m_Lyrics.synced)
                       ? ComputeUnsyncedScrollTarget()
                       : ComputeScrollTarget();
    m_ScrollCurrent = m_ScrollTarget;
    m_ScrollTimer.stop();
    update();
  }
}

void LyricsWindow::keyPressEvent(QKeyEvent* p_Event)
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
