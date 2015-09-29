/*
 * mpeg.c
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
#include <ao/ao.h>

#include <mpg123.h>
#include <string.h>

#include "mpeg.h"


/* ----------- Defines ------------------------------------------- */
#define BUF_DIVISOR    8     /* Small number = large buffer */


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */
typedef struct
{
  mpg123_handle *mh;
  int driver;
  unsigned char *buf;
  size_t bufsize;
  ao_device *device;
  ao_sample_format format;
} mpeg_handle_t;


/* ----------- Local Function Prototypes ------------------------- */


/* ----------- File Global Variables ----------------------------- */
static mpeg_handle_t *handle = NULL;


/* ----------- Global Functions ---------------------------------- */
int mpeg_isvalid(char *path)
{
  int rv = 0;
  int fd = 0;
  struct mpg123_frameinfo finfo;

  fd = mpeg_open(path);
  if(fd != -1)
  {
    if(handle && (mpg123_info(handle->mh, &finfo) == MPG123_OK))
    {
      rv = 1;
    }
    mpeg_close(fd);
  }

  return rv;
}


int mpeg_open(char *path)
{
  int rv = -1;
  int err = 0;
  int res = 0;
  int channels = 0;
  int encoding = 0;
  long rate = 0;

  if(handle != NULL)
  {
    goto mpeg_open_exit;
  }

  handle = calloc(1, sizeof(mpeg_handle_t));
  if(handle == NULL)
  {
    goto mpeg_open_exit;
  }

  handle->mh = mpg123_new(NULL, &err);
  if(handle->mh == NULL)
  {
    goto mpeg_open_exit;
  }

  mpg123_param(handle->mh, MPG123_FLAGS, MPG123_QUIET, 0.0);
  handle->driver = ao_default_driver_id();
  handle->bufsize = mpg123_outblock(handle->mh) / BUF_DIVISOR;
  handle->buf = (unsigned char*) malloc(handle->bufsize);
  if(handle->buf == NULL)
  {
    goto mpeg_open_exit;
  }

  res = mpg123_open(handle->mh, path);
  if(res != MPG123_OK)
  {
    goto mpeg_open_exit;
  }

  res = mpg123_getformat(handle->mh, &rate, &channels, &encoding);
  if(res != MPG123_OK)
  {
    goto mpeg_open_exit;
  }

  handle->format.bits = mpg123_encsize(encoding) * 8;
  handle->format.rate = rate;
  handle->format.channels = channels;
  handle->format.byte_format = AO_FMT_NATIVE;
  handle->format.matrix = 0;
  handle->device = ao_open_live(handle->driver, &(handle->format), NULL);
  if(handle->device == NULL)
  {
    goto mpeg_open_exit;
  }
  else
  {
    /* Return value to indicate success */
    rv = 0;
  }

 mpeg_open_exit:
  if(rv == -1)
  {
    if(handle != NULL)
    {
      if(handle->buf != NULL)
      {
        free(handle->buf);
        handle->buf = NULL;
      }

      if(handle->device != NULL)
      {
        ao_close(handle->device);
        handle->device = NULL;
      }

      if(handle->mh != NULL)
      {
        mpg123_close(handle->mh);
        mpg123_delete(handle->mh);
        handle->mh = NULL;
      }

      free(handle);
      handle = NULL;
    }
  }

  return rv;
}


void mpeg_close(int fd)
{
  if((handle != NULL) && (fd != -1))
  {
    if(handle->buf != NULL)
    {
      free(handle->buf);
      handle->buf = NULL;
    }

    if(handle->device != NULL)
    {
      ao_close(handle->device);
      handle->device = NULL;
    }

    if(handle->mh != NULL)
    {
      mpg123_close(handle->mh);
      mpg123_delete(handle->mh);
      handle->mh = NULL;
    }

    free(handle);
    handle = NULL;
  }
}


int mpeg_seek(int fd, int offs_secs, int whence)
{
  int rv = -1;
  if((handle != NULL) && (fd != -1))
  {
    off_t seek_off = 0;
    seek_off = mpg123_timeframe(handle->mh, (double)offs_secs);
    if(offs_secs < 0)
    {
      mpg123_seek_frame(handle->mh, seek_off, whence);
    }
    else
    {
      mpg123_seek_frame(handle->mh, (seek_off), whence);
    }
    rv = 0;
  }
  return rv;
}


int mpeg_tell(int fd, int *pos, int *len)
{
  int rv = -1;
  if((handle != NULL) && (fd != -1))
  {
    off_t sample_off = 0;
    off_t frame_off = 0;
    double time_off = 0;
    double time_per_frame = 0;
    int samples_per_frame = 0;
    time_per_frame = mpg123_tpf(handle->mh);
    samples_per_frame = mpg123_spf(handle->mh);

    /* Determine current position */
    frame_off = mpg123_tellframe(handle->mh);
    time_off = frame_off * (double)time_per_frame;
    *pos = (int) time_off;

    /* Determine track duration */
    sample_off = mpg123_length(handle->mh);
    if(samples_per_frame != 0)
    {
      frame_off = sample_off / samples_per_frame;
      time_off = frame_off * (double)time_per_frame;
      *len = (int) time_off;
    }

    rv = 0;
  }
  return rv;
}


int mpeg_play(int fd)
{
  int rv = -1;
  size_t bytes;

  if((handle != NULL) && (fd != -1))
  {
    if(mpg123_read(handle->mh, handle->buf, handle->bufsize, &bytes) == 
       MPG123_OK)
    {
      if(ao_play(handle->device, (char *)handle->buf, bytes) != 0)
      {
        rv = 0;
      }
    }
  }
  return rv;
}

