/*
 * model.c
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
#include <unistd.h>

#include "conf.h"
#include "list.h"
#include "mixer.h"
#include "model.h"
#include "play.h"
#include "scrobbler.h"
#include "uictrl.h"
#include "uiview.h"


/* ----------- Defines ------------------------------------------- */
#define VOLUME_MIN      0
#define VOLUME_MAX      100
#define VOLUME_STEP     5
#define SEEK_STEP       5
#define SCROBBLE_US     1000000


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */


/* ----------- Local Function Prototypes ------------------------- */


/* ----------- File Global Variables ----------------------------- */
static int run = 0;
static int volume = 0;
static int state = STATE_PLAYING;
static int duration = 0;
static int shuffle = 0;
static int repeat = 0;
static int listwin = 0;
static gint64 last_scrobble = 0;


/* ----------- Global Functions ---------------------------------- */
int model_init(void)
{
  int rv = 0;

  shuffle = conf_get_shuffle();
  repeat = conf_get_repeat();
  uiview_set_flag(state, shuffle, repeat, listwin);
  uiview_set_volume(volume);
  list_enqueue(shuffle);
  uiview_update_track();
  play_update_track();
  volume = mixer_get_volume();
  uiview_set_volume(volume);

  return rv;
}


void model_cleanup(void)
{
  conf_set_shuffle(shuffle);
  conf_set_repeat(repeat);
}


int model_mainloop(void)
{
  int rv = 0;

  run = 1;

  while(run)
  {
    model_scrobble();
    play_update_pos();
    uiview_refresh();
    uictrl_process();
    usleep(25000);
  }

  return rv;
}


void model_quit(void)
{
  run = 0;
}


void model_volume_up(void)
{
  if(volume < VOLUME_MAX)
  {
    volume += VOLUME_STEP;
  }
  uiview_set_volume(volume);
  mixer_set_volume(volume);
}


void model_volume_down(void)
{
  if(volume > VOLUME_MIN)
  {
    volume -= VOLUME_STEP;
  }
  uiview_set_volume(volume);
  mixer_set_volume(volume);
}


void model_previous(void)
{
  list_queue_previous();
  uiview_update_track();
  play_update_track();
  duration = 0;
}


void model_play(void)
{
  play_start();
  state = STATE_PLAYING;
}


void model_pause(void)
{
  play_pause();
  state = STATE_PAUSED;
}


void model_stop(void)
{
  play_stop();
  state = STATE_STOPPED;
}


void model_next(void)
{
  if(list_queue_next(shuffle, repeat) == 0)
  {
    uiview_update_track();
    play_update_track();
  }
  else
  {
    model_stop();
  }
  duration = 0;
}


void model_skip_backward(void)
{
  play_seek(-SEEK_STEP);
}


void model_skip_forward(void)
{
  play_seek(SEEK_STEP);
}


void model_toggle_shuffle(void)
{
  shuffle = !shuffle;
  uiview_set_flag(state, shuffle, repeat, listwin);
  list_queue_clear_after_current();
  if(!shuffle)
  {
    list_queue_enqueue_from_current();
  }
}


void model_toggle_repeat(void)
{
  repeat = !repeat;
  uiview_set_flag(state, shuffle, repeat, listwin);
}


void model_toggle_listwin(void)
{
  listwin = !listwin;
  uiview_set_flag(state, shuffle, repeat, listwin);
}


void model_set_listwin(int listwinflag)
{
  listwin = listwinflag;
  uiview_set_flag(state, shuffle, repeat, listwin);
}


int model_get_listwin(void)
{
  return listwin;
}


int model_scrobble(void)
{
  int rv = 0;
  gint64 now_us = 0;
  track_t *track = NULL;

  now_us = g_get_monotonic_time();
  if((now_us - last_scrobble) > SCROBBLE_US)
  {
    if(state == STATE_PLAYING)
    {
      duration++;
      track = list_queue_current();
      scrobbler_update(track, duration);
    }
    last_scrobble = now_us;
  }

  return rv;
}

