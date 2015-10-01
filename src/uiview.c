/*
 * uiview.c
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
#define _GNU_SOURCE
#include <ctype.h>
#include <glib.h>
#include <ncursesw/ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "list.h"
#include "main.h"
#include "model.h"
#include "uiview.h"


/* ----------- Defines ------------------------------------------- */
#define MW_H          6
#define MW_W          40
#define MW_Y          0
#define MW_X          0
#define SPEC_SZ       8
#define SPEC_VALS     9
#define SECS_PER_MIN  60
#define MAX_TRACK_SZ  27
#define UI_UPDATE_US  100000
#define TITLEHOLD_US  4000000
#define TITLESTEP_US  250000

#define PL_MINH       6
#define PL_W          40


/* ----------- Macros -------------------------------------------- */
#define STR_HELPER(x) #x
#define STR(x)        STR_HELPER(x)


/* ----------- Types --------------------------------------------- */


/* ----------- Local Function Prototypes ------------------------- */
static void uiview_check_screen(void);
static void uiview_draw_mainwin(void);
static void uiview_draw_listwin(void);
static void uiview_draw_searchwin(void);


/* ----------- File Global Variables ----------------------------- */
static WINDOW *mainwin = NULL;
static WINDOW *listwin = NULL;

static int playflag = 0;
static int shuffleflag = 0;
static int repeatflag = 0;
static int listwinflag = 0;
static int volumepercent = 0;
static int trackduration = 0;
static int trackposition = 0;
static int spectrum[SPEC_SZ] = { 0 };
static wchar_t *trackname = NULL;
static int tracknameposition = 0;
static wchar_t *notracktitle = L"";
static wchar_t spectrum_wchars[SPEC_VALS] = L" ▁▂▃▄▅▆▇█";
static gint64 last_ui_update_us = 0;
static gint64 last_title_start_us = 0;
static gint64 last_title_stop_us = 0;
static int volume_draw_cnt = 0;
static int duration_draw_cnt = 0;
static int screen_h = -1;
static int screen_w = -1;
static int pl_y = 0;
static int pl_h = 0;
static int pl_x = 0;
static int pl_w = 0;
static int selected_track = -1;
static int track_max = 0;
static int list_len = 0;
static int view_max = 0;
static int list_offset = 0;
static int prevent_offset_update = 0;
static int user_selected_track = 0;
static int searchmode = 0;
static char searchstr[128] = { 0 };
static int searchstr_pos = 0;
static int searchstr_len = 0;
static GSList *search_results = NULL;
static int search_selected = 0;


/* ----------- Global Functions ---------------------------------- */
int uiview_init(void)
{
  int rv = 0;

  initscr();
  cbreak();
  noecho();
  curs_set(0);
  nodelay(stdscr, TRUE);
  keypad(stdscr, TRUE);
  mousemask(ALL_MOUSE_EVENTS, NULL);
  mouseinterval(300);
 
  return rv;
}


void uiview_cleanup(void)
{
  if(trackname)
  {
    free(trackname);
    trackname = NULL;
  }
  if(listwin)
  {
    delwin(listwin);
    listwin = NULL;
  }
  if(mainwin)
  {
    delwin(mainwin);
    mainwin = NULL;
  }
  endwin();
}


void uiview_refresh(void)
{
  uiview_check_screen();
  uiview_draw_mainwin();
  if(searchmode)
  {
    uiview_draw_searchwin();
  }
  else
  {
    uiview_draw_listwin();
  }
}


void uiview_set_flag(int play, int shuffle, int repeat, int listwinfocus)
{
  playflag = play;
  shuffleflag = shuffle;
  repeatflag = repeat;
  listwinflag = listwinfocus;
}


void uiview_set_volume(int percent)
{
  volumepercent = percent;
}


void uiview_set_duration(int seconds)
{
  trackduration = seconds;
}


void uiview_set_position(int seconds)
{
  trackposition = seconds;
}


void uiview_set_spectrum(int *values)
{
  int i = 0;
  for(i = 0; i < SPEC_SZ; i++)
  {
    if(values[i] >= spectrum[i])
    {
      spectrum[i] = values[i];
    }
    else if(spectrum[i] > 0)
    {
      spectrum[i]--;
    }
  }
}


void uiview_update_track(void)
{
  track_t *track = NULL;
  track = list_queue_current();
  if(track)
  {
    if(trackname)
    {
      free(trackname);
    }
    if(track->artist && (strlen(track->artist) > 0) &&
       track->title && (strlen(track->title) > 0))
    {
      int estlen = 0;
      estlen = strlen(track->artist) + strlen(" - ") +
        strlen(track->title) + strlen(" (999:59:59)") + 1;
      trackname = calloc(estlen, sizeof(wchar_t));
      swprintf(trackname, estlen, L"%s - %s (%d:%02d)",
               track->artist, track->title,
               track->duration / 60, track->duration % 60);
    }
    else
    {
      int estlen = 0;
      estlen = strlen(basename(track->path)) + 1;
      trackname = calloc(estlen, sizeof(wchar_t));
      swprintf(trackname, estlen, L"%s%", basename(track->path));
    }
    tracknameposition = 0;
    last_title_start_us = 0;
    selected_track = -1;
    if(user_selected_track == 0)
    {
      prevent_offset_update = 0;
    }
  }
}


int uiview_get_element(int y, int x)
{
  int rv = 0;

  switch(y)
  {
    case 1:
      if((x >= 4) && (x <= 9))
      {
        rv = UI_ELEM_TIME;
      }
      else if((x >= 11) && (x <= 37))
      {
        rv = UI_ELEM_TITLE;
      }
      break;

    case 2:
      if((x >= 2) && (x <= 9))
      {
        rv = UI_ELEM_SPECTRUM;
      }
      else if((x >= 11) && (x <= (11 + volume_draw_cnt)))
      {
        rv = UI_ELEM_VOLDOWN;
      }
      else if((x > (11 + volume_draw_cnt)) && (x <= 31))
      {
        rv = UI_ELEM_VOLUP;
      }
      break;

    case 3:
      if((x >= 2) && (x <= (2 + duration_draw_cnt)))
      {
        rv = UI_ELEM_BACKWARD;
      }
      else if((x > (2 + duration_draw_cnt)) && (x <= 37))
      {
        rv = UI_ELEM_FORWARD;
      }
      break;

    case 4:
      if((x >= 2) && (x <= 3))
      {
        rv = UI_ELEM_PREVIOUS;
      }
      else if((x >= 5) && (x <= 6))
      {
        rv = UI_ELEM_PLAY;
      }
      else if((x >= 8) && (x <= 9))
      {
        rv = UI_ELEM_PAUSE;
      }
      else if((x >= 11) && (x <= 12))
      {
        rv = UI_ELEM_STOP;
      }
      else if((x >= 14) && (x <= 15))
      {
        rv = UI_ELEM_NEXT;
      }
      else if(x == 19)
      {
        rv = UI_ELEM_SHUFFLE;
      }
      else if(x == 32)
      {
        rv = UI_ELEM_REPEAT;
      }
      break;

    default:
      break;
  }

  if((y >= MW_Y) && (y < MW_H) && (x >= MW_X) && (x <= MW_W))
  {
    rv |= UI_MAINWIN;
  }
  else if((y >= pl_y) && (y <= (pl_y + pl_h)) && 
          (x >= pl_x) && (x <= (pl_x + pl_w)))
  {
    rv |= UI_LISTWIN;
    rv |= (y - pl_y);
  }

  return rv;
}


void uiview_list_track_prev(void)
{
  if(selected_track != -1)
  {
    if(selected_track > 0)
    {
      selected_track--;
      prevent_offset_update = 0;
    }
  }
}


void uiview_list_track_next(void)
{
  if(selected_track != -1)
  {
    if((selected_track + 1) < list_len)
    {
      selected_track++;
      prevent_offset_update = 0;
    }
  }
}


void uiview_list_track_prevpage(void)
{
  if(selected_track != -1)
  {
    if(selected_track > 0)
    {
      selected_track -= track_max;
      if(selected_track < 0)
      {
        selected_track = 0;
      }
      prevent_offset_update = 0;
    }
  }
}


void uiview_list_track_nextpage(void)
{
  if(selected_track != -1)
  {
    if((selected_track + 1) < list_len)
    {
      selected_track += track_max;
      if(selected_track >= list_len)
      {
        selected_track = list_len - 1;
      }
      prevent_offset_update = 0;
    }
  }
}


void uiview_list_select_track(int view_index)
{
  view_index--;
  if((view_index >= 0) && (view_index < (pl_h - 2)))
  {
    selected_track = view_index + list_offset;
    prevent_offset_update = 1;
  }
}


void uiview_list_track_play(void)
{
  list_enqueue_insert_first(selected_track);
  user_selected_track = 1;
  model_next();
  user_selected_track = 0;
}


void uiview_list_track_enqueue(void)
{
  list_enqueue_insert_first(selected_track);
}


void uiview_set_search(int search)
{
  if(listwin == NULL)
  {
    return;
  }

  searchmode = search;
  if(searchmode)
  {
    memset(searchstr, 0, sizeof(searchstr));
    searchstr_pos = 0;
    searchstr_len = 0;
    search_selected = 0;
    curs_set(1);
  }
  else
  {
    curs_set(0);
  }

  model_set_listwin(1);
}


int uiview_get_search(void)
{
  return searchmode;
}


void uiview_search_addch(int ch)
{
  if(ispunct(ch) || isalnum(ch) || (ch == ' '))
  {
    if(searchstr_pos < searchstr_len)
    {
      memmove(&(searchstr[searchstr_pos + 1]),
              &(searchstr[searchstr_pos]), searchstr_len - searchstr_pos);
    }
    searchstr[searchstr_pos] = (char)ch;
    searchstr_pos++;
    searchstr_len++;
    search_selected = 0;
  }
  else
  {
    switch(ch)
    {
      case '\n':
        /* Play selected track */
        {
          track_t *track = g_slist_nth_data(search_results, search_selected);
          if(track)
          {
            list_enqueue_insert_first_track(track);
            model_next();
          }
          uiview_set_search(0);
        }
        break;

      case KEY_IC:
        /* Enqueue selected track */
        {
          track_t *track = g_slist_nth_data(search_results, search_selected);
          if(track)
          {
            list_enqueue_insert_first_track(track);
          }
        }
        break;

      case KEY_LEFT:
        if(searchstr_pos > 0)
        {
          searchstr_pos--;
        }
        break;

      case KEY_RIGHT:
        if(searchstr_pos < searchstr_len)
        {
          searchstr_pos++;
        }
        break;

      case KEY_UP:
        if(search_selected > 0)
        {
          search_selected--;
        }
        break;

      case KEY_DOWN:
        if(search_selected < ((int)g_slist_length(search_results) - 1))
        {
          search_selected++;
        }
        break;

      case KEY_BACKSPACE:
        if(searchstr_pos > 0)
        {
          if(searchstr_pos < searchstr_len)
          {
            memmove(&(searchstr[searchstr_pos - 1]),
                    &(searchstr[searchstr_pos]), 
                    searchstr_len - searchstr_pos);
          }
          searchstr_pos--;
          searchstr_len--;
          searchstr[searchstr_len] = '\0';
          search_selected = 0;
        }
        break;

      case 27:
        uiview_set_search(0);
        break;

      default:
        break;
    }
  }
}


/* ----------- Local Functions ----------------------------------- */
static void uiview_check_screen(void)
{
  int new_screen_h = 0;
  int new_screen_w = 0;

  getmaxyx(stdscr, new_screen_h, new_screen_w);

  if((new_screen_h != screen_h) || (new_screen_w != screen_w))
  {
    screen_h = new_screen_h;
    screen_w = new_screen_w;

    /* Clear screen */
    wclear(stdscr);

    /* Delete old windows */
    if(mainwin != NULL)
    {
      delwin(mainwin);
    }
    if(listwin != NULL)
    {
      delwin(listwin);
    }

    /* Set up main window (always, irrespective of console size) */
    mainwin = newwin(MW_H, MW_W, MW_Y, MW_X);

    /* Set up play list window based on console size */
    if(screen_h >= (MW_H + PL_MINH))
    {
      /* If playlist can fit under main window that's preferred */
      pl_h = (screen_h - MW_H);
      pl_w = PL_W;
      pl_y = MW_H;
      pl_x = 0;
      listwin = newwin(pl_h, pl_w, pl_y, pl_x);
      track_max = pl_h - 2;
    }
    else if(screen_w >= (MW_W + PL_W))
    {
      /* Second option is to see if it fits on right side of main window */
      pl_h = PL_MINH;
      pl_w = PL_W;
      pl_y = 0;
      pl_x = MW_W;
      listwin = newwin(pl_h, pl_w, pl_y, pl_x);
      track_max = pl_h - 2;
    }
    else
    {
      /* Do not enable playlist window */
      listwin = NULL;
      track_max = 0;
    }
  }
}


static void uiview_draw_mainwin(void)
{
  int i = 0;
  wchar_t *tracktmp = NULL;
  wchar_t displaytitle[MAX_TRACK_SZ + 1] = { 0 };
  char displayshuffle = ' ';
  char displayrepeat = ' ';
  gint64 now_us = 0;
  gint64 offs = 0;

  /* Get current time */
  now_us = g_get_monotonic_time();

  /* Only perform update at certain interval (currently 100ms) */
  if((now_us - last_ui_update_us) > UI_UPDATE_US)
  {
    /* Main window border */
    wborder(mainwin, 0, 0, 0, 0, 0, 0, 0, 0);

    /* Main window title */
    if(!listwinflag)
    {
      wattron(mainwin, A_BOLD);      
      mvwprintw(mainwin, 0, 17, " namp ");
      wattroff(mainwin, A_BOLD);
    }
    else
    {
      mvwprintw(mainwin, 0, 17, " namp ");
    }

    /* Current track position */
    mvwprintw(mainwin, 1, 3, " %02d:%02d", 
              (trackposition / SECS_PER_MIN), 
              (trackposition % SECS_PER_MIN));

    /* Track title */
    if(trackname == NULL)
    {
      tracktmp = notracktitle;
    }
    else
    {
      tracktmp = trackname;
    }

    swprintf(displaytitle, MAX_TRACK_SZ, L"%ls", tracktmp + tracknameposition);

    mvwprintw(mainwin, 1, 11, "%-" STR(MAX_TRACK_SZ) "s", "");
    mvwaddnwstr(mainwin, 1, 11, displaytitle, sizeof(displaytitle));
    if(wcslen(tracktmp) > MAX_TRACK_SZ)
    {
      if((now_us - last_title_start_us) > TITLEHOLD_US)
      {
        offs = (((now_us-last_title_start_us)-TITLEHOLD_US)/TITLESTEP_US);
        tracknameposition = offs;
        if(tracknameposition > ((int)wcslen(tracktmp) - MAX_TRACK_SZ + 1))
        {
          if(last_title_stop_us < last_title_start_us)
          {
            last_title_stop_us = now_us;
          }

          if((now_us - last_title_stop_us) > TITLEHOLD_US)
          {
            tracknameposition = 0;
            last_title_start_us = now_us;            
          }
          else
          {
            tracknameposition = (wcslen(tracktmp) - MAX_TRACK_SZ + 1);
          }
        }
      }
    }

    /* Volume */
    mvwprintw(mainwin, 2, 11, "-                   +   PL");
    volume_draw_cnt = (19 * volumepercent) / 100;
    mvwhline(mainwin, 2, 12, 0, volume_draw_cnt);

    /* Progress */
    mvwprintw(mainwin, 3, 2, "|                                  |");
    if(trackduration != 0)
    {
      duration_draw_cnt = (34 * trackposition) / trackduration;
      mvwhline(mainwin, 3, 3, 0, duration_draw_cnt);
    }
    else
    {
      duration_draw_cnt = 0;
    }

    /* Spectrum */
    wmove(mainwin, 2, 2);
    for(i = 0; i < 8; i++)
    {
      waddnwstr(mainwin, &spectrum_wchars[spectrum[i]], 1);
    }

    /* Playback control buttons */
    if(shuffleflag)
    {
      displayshuffle = 'X';
    }
    if(repeatflag)
    {
      displayrepeat = 'X';
    }
    mvwprintw(mainwin, 4, 2, "|< |> || [] >|  [%c] Shuffle  [%c] Rep",
              displayshuffle, displayrepeat);

    /* Refresh window */
    wrefresh(mainwin);

    /* Store time */
    last_ui_update_us = now_us;
  }
}


static void uiview_draw_listwin(void)
{
  if(listwin != NULL)
  {
    GSList *list = NULL;
    int i = 0;
    track_t *current_track = NULL;

    /* List window border */
    wborder(listwin, 0, 0, 0, 0, 0, 0, 0, 0);

    /* List window title */
    if(listwinflag)
    {
      wattron(listwin, A_BOLD);      
      mvwprintw(listwin, 0, 15, " playlist ");
      wattroff(listwin, A_BOLD);
    }
    else
    {
      mvwprintw(listwin, 0, 15, " playlist ");
    }

    /* List tracks */
    list = list_get();
    list_len = g_slist_length(list);
    current_track = list_queue_current();

    /* Determine index of selected track */
    if(selected_track == -1)
    {
      track_t *track = NULL;
      for(i = 0; i < list_len; i++)
      {
        track = g_slist_nth_data(list, i + list_offset);
        if(track == current_track)
        {
          selected_track = i + list_offset;
        }
      }
    }

    /* Try center around selected track */
    if(prevent_offset_update == 0)
    {
      list_offset = selected_track - (track_max / 2);
      if((list_offset + track_max) > list_len)
      {
        list_offset = list_len - track_max;
      }
      if(list_offset < 0)
      {
        list_offset = 0;
      }
    }

    /* Determine max tracks to view */
    if(list_len > track_max)
    {
      view_max = track_max;
    }
    else
    {
      view_max = list_len;
    }

    /* Show track list */
    for(i = 0; i < view_max; i++)
    {
      wchar_t track_item[MW_W];
      track_t *track = NULL;
      track = g_slist_nth_data(list, i + list_offset);
      if((strlen(track->artist) > 0) && (strlen(track->title) > 0))
      {
        swprintf(track_item, MW_W - 3, L"%s - %s%-36s",
                 track->artist, track->title, "");
      }
      else
      {
        swprintf(track_item, MW_W - 3, L"%s%-36s", basename(track->path), "");
      }

      if(listwinflag && ((i + list_offset) == selected_track))
      {
        wattron(listwin, A_REVERSE);
        mvwaddnwstr(listwin, i + 1, 2, track_item, MW_W - 4);
        wattroff(listwin, A_REVERSE);
      }
      else
      {
        mvwaddnwstr(listwin, i + 1, 2, track_item, MW_W - 4);
      }
    }
    list = NULL;

    /* Refresh window */
    wrefresh(listwin);
  }
}


static void uiview_draw_searchwin(void)
{
  if(listwin != NULL)
  {
    GSList *list = NULL;
    int i = 0;

    /* Search window border */
    wborder(listwin, 0, 0, 0, 0, 0, 0, 0, 0);

    /* Clear old results */
    if(search_results != NULL)
    {
      g_slist_free(search_results);
      search_results = NULL;
    }

    /* Find matching tracks */
    list = list_get();
    list_len = g_slist_length(list);
    for(i = 0; i < list_len; i++)
    {
      track_t *track = g_slist_nth_data(list, i);
      if(strcasestr(track->title, searchstr) ||
         strcasestr(track->artist, searchstr) ||
         strcasestr(track->path, searchstr))
      {
        search_results = g_slist_append(search_results, track);        
      }
    }
    list_len = g_slist_length(search_results);

    /* Try center around selected track */
    //if(prevent_offset_update == 0)
    {
      list_offset = search_selected - (track_max / 2);
      if((list_offset + track_max) > list_len)
      {
        list_offset = list_len - track_max;
      }
      if(list_offset < 0)
      {
        list_offset = 0;
      }
    }

    /* Determine max tracks to view */
    if(list_len > track_max)
    {
      view_max = track_max;
    }
    else
    {
      view_max = list_len;
    }

    /* Show track list */
    for(i = 0; i < view_max; i++)
    {
      wchar_t track_item[MW_W];
      track_t *track = NULL;
      track = g_slist_nth_data(search_results, i + list_offset);
      if((strlen(track->artist) > 0) && (strlen(track->title) > 0))
      {
        swprintf(track_item, MW_W - 3, L"%s - %s%-36s",
                 track->artist, track->title, "");
      }
      else
      {
        swprintf(track_item, MW_W - 3, L"%s%-36s", basename(track->path), "");
      }

      if((i + list_offset) == search_selected)
      {
        wattron(listwin, A_REVERSE);
        mvwaddnwstr(listwin, i + 1, 2, track_item, MW_W - 4);
        wattroff(listwin, A_REVERSE);
      }
      else
      {
        mvwaddnwstr(listwin, i + 1, 2, track_item, MW_W - 4);
      }
    }
    for(; i < track_max; i++)
    {
      mvwprintw(listwin, i + 1, 2, "%-36s", "");
    }
    list = NULL;

    /* Search window title */
    wattron(listwin, A_BOLD);      
    mvwprintw(listwin, 0, 2, " search: %-26s ", searchstr);
    wattroff(listwin, A_BOLD);
    wmove(listwin, 0, 11 + searchstr_pos);

    /* Refresh window */
    wrefresh(listwin);
  }
}

