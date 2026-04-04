// spectrum.cpp
//
// Copyright (C) 2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <QtGlobal>

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QDateTime>
#include <QUrl>

#include "log.h"
#include "spectrum.h"

// Timer interval in ms - controls display update rate (~16 fps at 60ms)
static const int kTimerIntervalMs = 60;

// Spectrum snapshots per second of audio - controls how many FFTs are
// computed per second of decoded audio. Lower values reduce CPU load.
static const int kSpectrumFps = 16;

Spectrum::Spectrum(std::function<qint64()> p_PositionGetter, QObject* p_Parent)
  : QObject(p_Parent)
  , m_PositionGetter(p_PositionGetter)
  , m_Window(kFFTSize)
  , m_FFTBuffer(kFFTSize)
{
  // Precompute Hann window
  for (int i = 0; i < kFFTSize; ++i)
  {
    m_Window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (kFFTSize - 1)));
  }

  connect(&m_Timer, &QTimer::timeout, this, &Spectrum::OnTimer);
  m_Timer.setInterval(kTimerIntervalMs);

  connect(&m_Decoder, &QAudioDecoder::finished, this, &Spectrum::OnDecoderFinished);
}

void Spectrum::StartTrack(const QString& p_Track)
{
  m_Decoder.stop();
  m_SpectrumData.clear();
  m_SpectrumIndex = 0;
  m_CurrentSpectrum.fill(0.0f);

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
  m_Decoder.setSource(QUrl::fromLocalFile(p_Track));
#else
  m_Decoder.setSourceFilename(p_Track);
#endif
  m_Decoder.start();

  if (!m_Timer.isActive())
  {
    m_Timer.start();
  }
}

void Spectrum::Stop()
{
  m_Timer.stop();
  m_Decoder.stop();
  m_SpectrumData.clear();
  m_CurrentSpectrum.fill(0.0f);
}

bool Spectrum::IsRunning() const
{
  return m_Timer.isActive();
}

void Spectrum::OnTimer()
{
  static int timerCount = 0;
  static qint64 lastLogTime = 0;
  ++timerCount;
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (lastLogTime == 0) lastLogTime = now;
  if (now - lastLogTime >= 2000)
  {
    Log::Info("SpectrumTimer: %d calls in %lld ms (%.1f fps), spectrumData=%d entries",
              timerCount, now - lastLogTime, timerCount * 1000.0 / (now - lastLogTime),
              m_SpectrumData.size());
    timerCount = 0;
    lastLogTime = now;
  }

  // Process pending decoder buffers (one per tick to avoid blocking)
  if (m_Decoder.bufferAvailable())
  {
    ProcessDecoderBuffer();
  }

  if (m_SpectrumData.isEmpty()) return;

  // Compensate for audio output buffer latency
  static const qint64 kLatencyOffsetMs = 175;
  qint64 pos = m_PositionGetter() + kLatencyOffsetMs;

  // Advance index to match current position
  while (m_SpectrumIndex < m_SpectrumData.size() - 1 &&
         m_SpectrumData[m_SpectrumIndex + 1].first <= pos)
  {
    ++m_SpectrumIndex;
  }

  // Handle seek backward
  if (m_SpectrumIndex > 0 && m_SpectrumData[m_SpectrumIndex].first > pos)
  {
    m_SpectrumIndex = 0;
    while (m_SpectrumIndex < m_SpectrumData.size() - 1 &&
           m_SpectrumData[m_SpectrumIndex + 1].first <= pos)
    {
      ++m_SpectrumIndex;
    }
  }

  const QVector<float>& bands = m_SpectrumData[m_SpectrumIndex].second;
  const float attack = 0.89f;  // Rise speed: 0=sluggish, 1=instant
  const float decay = 0.94f;   // Fall-off: 1=hold forever, 0=instant drop
  for (int i = 0; i < 8 && i < bands.size(); ++i)
  {
    if (bands[i] > m_CurrentSpectrum[i])
    {
      // Rising: smooth toward new peak
      m_CurrentSpectrum[i] += attack * (bands[i] - m_CurrentSpectrum[i]);
    }
    else
    {
      // Falling: gradual decay
      m_CurrentSpectrum[i] *= decay;
    }
  }

  emit SpectrumChanged(m_CurrentSpectrum);
}

void Spectrum::ProcessDecoderBuffer()
{
  QAudioBuffer buffer = m_Decoder.read();
  if (!buffer.isValid() || buffer.frameCount() < kFFTSize) return;

  const QAudioFormat fmt = buffer.format();
  const int channels = fmt.channelCount();
  const int sampleRate = fmt.sampleRate();
  const int frames = buffer.frameCount();
  const qint64 startUs = buffer.startTime();
  const float usPerFrame = 1000000.0f / sampleRate;

  const int kStepSize = sampleRate / kSpectrumFps;
  for (int offset = 0; offset + kFFTSize <= frames; offset += kStepSize)
  {
    qint64 timeMs = (startUs + static_cast<qint64>(offset * usPerFrame)) / 1000;

    QVector<float> spectrum(8);

    if (fmt.sampleFormat() == QAudioFormat::Int16)
    {
      const qint16* data = buffer.constData<qint16>() + offset * channels;
      for (int i = 0; i < kFFTSize; ++i)
      {
        float sample = 0;
        for (int ch = 0; ch < channels; ++ch)
        {
          sample += data[i * channels + ch] / 32768.0f;
        }
        m_FFTBuffer[i] = std::complex<float>(sample / channels * m_Window[i], 0.0f);
      }
    }
    else if (fmt.sampleFormat() == QAudioFormat::Float)
    {
      const float* data = buffer.constData<float>() + offset * channels;
      for (int i = 0; i < kFFTSize; ++i)
      {
        float sample = 0;
        for (int ch = 0; ch < channels; ++ch)
        {
          sample += data[i * channels + ch];
        }
        m_FFTBuffer[i] = std::complex<float>(sample / channels * m_Window[i], 0.0f);
      }
    }
    else
    {
      return;
    }

    FFT(m_FFTBuffer);

    static const int kBandCount = 8;
    static const float bandEdges[kBandCount + 1] = {
      20, 150, 400, 800, 1500, 3000, 6000, 12000, 20000
    };
    const float binHz = static_cast<float>(sampleRate) / kFFTSize;
    const int maxBin = kFFTSize / 2;

    static const float kTiltDbPerOctave = 4.0f;
    static const float kRefFreqHz = 1000.0f;
    static const float kNoiseFloorDb = -60.0f;
    static const float kDynamicRangeDb = 75.0f;

    // Per-band display correction (dB) — attenuate energy-dense mid-range
    //   sub-bass  bass  lo-mid  mid  up-mid  presence  brilliance  air
    static const float bandCorrectionDb[kBandCount] = {
      0.0f, 0.0f, -3.0f, -5.0f, -4.0f, -2.0f, 0.0f, 0.0f
    };

    for (int b = 0; b < kBandCount; ++b)
    {
      int binLo = qBound(0, static_cast<int>(bandEdges[b] / binHz), maxBin - 1);
      int binHi = qBound(binLo + 1, static_cast<int>(bandEdges[b + 1] / binHz), maxBin);

      float magnitude = 0;
      for (int i = binLo; i < binHi; ++i)
      {
        magnitude += std::abs(m_FFTBuffer[i]);
      }
      magnitude /= (binHi - binLo);

      float centerFreq = sqrtf(bandEdges[b] * bandEdges[b + 1]);
      float db = 20.0f * log10f(qMax(magnitude, 1e-6f));
      float tiltDb = kTiltDbPerOctave * log2f(centerFreq / kRefFreqHz);
      spectrum[b] = qBound(0.0f, (db + tiltDb + bandCorrectionDb[b] - kNoiseFloorDb) / kDynamicRangeDb, 1.0f);
    }

    m_SpectrumData.append(qMakePair(timeMs, spectrum));
  }
}

void Spectrum::OnDecoderFinished()
{
  Log::Debug("Decoder finished, %d spectrum entries", m_SpectrumData.size());
}

void Spectrum::FFT(std::vector<std::complex<float>>& p_Data)
{
  const size_t n = p_Data.size();
  if (n <= 1) return;

  // Bit-reversal permutation
  for (size_t i = 1, j = 0; i < n; ++i)
  {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
    {
      j ^= bit;
    }
    j ^= bit;
    if (i < j)
    {
      std::swap(p_Data[i], p_Data[j]);
    }
  }

  // Butterfly stages
  for (size_t len = 2; len <= n; len <<= 1)
  {
    float angle = -2.0f * M_PI / len;
    std::complex<float> wlen(cosf(angle), sinf(angle));
    for (size_t i = 0; i < n; i += len)
    {
      std::complex<float> w(1.0f, 0.0f);
      for (size_t j = 0; j < len / 2; ++j)
      {
        std::complex<float> u = p_Data[i + j];
        std::complex<float> v = p_Data[i + j + len / 2] * w;
        p_Data[i + j] = u + v;
        p_Data[i + j + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
}
