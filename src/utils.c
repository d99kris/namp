/*
 * utils.c
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
#include <curl/curl.h>
#include <openssl/md5.h>

#include <stdlib.h>
#include <string.h>

#include "utils.h"


/* ----------- Defines ------------------------------------------- */


/* ----------- Macros -------------------------------------------- */


/* ----------- Types --------------------------------------------- */
typedef struct
{
  char *ptr;
  size_t size;
} mem_t;


/* ----------- Local Function Prototypes ------------------------- */
static size_t utils_mem_write(void *ptr, size_t size, size_t nmemb, void *udata);


/* ----------- File Global Variables ----------------------------- */


/* ----------- Global Functions ---------------------------------- */
char *utils_http_request(char *url, char *post)
{
  char *rv = NULL;
  CURL *curl = NULL;
  CURLcode result = -1;
  mem_t mem;
  mem.ptr = calloc(1, sizeof(char));
  mem.size = 0;

  curl = curl_easy_init();
  if(curl)
  {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, utils_mem_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    if(post)
    {
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
    }
    result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
  }

  if(result == CURLE_OK)
  {
    rv = mem.ptr;
  }
  else
  {
    if(mem.ptr)
    {
      free(mem.ptr);
    }
  }
  return rv;
}


char *utils_md5(char *data)
{
  unsigned char md[16] = {0};
  char mdstr[33] = {0};
  char *rv = NULL;
  int i = 0;
  MD5((unsigned char *)data, strlen(data), md);
  for(i = 0; i < 16; i++)
  {
    sprintf(&mdstr[i*2], "%02x", (unsigned int)md[i]);
  }
  rv = strdup(mdstr);
  return rv;
}


char *utils_strline(char *lines, int i)
{
  char *rv = NULL;
  char *pos = NULL;
  char *next = NULL;
  char *copy = NULL;
  copy = strdup(lines);
  pos = copy;
  while(i > 0)
  {
    pos = strchr(pos, '\n');
    if(pos)
    {
      pos += 1;
    }
    else
    {
      break;
    }
    i--;
  }
  next = strchr(pos, '\n');
  if(next)
  {
    *next = '\0';
  }
  if(pos)
  {
    rv = strdup(pos);
  }
  free(copy);
  return rv;
}


char *utils_urlencode(char *str)
{
  char *rv = NULL;
  CURL *curl = NULL;
  char *tmp = NULL;

  curl = curl_easy_init();
  if(curl)
  {
    tmp = curl_easy_escape(curl, str, 0);
    rv = strdup(tmp);
    curl_free(tmp);
    curl_easy_cleanup(curl);
  }
  return rv;
}


/* ----------- Local Functions ----------------------------------- */
static size_t utils_mem_write(void *ptr, size_t size, size_t nmemb, void *udata)
{
  size_t rv = 0;
  size_t count = 0;
  mem_t *mem = NULL;
  count = size * nmemb;
  mem = (mem_t *)udata;
  mem->ptr = realloc(mem->ptr, mem->size + count + 1);
  if(mem->ptr != NULL)
  {
    memcpy(&(mem->ptr[mem->size]), ptr, count);
    mem->size += count;
    mem->ptr[mem->size] = '\0';
    rv = count;
  }
  return rv;
}

