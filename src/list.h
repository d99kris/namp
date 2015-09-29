/*
 * list.h
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
#include <glib.h>


/* ----------- Defines ------------------------------------------- */


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */
typedef struct
{
  char *path;
  char *artist;
  char *title;
  int duration;
} track_t;


/* ----------- Global Function Prototypes ------------------------ */
int list_init(void);
void list_cleanup(void);
int list_clear(void);
int list_add(const char *path);
void list_enqueue(int shuffle);
void list_queue_previous(void);
int list_queue_next(int shuffle, int repeat);
track_t *list_queue_current(void);
GSList *list_get(void);
void list_enqueue_insert_first(int list_item);
void list_enqueue_insert_first_track(track_t *track);
void list_queue_clear_after_current(void);
void list_queue_enqueue_from_current(void);

