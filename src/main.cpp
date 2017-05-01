// main.cpp
//
// Copyright (C) 2017 Kristofer Berggren
// All rights reserved.
//
// namp is distributed under the GPLv2 license, see LICENSE for details.
//

#include <iostream>

#ifdef __APPLE__
#include <QApplication>
#endif
#include <QCoreApplication>
#include <QSettings>
#include <QTimer>

#include "audioplayer.h"
#include "uikeyhandler.h"
#include "uiview.h"

static void ShowHelp();
static void ShowVersion();

int main(int argc, char *argv[])
{
#ifdef __APPLE__
  // Workaround for OS X not allowing audio playback from QCoreApplication
  QApplication application(argc, argv);
#else
  QCoreApplication application(argc, argv);
#endif

  // Handle arguments
  QStringList arguments = QCoreApplication::arguments();
  arguments.removeAt(0);
  if ((arguments.size() == 0) || ((arguments.size() > 0) && ((arguments.at(0) == "-h") || (arguments.at(0) == "--help"))))
  {
    ShowHelp();
    return 1;
  }
  else if ((arguments.size() > 0) && ((arguments.at(0) == "-v") || (arguments.at(0) == "--version")))
  {
    ShowVersion();
    return 0;
  }
  
  // Init settings
  QCoreApplication::setApplicationName("namp");
  QCoreApplication::setOrganizationName("nope");
  QCoreApplication::setOrganizationDomain("nope.se");
  QSettings settings;

  // Init ui
  UIView uiView(&application);
  UIKeyhandler uiKeyhandler(&application);

  // Init player
  AudioPlayer audioPlayer(&application);

  // Signals to application
  QObject::connect(&uiKeyhandler, SIGNAL(Quit()), &application, SLOT(quit()));

  // Signals to audio player
  QObject::connect(&uiKeyhandler, SIGNAL(Previous()), &audioPlayer, SIGNAL(Previous()));
  QObject::connect(&uiKeyhandler, SIGNAL(Play()), &audioPlayer, SIGNAL(Play()));
  QObject::connect(&uiKeyhandler, SIGNAL(Pause()), &audioPlayer, SIGNAL(Pause()));
  QObject::connect(&uiKeyhandler, SIGNAL(Stop()), &audioPlayer, SIGNAL(Stop()));
  QObject::connect(&uiKeyhandler, SIGNAL(Next()), &audioPlayer, SIGNAL(Next()));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleShuffle()), &audioPlayer, SLOT(ToggleShuffle()));
  QObject::connect(&uiKeyhandler, SIGNAL(VolumeUp()), &audioPlayer, SLOT(VolumeUp()));
  QObject::connect(&uiKeyhandler, SIGNAL(VolumeDown()), &audioPlayer, SLOT(VolumeDown()));
  QObject::connect(&uiKeyhandler, SIGNAL(SkipBackward()), &audioPlayer, SLOT(SkipBackward()));
  QObject::connect(&uiKeyhandler, SIGNAL(SkipForward()), &audioPlayer, SLOT(SkipForward()));
  QObject::connect(&uiKeyhandler, SIGNAL(SetVolume(int)), &audioPlayer, SLOT(SetVolume(int)));
  QObject::connect(&uiKeyhandler, SIGNAL(SetPosition(int)), &audioPlayer, SLOT(SetPosition(int)));
  QObject::connect(&uiView, SIGNAL(SetCurrentIndex(int)), &audioPlayer, SIGNAL(SetCurrentIndex(int)));

  // Signals to ui view
  QObject::connect(&audioPlayer, SIGNAL(PlaylistUpdated(const QVector<QString>&)), &uiView, SLOT(PlaylistUpdated(const QVector<QString>&)));
  QObject::connect(&audioPlayer, SIGNAL(PositionChanged(qint64)), &uiView, SLOT(PositionChanged(qint64)));
  QObject::connect(&audioPlayer, SIGNAL(DurationChanged(qint64)), &uiView, SLOT(DurationChanged(qint64)));
  QObject::connect(&audioPlayer, SIGNAL(CurrentIndexChanged(int)), &uiView, SLOT(CurrentIndexChanged(int)));
  QObject::connect(&audioPlayer, SIGNAL(VolumeChanged(int)), &uiView, SLOT(VolumeChanged(int)));
  QObject::connect(&audioPlayer, SIGNAL(PlaybackModeUpdated(bool)), &uiView, SLOT(PlaybackModeUpdated(bool)));  
  QObject::connect(&uiKeyhandler, SIGNAL(Search()), &uiView, SLOT(Search()));
  QObject::connect(&uiKeyhandler, SIGNAL(SelectPrevious()), &uiView, SLOT(SelectPrevious()));
  QObject::connect(&uiKeyhandler, SIGNAL(SelectNext()), &uiView, SLOT(SelectNext()));
  QObject::connect(&uiKeyhandler, SIGNAL(PagePrevious()), &uiView, SLOT(PagePrevious()));
  QObject::connect(&uiKeyhandler, SIGNAL(PageNext()), &uiView, SLOT(PageNext()));
  QObject::connect(&uiKeyhandler, SIGNAL(PlaySelected()), &uiView, SLOT(PlaySelected()));
  QObject::connect(&uiKeyhandler, SIGNAL(ToggleWindow()), &uiView, SLOT(ToggleWindow()));
  QObject::connect(&uiKeyhandler, SIGNAL(KeyPress(int)), &uiView, SLOT(KeyPress(int)));
  QObject::connect(&uiKeyhandler, SIGNAL(MouseEventRequest(int, int, uint32_t)), &uiView, SLOT(MouseEventRequest(int, int, uint32_t)));

  // Signals to ui key handler
  QObject::connect(&uiView, SIGNAL(UIStateUpdated(UIState)), &uiKeyhandler, SLOT(UIStateUpdated(UIState)));
  QObject::connect(&uiView, SIGNAL(ProcessMouseEvent(const UIMouseEvent&)), &uiKeyhandler, SLOT(ProcessMouseEvent(const UIMouseEvent&)));

  // Set playlist
  audioPlayer.SetPlaylist(arguments);

  // Apply settings
  bool shuffle = settings.value("player/shuffle", true).toBool();
  emit audioPlayer.SetPlaybackMode(shuffle);
  int volume = settings.value("player/volume", 100).toInt();
  emit audioPlayer.SetVolume(volume);

  // Start playback and main event loop
  emit audioPlayer.Play();
  int rv = application.exec();

  // Save settings
  audioPlayer.GetPlaybackMode(shuffle);
  settings.setValue("player/shuffle", shuffle);
  audioPlayer.GetVolume(volume);
  settings.setValue("player/volume", volume);

  // Exit
  return rv;
}

static void ShowHelp()
{
  std::cout <<
    "namp is a minimalistic console-based audio player.\n"
    "\n"
    "Usage: namp OPTION\n"
    "   or: namp PATH...\n"
    "\n"
    "Command-line Options:\n"
    "   -h, --help        display this help and exit\n"
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
    "   pgup              playlist previous page\n"
    "   pgdn              playlist next page\n"
    "   ENTER             play selected track\n"
    "   TAB               toggle main window / playlist focus\n"
    "   s                 toggle shuffle on/off\n"
    "\n"
    "Report bugs at https://github.com/d99kris/namp/\n"
    "\n"
    ;
}

static void ShowVersion()
{
  std::cout <<
    "namp v2\n"
    "\n"
    "Copyright (C) 2015-2017 Kristofer Berggren\n"
    "This is free software; see the source for copying\n"
    "conditions. There is NO warranty; not even for\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
    "\n"
    "Written by Kristofer Berggren.\n"
    ;
}

