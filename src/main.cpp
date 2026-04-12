// main.cpp
//
// Copyright (C) 2017-2026 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <iostream>

#include <fcntl.h>
#include <unistd.h>

#ifdef HAS_GUI
#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPixmap>
#endif
#include <QAudioDecoder>
#include <QCoreApplication>
#include <QSettings>
#include <QSocketNotifier>
#include <QTimer>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QMediaFormat>
#endif

#include "audioplayer.h"
#ifdef HAS_GUI
#include "cdgwindow.h"
#include "lyricsprovider.h"
#include "lyricswindow.h"
#ifdef __APPLE__
#include "macdock.h"
#endif
#endif
#include "log.h"
#include "uikeyhandler.h"
#include "uiview.h"
#include "version.h"

static void ShowHelp();
static void ShowVersion();
static void InitStdErrRedirect(const std::string& p_Path);
static void CleanupStdErrRedirect();

static int s_OrgStdErr = -1;
static int s_NewStdErr = -1;

int main(int argc, char *argv[])
{
  Log::SetPath("/tmp/namp.log");
  Log::SetDebugEnabled(false);

#ifdef HAS_GUI
  QApplication application(argc, argv);
  {
    QFile iconFile(":/namp.png");
    if (iconFile.open(QIODevice::ReadOnly))
    {
      QByteArray iconData = iconFile.readAll();
      iconFile.close();
      QPixmap iconPixmap;
      iconPixmap.loadFromData(iconData);
      QApplication::setWindowIcon(QIcon(iconPixmap));
#ifdef __APPLE__
      SetDockIcon(iconData.constData(), static_cast<size_t>(iconData.size()));
#endif
    }
  }
  qRegisterMetaType<LyricsData>("LyricsData");
#else
  QCoreApplication application(argc, argv);
#endif

  Log::Info("namp v" VERSION);

  // Handle arguments
  QStringList arguments = QCoreApplication::arguments();
  arguments.removeAt(0);
  bool setup = false;
  if (arguments.size() == 0)
  {
    ShowHelp();
    return 1;
  }
  else if ((arguments.size() > 0) && ((arguments.at(0) == "-h") || (arguments.at(0) == "--help")))
  {
    ShowHelp();
    return 0;
  }
  else if ((arguments.size() > 0) && ((arguments.at(0) == "-v") || (arguments.at(0) == "--version")))
  {
    ShowVersion();
    return 0;
  }
  else if ((arguments.size() > 0) && ((arguments.at(0) == "-s") || (arguments.at(0) == "--setup")))
  {
    setup = true;
  }

  // Init environment
  InitStdErrRedirect("/dev/null");
  srand(time(0));

  // Init settings
  QCoreApplication::setApplicationName("namp");
  QCoreApplication::setOrganizationName("nope");
  QCoreApplication::setOrganizationDomain("nope.se");
  QSettings settings;

  Scrobbler* scrobbler = NULL;

  if (setup)
  {
    std::cout << "last.fm scrobbler account setup\n";
    std::cout << "user: ";
    std::string user;
    std::getline(std::cin, user);
    std::cout << "pass: ";
    std::string pass;
    pass = Scrobbler::GetPass();
    if (!pass.empty())
    {
      pass = Scrobbler::MD5(pass);
    }

    scrobbler = new Scrobbler(&application, user, pass);

    settings.setValue("scrobbler/user", QString::fromStdString(user));
    settings.setValue("scrobbler/pass", QString::fromStdString(pass));

    return 0;
  }

  // Check dependencies
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const bool isMp3DecodeSupported = QMediaFormat().supportedAudioCodecs(QMediaFormat::Decode).contains(QMediaFormat::AudioCodec::MP3);
#else
  const bool isMp3DecodeSupported = QAudioDecoder::hasSupport("audio/mpeg");
#endif
  if (!isMp3DecodeSupported)
  {
    std::cout << "warning: mp3 decode support not detected, please see:\n";
    std::cout << "https://github.com/d99kris/namp/blob/master/doc/CODECS.md\n";
  }

  // Init player
  AudioPlayer audioPlayer(&application);
  if (!audioPlayer.IsInited())
  {
    std::cout << "failed to init audio output\n";
    return 1;
  }

  // Init scrobbler
  std::string user = settings.value("scrobbler/user", "").toString().toStdString();
  std::string pass = settings.value("scrobbler/pass", "").toString().toStdString();
  if (!user.empty() && !pass.empty())
  {
    scrobbler = new Scrobbler(&application, user, pass);
  }
  else
  {
    Log::Debug("Scrobbler disabled");
  }

  // Init ui
  UIView uiView(&application, scrobbler);
  UIKeyhandler uiKeyhandler(&application);

  // Signals to application
  QObject::connect(&uiKeyhandler, SIGNAL(Quit()), &application, SLOT(quit()));

  // Signals to audio player
  QObject::connect(&uiKeyhandler, SIGNAL(Previous()), &audioPlayer, SLOT(Previous()));
  QObject::connect(&uiKeyhandler, SIGNAL(Play()), &audioPlayer, SLOT(Play()));
  QObject::connect(&uiKeyhandler, SIGNAL(Pause()), &audioPlayer, SLOT(Pause()));
  QObject::connect(&uiKeyhandler, SIGNAL(Stop()), &audioPlayer, SLOT(Stop()));
  QObject::connect(&uiKeyhandler, SIGNAL(Next()), &audioPlayer, SLOT(Next()));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleShuffle()), &audioPlayer, SLOT(ToggleShuffle()));
  QObject::connect(&uiKeyhandler, SIGNAL(VolumeUp()), &audioPlayer, SLOT(VolumeUp()));
  QObject::connect(&uiKeyhandler, SIGNAL(VolumeDown()), &audioPlayer, SLOT(VolumeDown()));
  QObject::connect(&uiKeyhandler, SIGNAL(SkipBackward()), &audioPlayer, SLOT(SkipBackward()));
  QObject::connect(&uiKeyhandler, SIGNAL(SkipForward()), &audioPlayer, SLOT(SkipForward()));
  QObject::connect(&uiKeyhandler, SIGNAL(SetVolume(int)), &audioPlayer, SLOT(SetVolume(int)));
  QObject::connect(&uiKeyhandler, SIGNAL(SetPosition(int)), &audioPlayer, SLOT(SetPosition(int)));
  QObject::connect(&uiKeyhandler, SIGNAL(ExternalEdit()), &uiView, SLOT(ExternalEdit()));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleAnalyzer()), &uiView, SLOT(ToggleAnalyzer()));
  QObject::connect(&uiView, SIGNAL(ExternalEdit(int)), &audioPlayer, SLOT(ExternalEdit(int)));
  QObject::connect(&uiView, SIGNAL(SetCurrentIndex(int)), &audioPlayer, SLOT(SetCurrentIndex(int)));
  QObject::connect(&uiView, SIGNAL(Play()), &audioPlayer, SLOT(Play()));
  QObject::connect(&uiView, SIGNAL(AnalyzerEnabled(bool)), &audioPlayer, SLOT(SetAnalyzerEnabled(bool)));

  // Signals to ui view
  QObject::connect(&audioPlayer, SIGNAL(PlaylistUpdated(const QVector<QString>&)), &uiView, SLOT(PlaylistUpdated(const QVector<QString>&)));
  QObject::connect(&audioPlayer, SIGNAL(PositionChanged(qint64)), &uiView, SLOT(PositionChanged(qint64)));
  QObject::connect(&audioPlayer, SIGNAL(DurationChanged(qint64)), &uiView, SLOT(DurationChanged(qint64)));
  QObject::connect(&audioPlayer, SIGNAL(CurrentIndexChanged(int)), &uiView, SLOT(CurrentIndexChanged(int)));
  QObject::connect(&audioPlayer, SIGNAL(VolumeChanged(int)), &uiView, SLOT(VolumeChanged(int)));
  QObject::connect(&audioPlayer, SIGNAL(PlaybackModeUpdated(bool)), &uiView, SLOT(PlaybackModeUpdated(bool)));
  QObject::connect(&audioPlayer, SIGNAL(RefreshTrackData(int)), &uiView, SLOT(RefreshTrackData(int)));
  QObject::connect(&audioPlayer, SIGNAL(SpectrumChanged(const QVector<float>&)), &uiView, SLOT(SpectrumChanged(const QVector<float>&)));
  QObject::connect(&uiKeyhandler, SIGNAL(Search()), &uiView, SLOT(Search()));
  QObject::connect(&uiKeyhandler, SIGNAL(SelectPrevious()), &uiView, SLOT(SelectPrevious()));
  QObject::connect(&uiKeyhandler, SIGNAL(SelectNext()), &uiView, SLOT(SelectNext()));
  QObject::connect(&uiKeyhandler, SIGNAL(PagePrevious()), &uiView, SLOT(PagePrevious()));
  QObject::connect(&uiKeyhandler, SIGNAL(PageNext()), &uiView, SLOT(PageNext()));
  QObject::connect(&uiKeyhandler, SIGNAL(Home()), &uiView, SLOT(Home()));
  QObject::connect(&uiKeyhandler, SIGNAL(End()), &uiView, SLOT(End()));
  QObject::connect(&uiKeyhandler, SIGNAL(PlaySelected()), &uiView, SLOT(PlaySelected()));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleWindow()), &uiView, SLOT(ToggleWindow()));
  QObject::connect(&uiKeyhandler, SIGNAL(KeyPress(int)), &uiView, SLOT(KeyPress(int)));
  QObject::connect(&uiKeyhandler, SIGNAL(MouseEventRequest(int, int, uint32_t)), &uiView, SLOT(MouseEventRequest(int, int, uint32_t)));

  // Signals to ui key handler
  QSocketNotifier socketNotifier(fileno(stdin), QSocketNotifier::Read, &application);
  QObject::connect(&socketNotifier, SIGNAL(activated(int)), &uiKeyhandler, SLOT(ProcessKeyEvent()));
  QObject::connect(&uiView, SIGNAL(UIStateUpdated(UIState)), &uiKeyhandler, SLOT(UIStateUpdated(UIState)));
  QObject::connect(&uiView, SIGNAL(ProcessMouseEvent(const UIMouseEvent&)), &uiKeyhandler, SLOT(ProcessMouseEvent(const UIMouseEvent&)));

#ifdef HAS_GUI
  // Init CDG window
  CdgWindow cdgWindow;
  QObject::connect(&audioPlayer, SIGNAL(TrackChanged(const QString&)), &cdgWindow, SLOT(TrackChanged(const QString&)));
  QObject::connect(&audioPlayer, SIGNAL(PositionChanged(qint64)), &cdgWindow, SLOT(PositionChanged(qint64)));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleCdg()), &cdgWindow, SLOT(ToggleCdg()));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleFullScreen()), &cdgWindow, SLOT(ToggleFullScreen()));
  QObject::connect(&cdgWindow, SIGNAL(KeyReceived()), &uiKeyhandler, SLOT(ProcessKeyEvent()));

  // Init lyrics
  LyricsProvider lyricsProvider(&application);
  LyricsWindow lyricsWindow;
  QObject::connect(&audioPlayer, SIGNAL(TrackChanged(const QString&)), &lyricsProvider, SLOT(TrackChanged(const QString&)));
  QObject::connect(&audioPlayer, SIGNAL(RefreshLyrics(const QString&)), &lyricsProvider, SLOT(TrackChanged(const QString&)));
  QObject::connect(&lyricsProvider, SIGNAL(LyricsReady(const LyricsData&)), &lyricsWindow, SLOT(SetLyrics(const LyricsData&)));
  QObject::connect(&lyricsProvider, SIGNAL(LyricsCleared()), &lyricsWindow, SLOT(ClearLyrics()));
  QObject::connect(&lyricsProvider, SIGNAL(LyricsLoading()), &lyricsWindow, SLOT(LyricsLoading()));
  QObject::connect(&lyricsWindow, SIGNAL(EnabledChanged(bool)), &lyricsProvider, SLOT(SetEnabled(bool)));
  QObject::connect(&lyricsWindow, SIGNAL(EnabledChanged(bool)), &uiView, SLOT(LyricsUpdated(bool)));
  uiView.SetLyricsAvailable(true);
  QObject::connect(&audioPlayer, SIGNAL(PositionChanged(qint64)), &lyricsWindow, SLOT(PositionChanged(qint64)));
  QObject::connect(&audioPlayer, SIGNAL(DurationChanged(qint64)), &lyricsWindow, SLOT(DurationChanged(qint64)));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleLyrics()), &lyricsWindow, SLOT(ToggleLyrics()));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleFullScreen()), &lyricsWindow, SLOT(ToggleFullScreen()));
  QObject::connect(&uiKeyhandler, SIGNAL(LyricsZoomIn()), &lyricsWindow, SLOT(ZoomIn()));
  QObject::connect(&uiKeyhandler, SIGNAL(LyricsZoomOut()), &lyricsWindow, SLOT(ZoomOut()));
  QObject::connect(&uiKeyhandler, SIGNAL(LyricsZoomReset()), &lyricsWindow, SLOT(ZoomReset()));
  QObject::connect(&lyricsWindow, SIGNAL(KeyReceived()), &uiKeyhandler, SLOT(ProcessKeyEvent()));
#endif

  // Apply settings
  bool shuffle = settings.value("player/shuffle", false).toBool();
  emit audioPlayer.SetPlaybackMode(shuffle);
  int volume = settings.value("player/volume", 100).toInt();
  emit audioPlayer.SetVolume(volume);
  QString currentTrack = settings.value("player/track", "").toString();
  bool scrollTitle = settings.value("ui/scrolltitle", false).toBool();
  uiView.SetScrollTitle(scrollTitle);
  bool viewPosition = settings.value("ui/viewposition", true).toBool();
  uiView.SetViewPosition(viewPosition);
  bool viewAnalyzer = settings.value("ui/viewanalyzer", false).toBool();
  uiView.SetViewAnalyzer(viewAnalyzer);
#ifdef HAS_GUI
  bool viewCdg = settings.value("ui/viewcdg", true).toBool();
  cdgWindow.SetEnabled(viewCdg);
  bool viewLyrics = settings.value("ui/viewlyrics", false).toBool();
  lyricsWindow.SetEnabled(viewLyrics);
  lyricsProvider.SetEnabled(viewLyrics);
  float lyricsZoom = settings.value("ui/lyricszoom", static_cast<double>(lyricsWindow.GetDefaultFontScale())).toFloat();
  lyricsWindow.SetFontScale(lyricsZoom);
  QByteArray cdgGeometry = settings.value("ui/cdggeometry").toByteArray();
  if (!cdgGeometry.isEmpty())
    cdgWindow.restoreGeometry(cdgGeometry);
  QByteArray lyricsGeometry = settings.value("ui/lyricsgeometry").toByteArray();
  if (!lyricsGeometry.isEmpty())
    lyricsWindow.restoreGeometry(lyricsGeometry);
#endif

  // Set playlist and track
  audioPlayer.SetPlaylist(arguments, currentTrack);

  // Start playback and main event loop
  audioPlayer.Play();
  int rv = application.exec();

  // Stop audio backend cleanly before settings save / teardown
  audioPlayer.Shutdown();
  QCoreApplication::processEvents();

  // Save settings
  audioPlayer.GetPlaybackMode(shuffle);
  settings.setValue("player/shuffle", shuffle);
  audioPlayer.GetVolume(volume);
  settings.setValue("player/volume", volume);
  audioPlayer.GetCurrentTrack(currentTrack);
  settings.setValue("player/track", currentTrack);
  uiView.GetScrollTitle(scrollTitle);
  settings.setValue("ui/scrolltitle", scrollTitle);
  uiView.GetViewPosition(viewPosition);
  settings.setValue("ui/viewposition", viewPosition);
  uiView.GetViewAnalyzer(viewAnalyzer);
  settings.setValue("ui/viewanalyzer", viewAnalyzer);
#ifdef HAS_GUI
  cdgWindow.GetEnabled(viewCdg);
  settings.setValue("ui/viewcdg", viewCdg);
  lyricsWindow.GetEnabled(viewLyrics);
  settings.setValue("ui/viewlyrics", viewLyrics);
  lyricsWindow.GetFontScale(lyricsZoom);
  settings.setValue("ui/lyricszoom", lyricsZoom);
  settings.setValue("ui/cdggeometry", cdgWindow.saveGeometry());
  settings.setValue("ui/lyricsgeometry", lyricsWindow.saveGeometry());
#endif

  // Cleanup
  if (scrobbler != NULL)
  {
    delete scrobbler;
    scrobbler = NULL;
  }

  // Cleanup environment
  CleanupStdErrRedirect();

  // Exit
  return rv;
}

static void ShowHelp()
{
  std::cout <<
    "namp is a minimalistic terminal-based audio player.\n"
    "\n"
    "Usage: namp OPTION\n"
    "   or: namp PATH...\n"
    "\n"
    "Command-line Options:\n"
    "   -h, --help        display this help and exit\n"
    "   -s, --setup       setup last.fm scrobbling account\n"
    "   -v, --version     output version information and exit\n"
    "   PATH              file or directory to add to playlist\n"
    "\n"
    "Command-line Examples:\n"
    "   namp ~/Music      play all files in ~/Music\n"
    "   namp hello.mp3    play hello.mp3\n"
    "\n"
    "Interactive Commands:\n"
    "   z                 previous track\n"
    "   x                 play\n"
    "   c                 pause\n"
    "   v                 stop\n"
    "   b                 next track\n"
    "   q,ESC             quit\n"
    "   /,j               find\n"
    "   up,+              volume up\n"
    "   down,-            volume down\n"
    "   left              skip/fast backward\n"
    "   right             skip/fast forward\n"
    "   home              playlist top\n"
    "   end               playlist end\n"
    "   pgup              playlist previous page\n"
    "   pgdn              playlist next page\n"
    "   ENTER             play selected track\n"
    "   TAB               toggle main window / playlist focus\n"
    "   e                 external tag editor\n"
#ifdef HAS_GUI
    "   g                 toggle CDG graphics window\n"
    "   l                 toggle lyrics window\n"
    "   f                 toggle fullscreen (lyrics/cdg)\n"
    "   ,                 lyrics font smaller\n"
    "   .                 lyrics font larger\n"
    "   ;                 lyrics font reset\n"
#endif
    "   s                 toggle shuffle on/off\n"
    "\n"
    "Config Path Linux:\n"
    "   ~/.config/nope/namp.conf\n"
    "\n"
    "Config Path macOS:\n"
    "   ~/Library/Preferences/se.nope.namp.plist\n"
    "\n"
    "Report bugs at https://github.com/d99kris/namp/\n"
    "\n"
    ;
}

static void ShowVersion()
{
  std::cout <<
    "namp v" VERSION "\n"
    "\n"
    "Copyright (C) 2015-2025 Kristofer Berggren\n"
    "This is free software; see the source for copying\n"
    "conditions. There is NO warranty; not even for\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
    "\n"
    "Written by Kristofer Berggren.\n"
    ;
}

void InitStdErrRedirect(const std::string& p_Path)
{
  s_NewStdErr = open(p_Path.c_str(), O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
  if (s_NewStdErr != -1)
  {
    s_OrgStdErr = dup(fileno(stderr));
    dup2(s_NewStdErr, fileno(stderr));
  }
}

void CleanupStdErrRedirect()
{
  if (s_NewStdErr != -1)
  {
    fflush(stderr);
    close(s_NewStdErr);
    dup2(s_OrgStdErr, fileno(stderr));
    close(s_OrgStdErr);
  }
}
