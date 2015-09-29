/*
 * conf.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "conf.h"
#include "utils.h"


/* ----------- Defines ------------------------------------------- */
#define GRP_GENERAL   "general"
#define GRP_SCROBBLE  "scrobble"
#define KEY_SHUFFLE   "shuffle"
#define KEY_REPEAT    "repeat"
#define KEY_ENABLE    "enable"
#define KEY_USER      "user"
#define KEY_PASS      "pass"
#define KEY_MD5PASS   "md5pass"
#define CONF_DIR      "/.namp"
#define CONF_PATH     CONF_DIR "/config"


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */


/* ----------- Local Function Prototypes ------------------------- */
static int conf_read(void);
static int conf_write(void);
static int conf_path_init(void);
static void conf_path_cleanup(void);


/* ----------- File Global Variables ----------------------------- */
static GKeyFile *conf = NULL;
static char *conf_dir = NULL;
static char *conf_path = NULL;

static int shuffle = 1;
static int repeat = 1;
static int scr_enable = 0;
static char *scr_user = NULL;
static char *scr_pass = NULL;
static char *scr_md5pass = NULL;


/* ----------- Global Functions ---------------------------------- */
int conf_init(void)
{
  int rv = 0;
  conf = g_key_file_new();
  conf_path_init();
  conf_read();
  conf_write();
  return rv;
}


void conf_cleanup(void)
{
  conf_write();
  conf_path_cleanup();
  g_key_file_free(conf);
  conf = NULL;
  if(scr_user != NULL)
  {
    free(scr_user);
    scr_user = NULL;
  }
  if(scr_pass != NULL)
  {
    free(scr_pass);
    scr_pass = NULL;
  }
  if(scr_md5pass != NULL)
  {
    free(scr_md5pass);
    scr_md5pass = NULL;
  }
}


int conf_get_shuffle(void)
{
  return shuffle;
}


void conf_set_shuffle(int flag)
{
  shuffle = flag;
}


int conf_get_repeat(void)
{
  return repeat;
}


void conf_set_repeat(int flag)
{
  repeat = flag;
}


int conf_get_scrobble_enable(void)
{
  return scr_enable;
}


char *conf_get_scrobble_user(void)
{
  return scr_user;
}


char *conf_get_scrobble_md5pass(void)
{
  return scr_md5pass;
}


/* ----------- Local Functions ----------------------------------- */
static int conf_read()
{
  int rv = 0;
  GError *err = NULL;

  if(g_key_file_load_from_file(conf, conf_path, 
                               G_KEY_FILE_KEEP_COMMENTS, &err))
  {
    /* Read from file */
    if(g_key_file_has_key(conf, GRP_GENERAL, KEY_SHUFFLE, &err))
    {
      shuffle = g_key_file_get_boolean(conf, GRP_GENERAL, 
                                       KEY_SHUFFLE, &err);
    }

    if(g_key_file_has_key(conf, GRP_GENERAL, KEY_REPEAT, &err))
    {
      repeat = g_key_file_get_boolean(conf, GRP_GENERAL, 
                                      KEY_REPEAT, &err);
    }

    if(g_key_file_has_key(conf, GRP_SCROBBLE, KEY_ENABLE, &err))
    {
      scr_enable = g_key_file_get_boolean(conf, GRP_SCROBBLE, 
                                          KEY_ENABLE, &err);
    }

    if(g_key_file_has_key(conf, GRP_SCROBBLE, KEY_USER, &err))
    {
      scr_user = g_key_file_get_string(conf, GRP_SCROBBLE, 
                                       KEY_USER, &err);
    }
    else
    {
      scr_user = strdup("");
    }

    if(g_key_file_has_key(conf, GRP_SCROBBLE, KEY_PASS, &err))
    {
      scr_pass = g_key_file_get_string(conf, GRP_SCROBBLE, 
                                       KEY_PASS, &err);
    }
    else
    {
      scr_pass = strdup("");
    }

    if(g_key_file_has_key(conf, GRP_SCROBBLE, KEY_MD5PASS, &err))
    {
      scr_md5pass = g_key_file_get_string(conf, GRP_SCROBBLE, 
                                          KEY_MD5PASS, &err);
    }
    else
    {
      scr_md5pass = strdup("");
    }

    /* Create MD5 hash of password if plain text password is set */
    if(strlen(scr_pass) > 0)
    {
      free(scr_md5pass);
      scr_md5pass = utils_md5(scr_pass);
      free(scr_pass);
      scr_pass = strdup("");
    }

    /* Write back config */
    conf_write();
  }
  else
  {
    scr_user = strdup("");
    scr_pass = strdup("");
    scr_md5pass = strdup("");

    /* Populate default structure */
    g_key_file_set_boolean(conf, GRP_GENERAL, KEY_SHUFFLE, shuffle);
    g_key_file_set_boolean(conf, GRP_GENERAL, KEY_REPEAT, repeat);
    g_key_file_set_comment(conf, GRP_GENERAL, NULL,
                           " namp configuration file", &err);
    g_key_file_set_boolean(conf, GRP_SCROBBLE, KEY_ENABLE, scr_enable);
    g_key_file_set_string(conf, GRP_SCROBBLE, KEY_USER, scr_user);
    g_key_file_set_string(conf, GRP_SCROBBLE, KEY_PASS, scr_pass);
    g_key_file_set_string(conf, GRP_SCROBBLE, KEY_MD5PASS, scr_md5pass);
  }

  return rv;
}


static int conf_write(void)
{
  int rv = 0;
  GError *err = NULL;
  struct stat stbuf;

  /* Create ~/.namp directory if not present */
  if(stat(conf_dir, &stbuf) != 0)
  {
    mkdir(conf_dir, 0700);
  }

  /* Update key values */
  g_key_file_set_boolean(conf, GRP_GENERAL, KEY_SHUFFLE, shuffle);
  g_key_file_set_boolean(conf, GRP_GENERAL, KEY_REPEAT, repeat);
  g_key_file_set_boolean(conf, GRP_SCROBBLE, KEY_ENABLE, scr_enable);
  g_key_file_set_string(conf, GRP_SCROBBLE, KEY_USER, scr_user);
  g_key_file_set_string(conf, GRP_SCROBBLE, KEY_PASS, scr_pass);
  g_key_file_set_string(conf, GRP_SCROBBLE, KEY_MD5PASS, scr_md5pass);

  /* Write to file */
  g_key_file_save_to_file(conf, conf_path, &err);

  return rv;
}


static int conf_path_init(void)
{
  int rv = 0;
  int pathlen = 0;
  const char *home = NULL;

  home = g_get_home_dir();
  pathlen = strlen(home) + strlen(CONF_PATH) + 1;

  conf_dir = calloc(pathlen, sizeof(char));
  snprintf(conf_dir, pathlen, "%s%s", home, CONF_DIR);

  conf_path = calloc(pathlen, sizeof(char));
  snprintf(conf_path, pathlen, "%s%s", home, CONF_PATH);

  return rv;
}


static void conf_path_cleanup(void)
{
  if(conf_dir)
  {
    free(conf_dir);
    conf_dir = NULL;
  }

  if(conf_path)
  {
    free(conf_path);
    conf_path = NULL;
  }
}

