/*
 * scrobbler.c
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "conf.h"
#include "list.h"
#include "scrobbler.h"
#include "utils.h"


/* ----------- Defines ------------------------------------------- */
#define SID_LEN      64
#define URL_LEN      256
#define STHNAME      "scrobblerthread"

#define TASK_NONE    0
#define TASK_PLAYING 1
#define TASK_PLAYED  2

#define URL_PLAYING  "s=%s&a=%s&t=%s&l=%d&b=%s&n=%s&m=%s"
#define URL_PLAYED   "s=%s&a[0]=%s&t[0]=%s&i[0]=%d&o[0]=%s&r[0]=%s&" \
                     "l[0]=%d&b[0]=%s&n[0]=%s&m[0]=%s"


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */
typedef struct
{
  char id[SID_LEN];
  char last_res[URL_LEN];
  char playing_url[URL_LEN];
  char played_url[URL_LEN];
} session_t;

typedef struct
{
  int type;
  char *artist;
  char *title;
  int duration;
} stask_t;


/* ----------- Local Function Prototypes ------------------------- */
static int scrobbler_connect(void);
static int scrobbler_disconnect(void);
static int scrobbler_send_played(track_t *track);
static int scrobbler_send_playing(track_t *track);
static gpointer scrobbler_thread(gpointer data);


/* ----------- File Global Variables ----------------------------- */
static int run = 0;
static GThread *thread = NULL;
static GAsyncQueue *tasks = NULL;
static session_t *session = NULL;
static int enable = 0;
static char *user = NULL;
static char *md5pass = NULL;


/* ----------- Global Functions ---------------------------------- */
int scrobbler_init(void)
{
  int rv = 0;
  GError *gerror = NULL;

  /* Get settings */
  user = conf_get_scrobble_user();
  md5pass = conf_get_scrobble_md5pass();
  enable = conf_get_scrobble_enable();

  /* Start scrobbler thread */
  run = 1;
  tasks = g_async_queue_new();
  thread = g_thread_try_new(STHNAME, scrobbler_thread, NULL, &gerror);

  return rv;
}


void scrobbler_cleanup(void)
{
  stask_t *task = NULL;
  /* Stop scrobbler thread */  
  run = 0;
  task = calloc(1, sizeof(stask_t));
  if(task)
  {
    task->type = TASK_NONE;
    g_async_queue_push(tasks, task);
  }
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
}


int scrobbler_update(track_t *track, int duration)
{
  int rv = 0;
  if(session && track)
  {
    if(duration == 10)
    {
      rv = scrobbler_send_playing(track);
    }
    else if((track->duration > 0) && ((track->duration / 2) == duration))
    {
      rv = scrobbler_send_played(track);
    }
    else if((track->duration == 0) && (duration == 60))
    {
      rv = scrobbler_send_played(track);
    }
  }
  return rv;
}


/* ----------- Local Functions ----------------------------------- */
static int scrobbler_connect(void)
{
  int rv = -1;
  char *tmp = NULL;
  char *resp = NULL;
  char url[256] = {0};
  char auth[64] = {0};
  unsigned int now = time(NULL);

  /* Check if scrobbling should be enabled */
  if((enable == 0) || (strlen(user) == 0) || (strlen(md5pass) == 0))
  {
    return rv;
  }

  /* Generate auth string */
  now = time(NULL);
  snprintf(auth, sizeof(auth), "%s%u", md5pass, now);
  tmp = utils_md5(auth);
  snprintf(auth, sizeof(auth), "%s", tmp);
  free(tmp);
  tmp = NULL;

  /* Generate authentication URL */
  sprintf(url, "http://post.audioscrobbler.com/"
          "?hs=true&p=1.2.1&c=%s&v=%s&u=%s&t=%u&a=%s",
          "NAMP", "1.0", user, now, auth);

  /* Check response */
  resp = utils_http_request(url, NULL);
  if(resp)
  {
    /* Status */
    tmp = utils_strline(resp, 0);
    if(tmp)
    {
      if(strcmp(tmp, "OK") == 0)
      {
        session = calloc(1, sizeof(session_t));
      }
      strcpy(session->last_res, tmp);
      free(tmp);
      tmp = NULL;
    }

    if(session)
    {
      /* Session id */
      tmp = utils_strline(resp, 1);
      if(tmp)
      {
        strcpy(session->id, tmp);
        free(tmp);
        tmp = NULL;
      }

      /* Now playing url */
      tmp = utils_strline(resp, 2);
      if(tmp)
      {
        strcpy(session->playing_url, tmp);
        free(tmp);
        tmp = NULL;
      }

      /* Submit url */
      tmp = utils_strline(resp, 3);
      if(tmp)
      {
        strcpy(session->played_url, tmp);
        free(tmp);
        tmp = NULL;
      }

      rv = 0;
    }
    free(resp);
    resp = NULL;
  }

  return rv;
}


static int scrobbler_disconnect(void)
{
  int rv = -1;
  if(session)
  {
    free(session);
    session = NULL;
    rv = 0;
  }
  return rv;
}


static int scrobbler_send_played(track_t *track)
{
  int rv = -1;
  if((track) && (strlen(track->artist) > 0) && (strlen(track->title) > 0))
  {
    stask_t *task = NULL;
    task = calloc(1, sizeof(stask_t));
    if(task)
    {
      task->type = TASK_PLAYED;
      task->artist = utils_urlencode(track->artist);
      task->title = utils_urlencode(track->title);
      task->duration = track->duration;
      g_async_queue_push(tasks, task);
      rv = 0;
    }
  }
  return rv;
}


static int scrobbler_send_playing(track_t *track)
{
  int rv = -1;
  if((track) && (strlen(track->artist) > 0) && (strlen(track->title) > 0))
  {
    stask_t *task = NULL;
    task = calloc(1, sizeof(stask_t));
    if(task)
    {
      task->type = TASK_PLAYING;
      task->artist = utils_urlencode(track->artist);
      task->title = utils_urlencode(track->title);
      task->duration = track->duration;
      g_async_queue_push(tasks, task);
      rv = 0;
    }
  }
  return rv;
}


static gpointer scrobbler_thread(gpointer data)
{
  stask_t *task = NULL;
  char *resp = NULL;
  unsigned int now = 0;
  char *post = NULL;
  int len = 0;

  scrobbler_connect();

  while(run)
  {
    /* Process events from application */
    task = g_async_queue_pop(tasks);
    if(task != NULL)
    {
      switch(task->type)
      {
        case TASK_PLAYING:
          len = strlen(URL_PLAYING) + strlen(session->id) + 
            strlen(task->artist) + strlen(task->title) + 
            snprintf(NULL, 0, "%d", task->duration) + 1;
          post = calloc(len, sizeof(char));
          if(post)
          {
            snprintf(post, len, URL_PLAYING, session->id, task->artist, 
                     task->title, task->duration, "", "", "");
            resp = utils_http_request(session->playing_url, post);
            if(resp)
            {
              free(resp);
              resp = NULL;
            }
            free(post);
            post = NULL;
          }
          break;

        case TASK_PLAYED:
          now = time(NULL);
          len = strlen(URL_PLAYED) + strlen(session->id) + 
            strlen(task->artist) + strlen(task->title) + 
            snprintf(NULL, 0, "%d", now) + 1 +
            snprintf(NULL, 0, "%d", task->duration) + 1;
          post = calloc(len, sizeof(char));
          if(post)
          {
            snprintf(post, len, URL_PLAYED, session->id, task->artist,
                     task->title, now, "P", "", task->duration,
                     "", "", "");
            resp = utils_http_request(session->played_url, post);
            if(resp)
            {
              free(resp);
              resp = NULL;
            }
            free(post);
            post = NULL;
          }
          break;

        case TASK_NONE:
        default:
          break;
      }
      free(task);
      task = NULL;
    }
  }

  scrobbler_disconnect();

  return data;
}

