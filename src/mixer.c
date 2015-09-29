/*
 * mixer.c
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

#include <alloca.h>

#include "mixer.h"


/* ----------- Defines ------------------------------------------- */


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */


/* ----------- Local Function Prototypes ------------------------- */


/* ----------- File Global Variables ----------------------------- */


/* ----------- Global Functions ---------------------------------- */
int mixer_set_volume(int percentage)
{
  snd_mixer_selem_id_t *selem = NULL;
  snd_mixer_t *mixer = NULL;
  snd_mixer_elem_t *elem = NULL;
  long volume = 0;
  long min = 0;
  long max = 0;
  int rv = 0;

  volume = percentage;

  if(snd_mixer_open(&mixer, 0) == 0)
  {
    snd_mixer_attach(mixer, "default");
    snd_mixer_selem_register(mixer, NULL, NULL);
    snd_mixer_load(mixer);
    snd_mixer_selem_id_alloca(&selem);
    snd_mixer_selem_id_set_index(selem, 0);
    snd_mixer_selem_id_set_name(selem, "Master");
    elem = snd_mixer_find_selem(mixer, selem);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);
    snd_mixer_close(mixer);
    mixer = NULL;
  }

  return rv;
}


int mixer_get_volume(void)
{
  snd_mixer_selem_id_t *selem = NULL;
  snd_mixer_t *mixer = NULL;
  snd_mixer_elem_t *elem = NULL;
  long volume = 0;
  long min = 0;
  long max = 0;
  int rv = 0;

  if(snd_mixer_open(&mixer, 0) == 0)
  {
    snd_mixer_attach(mixer, "default");
    snd_mixer_selem_register(mixer, NULL, NULL);
    snd_mixer_load(mixer);
    snd_mixer_selem_id_alloca(&selem);
    snd_mixer_selem_id_set_index(selem, 0);
    snd_mixer_selem_id_set_name(selem, "Master");
    elem = snd_mixer_find_selem(mixer, selem);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, 0, &volume);
    rv = volume * 100 / max;
    snd_mixer_close(mixer);
    mixer = NULL;
  }
  return rv;
}

