/*
 * uictrl.c
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
#include <ncursesw/ncurses.h>

#include "model.h"
#include "uictrl.h"
#include "uiview.h"


/* ----------- Defines ------------------------------------------- */


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */


/* ----------- Local Function Prototypes ------------------------- */
static void uictrl_process_mouse(void);


/* ----------- File Global Variables ----------------------------- */


/* ----------- Global Functions ---------------------------------- */
int uictrl_init(void)
{
  int rv = 0;
  return rv;
}


void uictrl_cleanup(void)
{
}


void uictrl_process(void)
{
  int ch = 0;
  ch = getch();
  if(ch != ERR)
  {
    if(uiview_get_search() == 1)
    {
      uiview_search_addch(ch);
    }
    else
    {
      switch(ch)
      {
        case 'z':
        case 'Z':
          model_previous();
          break;

        case 'x':
        case 'X':
          model_play();
          break;

        case 'c':
        case 'C':
          model_pause();
          break;

        case 'v':
        case 'V':
          model_stop();
          break;

        case 'b':
        case 'B':
          model_next();
          break;

        case 27:
        case 'q':
        case 'Q':
          model_quit();
          break;

        case '/':
        case 'j':
        case 'J':
          uiview_set_search(1);
          break;

        case KEY_UP:
          if(model_get_listwin())
          {
            uiview_list_track_prev();
          }
          else
          {
            model_volume_up();
          }
          break;

        case KEY_DOWN:
          if(model_get_listwin())
          {
            uiview_list_track_next();
          }
          else
          {        
            model_volume_down();
          }
          break;

        case KEY_LEFT:
          model_skip_backward();
          break;

        case KEY_RIGHT:
          model_skip_forward();
          break;

        case KEY_PPAGE:
          if(model_get_listwin())
          {
            uiview_list_track_prevpage();
          }
          break;

        case KEY_NPAGE:
          if(model_get_listwin())
          {
            uiview_list_track_nextpage();
          }
          break;

        case '\n':
          if(model_get_listwin())
          {
            uiview_list_track_play();
          }
          break;

        case KEY_IC:
          if(model_get_listwin())
          {
            uiview_list_track_enqueue();
          }
          break;

        case '+':
          model_volume_up();
          break;

        case '-':
          model_volume_down();
          break;

        case 's':
        case 'S':
          model_toggle_shuffle();
          break;

        case 'r':
        case 'R':
          model_toggle_repeat();
          break;

        case KEY_MOUSE:
          uictrl_process_mouse();
          break;

        case '\t':
          model_toggle_listwin();
          break;

        default:
          break;
      }
    }
  }
}


/* ----------- Local Functions ----------------------------------- */
static void uictrl_process_mouse(void)
{
  MEVENT mevent;
  if(getmouse(&mevent) == OK)
  {
    int uielem = 0;
    uielem = uiview_get_element(mevent.y, mevent.x);

    if(mevent.bstate & BUTTON1_DOUBLE_CLICKED)
    {
      if((uielem & UI_WIN_MASK) == UI_LISTWIN)
      {
        uiview_list_select_track(uielem & UI_ELEM_MASK);
        uiview_list_track_play();
      }
    }

    if(mevent.bstate & BUTTON1_CLICKED)
    {
      if((uielem & UI_WIN_MASK) == UI_LISTWIN)
      {
        uiview_list_select_track(uielem & UI_ELEM_MASK);
      }

      if((uielem & UI_WIN_MASK) == UI_MAINWIN)
      {
        switch((uielem & UI_ELEM_MASK))
        {
          case UI_ELEM_TIME:
            /* No action */
            break;

          case UI_ELEM_TITLE:
            /* No action */
            break;
            
          case UI_ELEM_SPECTRUM:
            /* No action */
            break;

          case UI_ELEM_VOLDOWN:
            model_volume_down();
            break;

          case UI_ELEM_VOLUP:
            model_volume_up();
            break;

          case UI_ELEM_BACKWARD:
            model_skip_backward();
            break;

          case UI_ELEM_FORWARD:
            model_skip_forward();
            break;

          case UI_ELEM_PREVIOUS:
            model_previous();
            break;

          case UI_ELEM_PLAY:
            model_play();
            break;

          case UI_ELEM_PAUSE:
            model_pause();
            break;

          case UI_ELEM_STOP:
            model_stop();
            break;

          case UI_ELEM_NEXT:
            model_next();
            break;

          case UI_ELEM_SHUFFLE:
            model_toggle_shuffle();
            break;

          case UI_ELEM_REPEAT:
            model_toggle_repeat();
            break;

          default:
            /* No action */
            break;
        }
      }
    }

    if((mevent.bstate & BUTTON1_DOUBLE_CLICKED) ||
       (mevent.bstate & BUTTON1_CLICKED))
    {
      switch((uielem & UI_WIN_MASK))
      {
        case UI_MAINWIN:
          model_set_listwin(0);
          break;

        case UI_LISTWIN:
          model_set_listwin(1);
          break;

        default:
          /* No action */
          break;
      }
    }
  }
}

