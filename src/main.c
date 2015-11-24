/*
 * main.c
 *
 * namp - ncurses audio media player
 * Copyright (C) 2015  Kristofer Berggren
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
 * MA 02110-1301, USA.
 *
 */

/* ----------- Includes ------------------------------------------ */
#include <alsa/asoundlib.h>
#include <ao/ao.h>
#include <curl/curl.h>

#include <glib.h>
#include <locale.h>
#include <mpg123.h>
#include <stdio.h>
#include <string.h>

#include "conf.h"
#include "list.h"
#include "main.h"
#include "model.h"
#include "play.h"
#include "scrobbler.h"
#include "uictrl.h"
#include "uiview.h"


/* ----------- Defines ------------------------------------------- */


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */


/* ----------- Local Function Prototypes ------------------------- */
static void main_disp_help(void);
static void main_disp_version(void);


/* ----------- File Global Variables ----------------------------- */


/* ----------- Global Functions ---------------------------------- */
int main(int argc, char *argv[])
{
  int rv = 0;

  /* Handle special arguments */
  if(argc > 1)
  {
    if((strcmp(argv[1], "--help") == 0) ||
       (strcmp(argv[1], "-h") == 0))
    {
      main_disp_help();
      return 0;
    }
    else if((strcmp(argv[1], "--version") == 0) ||
            (strcmp(argv[1], "-v") == 0))
    {
      main_disp_version();
      return 0;
    }
  }
  else if(argc == 1)
  {
    main_disp_help();
    return 1;
  }

  /* General program init */
  setlocale(LC_CTYPE, "");
  srand(time(0));
  ao_initialize();
  mpg123_init();
  curl_global_init(CURL_GLOBAL_DEFAULT);

  /* Initializes components */
  uiview_init();
  uictrl_init();
  list_init();
  play_init();
  conf_init();
  scrobbler_init();

  /* Update playlist with command line input */
  if(argc > 1)
  {
    int i = 0;
    list_clear();
    for(i = 1; i < argc; i++)
    {
      list_add(argv[i]);
    }
  }

  /* Init model */
  model_init();

  /* Start main loop */
  model_mainloop();

  /* Cleanup model */
  model_cleanup();

  /* Cleanup components */
  scrobbler_cleanup();
  conf_cleanup();
  play_cleanup();
  list_cleanup();
  uictrl_cleanup();
  uiview_cleanup();

  /* General program cleanup */
  ao_shutdown();
  mpg123_exit();
  curl_global_cleanup();
  snd_config_update_free_global();

  return rv;
}


/* ----------- Local Functions ----------------------------------- */
static void main_disp_help(void)
{
  printf("namp is a console MP3 player for Linux.\n"
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
         "   namp ./music      play all files in ./music\n"
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
         "   INSERT            enqueue selected track\n"
         "   TAB               toggle main window / playlist focus\n"
         "   s                 toggle shuffle on/off\n"
         "   r                 toggle repeat on/off\n"
         "\n"
         "Configuration:\n"
         "   User-specific configuration (such as last.fm\n"
         "   scrobbling) is configured in ~/.namp/config\n"
         "\n"
         "   In order to enable scrobbling, set 'enable'\n"
         "   parameter to 'true' under [scrobble] section,\n"
         "   and set 'user' to your username and 'pass'\n"
         "   to your plaintext password. Alternatively\n"
         "   you may enter the md5 of your password under\n"
         "   'md5pass'. However, if 'pass' is set, then\n"
         "   'md5pass' will be automatically set and\n"
         "   'pass' cleared the next time namp is started.\n"
         "\n"
         "Report bugs at " PACKAGE_URL "\n"
         "\n"
         );
}


static void main_disp_version(void)
{
  printf("namp v" VERSION "\n"
         "\n"
         "Copyright (C) 2015 " AUTHOR "\n"
         "This is free software; see the source for copying\n"
         "conditions. There is NO warranty; not even for\n"
         "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
         "\n"
         "Written by " AUTHOR ".\n"
         );
}

