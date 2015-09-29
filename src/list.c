/*
 * list.c
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
#include <ftw.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <tag_c.h>

#include "list.h"


/* ----------- Defines ------------------------------------------- */
#define FTW_MAXFD   512


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */


/* ----------- Local Function Prototypes ------------------------- */
static int list_add_helper(const char *path, int ftwcall);
static int ftwfn(const char *fpath, const struct stat *sb, int typeflag);
static int list_get_track_details(track_t *track);


/* ----------- File Global Variables ----------------------------- */
static GSList *list = NULL;
static GSList *queue = NULL;
static int queuepos = 0;


/* ----------- Global Functions ---------------------------------- */
int list_init(void)
{
  int rv = 0;
  return rv;
}


void list_cleanup(void)
{
  if(list != NULL)
  {
    g_slist_free(list);
    list = NULL;
  }
  if(queue != NULL)
  {
    g_slist_free(queue);
    queue = NULL;
  }
}


int list_clear(void)
{
  int rv = 0;
  return rv;
}


int list_add(const char *path)
{
  int rv = 0;
  rv = list_add_helper(path, 0);
  return rv;
}


void list_enqueue(int shuffle)
{
  int listlen = 0;
  int remaining = 0;
  int item = 0;
  int step = 0;
  char *added = NULL;
  listlen = g_slist_length(list);
  if(shuffle)
  {
    remaining = listlen;
    added = calloc(listlen, sizeof(char));
    if(added)
    {
      while(remaining > 0)
      {
        step = rand() % listlen;
        item = (item + step) % listlen;
        while(added[item])
        {
          item = (item + 1) % listlen;
        }
        queue = g_slist_append(queue, g_slist_nth_data(list, item));
        added[item] = 1;
        remaining--;
      }
      free(added);
      added = NULL;
    }
  }
  else
  {
    while(item < listlen)
    {
      queue = g_slist_append(queue, g_slist_nth_data(list, item));
      item++;
    }
  }
}


void list_queue_previous(void)
{
  if(queuepos > 0)
  {
    queuepos--;
  }
}


int list_queue_next(int shuffle, int repeat)
{
  int rv = 0;
  int queuelen = 0;
  queuelen = g_slist_length(queue);
  if((queuepos + 1) >= queuelen)
  {
    if(repeat)
    {
      list_enqueue(shuffle);
      queuelen = g_slist_length(queue);
    }
  }
  if((queuepos + 1) < queuelen)
  {
    queuepos++;
  }
  else
  {
    rv = -1;
  }
  return rv;
}


track_t *list_queue_current(void)
{
  track_t *track = NULL;
  track = g_slist_nth_data(queue, queuepos);
  return track;
}


GSList *list_get(void)
{
  return list;
}


void list_enqueue_insert_first(int list_item)
{
  queue = g_slist_insert(queue, g_slist_nth_data(list, list_item), 
                         queuepos + 1);
}


void list_enqueue_insert_first_track(track_t *track)
{
  queue = g_slist_insert(queue, track, queuepos + 1);
}


void list_queue_clear_after_current(void)
{
  track_t *track = NULL;
  track = g_slist_nth_data(queue, queuepos);
  if(queue != NULL)
  {
    g_slist_free(queue);
    queue = NULL;
  }
  queue = g_slist_append(queue, track);
  queuepos = 0;
}


void list_queue_enqueue_from_current(void)
{
  track_t *track = NULL;
  int listlen = 0;
  int item = 0;
  track = g_slist_nth_data(queue, queuepos);
  if(track)
  {
    item = g_slist_index(list, track) + 1;
    listlen = g_slist_length(list);
    while(item < listlen)
    {
      queue = g_slist_append(queue, g_slist_nth_data(list, item));
      item++;
    }
  }
}


/* ----------- Local Functions ----------------------------------- */
static int list_add_helper(const char *path, int ftwcall)
{
  int rv = 0;
  struct stat statbuf;

  if(stat(path, &statbuf) == 0)
  {
    if(S_ISREG(statbuf.st_mode))
    {
      track_t *track = calloc(1, sizeof(track_t));
      if(track)
      {
        track->path = strdup(path);
        list_get_track_details(track);
        list = g_slist_append(list, track);
      }
    }
    else if((S_ISDIR(statbuf.st_mode)) && (ftwcall == 0))
    {
      rv = ftw(path, ftwfn, FTW_MAXFD);
    }
  }
  return rv;
}


static int ftwfn(const char *fpath, const struct stat *sb, int typeflag)
{
  int rv = 0;
  if((sb != NULL) && (typeflag != FTW_DNR))
  {
    rv = list_add_helper(fpath, 1);
  }
  return rv;
}


static int list_get_track_details(track_t *track)
{
  int rv = 0;
  TagLib_File *file = NULL;
  TagLib_Tag *tag = NULL;
  const TagLib_AudioProperties *properties = NULL;

  taglib_set_strings_unicode(TRUE);
  file = taglib_file_new(track->path);
  if(file != NULL)
  {
    tag = taglib_file_tag(file);
    if(tag != NULL)
    {
      track->artist = strdup(taglib_tag_artist(tag));
      track->title = strdup(taglib_tag_title(tag));
    }

    properties = taglib_file_audioproperties(file);
    if(properties != NULL)
    {
      track->duration = taglib_audioproperties_length(properties);
    }

    taglib_tag_free_strings();
    taglib_file_free(file);
  }
  return rv;
}

