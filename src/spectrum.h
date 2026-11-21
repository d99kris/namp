// spectrum.h
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#pragma once

#include <QAudioDecoder>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <complex>
#include <functional>
#include <vector>

class Spectrum : public QObject
{
  Q_OBJECT

public:
  Spectrum(std::function<qint64()> p_PositionGetter, QObject* p_Parent = nullptr);

  void StartTrack(const QString& p_Track);
  void Stop();
  bool IsRunning() const;
  void SetPaused(bool p_Paused);
  void StartDecay();

signals:
  void SpectrumChanged(const QVector<float>& p_Spectrum);

private slots:
  void OnTimer();
  void OnDecoderFinished();
  void OnDecoderSettled();
  void ApplyPendingOp();
  void OnDecoderLoadingWatchdog();

private:
  void StartDecoder(const QString& p_Track);
  void StopDecoder();
  void ProcessDecoderBuffer();
  static void FFT(std::vector<std::complex<float>>& p_Data);

  static const int kFFTSize = 512;

private:
  bool m_Paused = false;
  bool m_Decaying = false;
  std::function<qint64()> m_PositionGetter;
  QAudioDecoder m_Decoder;

  // The macOS AVFoundation decoder backend loads tracks asynchronously and
  // hands a non-retained AVAssetTrack* to a queued call. Calling stop() or
  // setSource() before that call runs releases the asset (and the track),
  // causing a use-after-free crash. While loading, decoder operations are
  // therefore deferred until the decoder has settled.
  enum class PendingOp { None, Start, Stop };
  bool m_DecoderLoading = false;
  bool m_ApplyPendingQueued = false;
  PendingOp m_PendingOp = PendingOp::None;
  QString m_PendingTrack;
  QTimer m_DecoderLoadingWatchdog;
  QTimer m_Timer;
  QVector<QPair<qint64, QVector<float>>> m_SpectrumData;
  QVector<float> m_CurrentSpectrum = QVector<float>(8, 0.0f);
  int m_SpectrumIndex = 0;
  std::vector<float> m_Window;
  std::vector<std::complex<float>> m_FFTBuffer;
};
