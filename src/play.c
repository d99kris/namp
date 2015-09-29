/*
 * play.c
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
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "list.h"
#include "model.h"
#include "mpeg.h"
#include "play.h"
#include "uiview.h"


/* ----------- Defines ------------------------------------------- */
#define TASK_START   1
#define TASK_STOP    2
#define TASK_PAUSE   3
#define TASK_SEEK    4
#define TASK_UPDATE  5

#define THNAME       "playthread"


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */
typedef struct
{
  int type;
  char *str;
  int val;
} task_t;

typedef struct
{
  int (*isvalid)(char *path);
  int (*open)(char *path);
  void (*close)(int fd);
  int (*seek)(int fd, int offs_secs, int whence);
  int (*tell)(int fd, int *pos, int *len);
  int (*play)(int fd);
} format_t;


/* ----------- Local Function Prototypes ------------------------- */
static format_t *play_getformat(char *path);
static void play_store_pos(int pos, int len, int fin);
static gpointer play_thread(gpointer data);


/* ----------- File Global Variables ----------------------------- */
static int run = 0;
static GThread *thread = NULL;
static GAsyncQueue *tasks = NULL;
static GMutex poslen_mx;
static int position = 0;
static int length = 0;
static int finish = 0;

static format_t formats[] =
{
  {
    mpeg_isvalid,
    mpeg_open,
    mpeg_close,
    mpeg_seek,
    mpeg_tell,
    mpeg_play,
  }
};
static int formats_cnt = sizeof(formats) / sizeof(format_t);


/* ----------- Global Functions ---------------------------------- */
int play_init(void)
{
  int rv = 0;
  GError *gerror = NULL;

  /* Start play thread */
  run = 1;
  g_mutex_init(&poslen_mx);
  tasks = g_async_queue_new();
  thread = g_thread_try_new(THNAME, play_thread, NULL, &gerror);

  return rv;
}


void play_cleanup(void)
{
  /* Stop play thread */  
  run = 0;
  if(thread != NULL)
  {
    g_thread_join(thread);
    thread = NULL;
  }
  if(tasks != NULL)
  {
    g_async_queue_unref(tasks);
    tasks = NULL;
  }
  g_mutex_clear(&poslen_mx);
}


void play_start(void)
{
  task_t *task = NULL;
  task = calloc(1, sizeof(task_t));
  if(task)
  {
    task->type = TASK_START;
    g_async_queue_push(tasks, task);
  }
}


void play_stop(void)
{
  task_t *task = NULL;
  task = calloc(1, sizeof(task_t));
  if(task)
  {
    task->type = TASK_STOP;
    g_async_queue_push(tasks, task);
  }
}


void play_pause(void)
{
  task_t *task = NULL;
  task = calloc(1, sizeof(task_t));
  if(task)
  {
    task->type = TASK_PAUSE;
    g_async_queue_push(tasks, task);
  }
}


void play_seek(int offs_secs)
{
  task_t *task = NULL;
  task = calloc(1, sizeof(task_t));
  if(task)
  {
    task->type = TASK_SEEK;
    task->val = offs_secs;
    g_async_queue_push(tasks, task);
  }
}


void play_update_track(void)
{
  track_t *track = NULL;
  track = list_queue_current();
  if(track)
  {
    task_t *task = NULL;
    task = calloc(1, sizeof(task_t));
    if(task)
    {
      task->type = TASK_UPDATE;
      task->str = strdup(track->path);
      g_async_queue_push(tasks, task);
    }
  }
}


void play_update_pos(void)
{
  int pos = 0;
  int len = 0;
  int fin = 0;
  g_mutex_lock(&poslen_mx);
  pos = position;
  len = length;
  fin = finish;
  g_mutex_unlock(&poslen_mx);
  uiview_set_position(pos);
  uiview_set_duration(len);
  if(fin)
  {
    g_mutex_lock(&poslen_mx);
    finish = 0;
    model_next();
    g_mutex_unlock(&poslen_mx);
  }
}




/* ----------- Local Functions ----------------------------------- */
static format_t *play_getformat(char *path)
{
  format_t *rv = NULL;
  int i = 0;
  for(i = 0; i < formats_cnt; i++)
  {
    if(formats[i].isvalid(path))
    {
      rv = &(formats[i]);
      break;
    }
  }
  return rv;
}


static void play_store_pos(int pos, int len, int fin)
{
  g_mutex_lock(&poslen_mx);
  position = pos;
  length = len;
  finish = fin;
  g_mutex_unlock(&poslen_mx);
}


static gpointer play_thread(gpointer data)
{
  task_t *task = NULL;
  char *path = NULL;
  int fd = -1;
  format_t *format = NULL;
  int stop = 0;
  int pause = 0;
  int idle = 0;
  int pos = 0;
  int len = 0;

  while(run)
  {
    /* Assume thread is idle */
    idle = 1;

    /* Process events from application */
    task = g_async_queue_try_pop(tasks);
    if(task != NULL)
    {
      switch(task->type)
      {
        case TASK_START:
          pause = 0;
          stop = 0;
          break;

        case TASK_STOP:
          stop = 1;
          if((fd != -1) && (format != NULL))
          {
            format->seek(fd, 0, SEEK_SET);
            format->tell(fd, &pos, &len);
            play_store_pos(pos, len, 0);
          }
          break;

        case TASK_PAUSE:
          pause = 1;
          break;

        case TASK_SEEK:
          if((fd != -1) && (format != NULL))
          {
            format->seek(fd, task->val, SEEK_CUR);
            format->tell(fd, &pos, &len);
            play_store_pos(pos, len, 0);
          }
          break;

        case TASK_UPDATE:
          /* Track update, first close previous track */
          if((fd != -1) && (format != NULL))
          {
            format->close(fd);
            fd = -1;
          }
          if(path)
          {
            free(path);
          }
          /* Store copy of path, determine file format */
          path = task->str;
          format = play_getformat(path);
          if(format != NULL)
          {
            /* Open file with supported file format handler */
            fd = format->open(path);
            format->tell(fd, &pos, &len);
            play_store_pos(pos, len, 0);
          }
          else
          { 
            play_store_pos(0, 0, 1);
          }
          break;
          
        default:
          break;
      }
      free(task);
      task = NULL;
      idle = 0;
    }

    /* Playback one block of data */
    if((fd != -1) && (format != NULL))
    {
      if((!stop) && (!pause))
      {
        if(format->play(fd) == 0)
        {
          format->tell(fd, &pos, &len);
          play_store_pos(pos, len, 0);
          idle = 0;
        }
        else
        {
          play_store_pos(pos, len, 1);
        }
      }
    }

    /* If thread is idle, sleep to reduce CPU load */
    if(idle)
    {
      usleep(10000);
    }
  }

  /* Clean up dynamic resources */
  if((fd != -1) && (format != NULL))
  {
    format->close(fd);
    fd = -1;
  }

  if(path)
  {
    free(path);
    path = NULL;
  }

  return data;
}

