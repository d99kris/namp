/*
 * model.h
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


/* ----------- Defines ------------------------------------------- */
#define STATE_STOPPED     0
#define STATE_PLAYING     1
#define STATE_PAUSED      2


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */


/* ----------- Global Function Prototypes ------------------------ */
int model_init(void);
void model_cleanup(void);
int model_mainloop(void);
void model_quit(void);
void model_volume_up(void);
void model_volume_down(void);
void model_previous(void);
void model_play(void);
void model_pause(void);
void model_stop(void);
void model_next(void);
void model_skip_backward(void);
void model_skip_forward(void);
void model_toggle_shuffle(void);
void model_toggle_repeat(void);
void model_toggle_listwin(void);
void model_set_listwin(int listwinflag);
int model_get_listwin(void);
int model_scrobble(void);

