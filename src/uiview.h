/*
 * uiview.h
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
#define UI_ELEM_MASK         0x0000ffff
#define UI_WIN_MASK          0x000f0000

#define UI_ELEM_TIME         0x01
#define UI_ELEM_TITLE        0x02
#define UI_ELEM_SPECTRUM     0x03
#define UI_ELEM_VOLDOWN      0x04
#define UI_ELEM_VOLUP        0x05
#define UI_ELEM_BACKWARD     0x06
#define UI_ELEM_FORWARD      0x07
#define UI_ELEM_PREVIOUS     0x08
#define UI_ELEM_PLAY         0x09
#define UI_ELEM_PAUSE        0x0a
#define UI_ELEM_STOP         0x0b
#define UI_ELEM_NEXT         0x0c
#define UI_ELEM_SHUFFLE      0x0d
#define UI_ELEM_REPEAT       0x0e

#define UI_MAINWIN           0x010000
#define UI_LISTWIN           0x020000


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */


/* ----------- Global Function Prototypes ------------------------ */
int uiview_init(void);
void uiview_cleanup(void);
void uiview_refresh(void);
void uiview_set_flag(int play, int shuffle, int repeat, int listwin);
void uiview_set_volume(int percent);
void uiview_set_duration(int seconds);
void uiview_set_position(int seconds);
void uiview_set_spectrum(int *values);
void uiview_update_track(void);
int uiview_get_element(int y, int x);
void uiview_list_track_prev(void);
void uiview_list_track_next(void);
void uiview_list_track_prevpage(void);
void uiview_list_track_nextpage(void);
void uiview_list_select_track(int view_index);
void uiview_list_track_play(void);
void uiview_list_track_enqueue(void);
void uiview_set_search(int search);
int uiview_get_search(void);
void uiview_search_addch(int ch);

