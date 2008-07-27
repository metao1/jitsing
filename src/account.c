/*
 *  TurnServer - TURN server implementation.
 *  Copyright (C) 2008 Sebastien Vincent <vincent@lsiit.u-strasbg.fr>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *    
 *  In addition, as a special exception, the copyright holders give
 *  permission to link the code of portions of this program with the
 *  OpenSSL library under certain conditions as described in each
 *  individual source file, and distribute linked combinations
 *  including the two.
 *  You must obey the GNU General Public License in all respects
 *  for all of the code used other than OpenSSL.  If you modify
 *  file(s) with this exception, you may extend this exception to your
 *  version of the file(s), but you are not obligated to do so.  If you
 *  do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source
 *  files in the program, then also delete it here.
 */

/**
 * \file account.c
 * \brief Account on TURN server (i.e. user).
 * \author Sebastien Vincent
 * \date 2008
 */

#ifndef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "account.h"

struct account_desc* account_desc_new(const char* username, const char* password, const char* realm)
{
  struct account_desc* ret = NULL;

  if(!username || !password || !realm)
  {
    return NULL;
  }

  if(!(ret = malloc(sizeof(struct account_desc))))
  {
    return NULL;
  }

  /* copy username, password and realm */
  strncpy(ret->username, username, sizeof(ret->username) -1);
  ret->username[sizeof(ret->username)-1] = 0x00;
  strncpy(ret->password, password, sizeof(ret->password) -1);
  ret->password[sizeof(ret->password)-1] = 0x00;
  strncpy(ret->realm, realm, sizeof(ret->realm) -1);
  ret->realm[sizeof(ret->realm)-1] = 0x00;

  /* set state */
  ret->state = AUTHORIZED;

  return ret;
}

void account_desc_free(struct account_desc** desc)
{
  free(*desc);
  *desc = NULL;
}

void account_desc_set_state(struct account_desc* desc, enum account_state state)
{
  desc->state = state;
}

struct account_desc* account_list_find(struct list_head* list, const char* username, const char* realm)
{
  struct list_head* get = NULL;
  struct list_head* n = NULL;

  list_iterate_safe(get, n, list)
  {
    struct account_desc* tmp = list_get(get, struct account_desc, list);
    
    if(!strcmp(tmp->username, username))
    {
      /* if realm is specified, try a match otherwise the peer is found */
      if(!realm || !strcmp(tmp->realm, realm))
      {
        return tmp;
      }
    }
  }

  /* not found */
  return NULL;
}

void account_list_free(struct list_head* list)
{
  struct list_head* get = NULL;
  struct list_head* n = NULL;

  list_iterate_safe(get, n, list)
  {
    struct account_desc* tmp = list_get(get, struct account_desc, list);
    LIST_DEL(&tmp->list);
    account_desc_free(&tmp);
  }
}

void account_list_add(struct list_head* list, struct account_desc* desc)
{
  LIST_ADD(&desc->list, list);
}

void account_list_remove(struct list_head* list, struct account_desc* desc)
{
  /* to avoid compilation warning */
  list = list;

  LIST_DEL(&desc->list);
}

int account_parse_file(struct list_head* list, const char* file)
{
  char line[512];
  char* save_ptr = NULL;
  const char* delim = ":";
  char* token = NULL;
  char* login = NULL;
  char* password = NULL;
  char* realm = NULL;
  FILE* account_file = NULL;
  struct account_desc* desc = NULL;

  account_file = fopen(file, "r");

  if(!account_file)
  {
    return -1;
  }

  while(!feof(account_file))
  {
    if(!fgets(line, sizeof(line) - 1, account_file))
    {
      continue;
    }
    
    token  = strtok_r(line, delim, &save_ptr);
    if(!token)
    {
      continue;
    }
    login = strdup(token);
    
    token  = strtok_r(NULL, delim, &save_ptr);
    if(!token)
    {
      free(login);
      continue;
    }
    password = strdup(token);

    token  = strtok_r(NULL, delim, &save_ptr);
    if(!token)
    {
      free(login);
      free(password);
      continue;
    }

    /* replace end of line by NULL character */
    save_ptr = strchr(token, '\n');
    if(save_ptr)
    {
      *save_ptr = 0x00;
    }
    
    realm = strdup(token);

    /* add it to the list */
    desc = account_desc_new(login, password, realm);
    account_list_add(list, desc);

    /* cleanup */
    desc = NULL;
    free(login);
    free(password);
    free(realm);
  }

  fclose(account_file);

  return 0;
}
