/*
 * Copyright (C) 2006 Vincent Wagelaar
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smbctx.h"
#include "debug.h"

#include "haiku/support.h"


config_t *fusesmb_auth_fn_cfg = NULL;
pthread_mutex_t *fusesmb_auth_fn_cfg_mutex = NULL;

static int nmblookup(const char *ip_server, char *output, size_t outputsize)
{
    char ipcmd[1024] = "nmblookup -A ";
    strcat(ipcmd, ip_server);
    FILE *pipe = popen(ipcmd, "r");
    if (NULL == pipe)
        return -1;
    while (!feof(pipe))
    {
        char buf2[4096];
        char buf[4096];
        char ip[32];

        char *start = buf;
        if (NULL == fgets(buf2, 4096, pipe))
            continue;
        if (strncmp(buf2, "Looking up status of ", strlen("Looking up status of ")) == 0)
        {
            char *tmp = rindex(buf2, ' ');
            tmp++;
            char *end = index(tmp, '\n');
            *end = '\0';
            strcpy(ip, tmp);
        }
        else
        {
            continue;
        }

        while (!feof(pipe))
        {

            if (NULL == fgets(buf, 4096, pipe))
                break;
            char *sep = buf;

            if (*buf != '\t')
                break;
            if (NULL != strstr(buf, "<GROUP>"))
                break;
            if (NULL == (sep = strstr(buf, "<00>")))
                break;
            *sep = '\0';

            start++;

            while (*sep == '\t' || *sep == ' ' || *sep == '\0')
            {
                *sep = '\0';
                sep--;
            }
            strlcpy(output, start, outputsize);
        }

    }
    pclose(pipe);
    return 0;
}


static void fusesmb_auth_fn(const char *server, const char *share,
                            char *workgroup, int wgmaxlen,
                            char *username, int unmaxlen,
                            char *password, int pwmaxlen)
{
	get_authentication(server, share, workgroup, wgmaxlen, username,
        unmaxlen, password, pwmaxlen);
}

static void fusesmb_cache_auth_fn(const char *server, const char *share,
                                  char *workgroup, int wgmaxlen,
                                  char *username, int unmaxlen,
                                  char *password, int pwmaxlen)
{

    (void)workgroup;
    (void)wgmaxlen;
    char sv[1024];

    /* Don't authenticate for workgroup listing */
    if (NULL == server || server[0] == '\0')
    {
        fprintf(stderr, "empty server name");
        return;
    }
    debug("server: %s : share: %s : workgroup: %s", server, share, workgroup);

    /* Convert ip to server name */
    nmblookup(server, sv, 1024);

	get_authentication(sv, share, workgroup, wgmaxlen, username,
        unmaxlen, password, pwmaxlen);
}

/*
 * Create a new libsmbclient context with all necessary options
 */
static SMBCCTX *fusesmb_context(smbc_get_auth_data_fn fn)
{
    /* Initializing libsbmclient */
    SMBCCTX *ctx;
    ctx = smbc_new_context();
    if (ctx == NULL)
        return NULL;

    ctx->callbacks.auth_fn = fn;
    //ctx->debug = 4;
    /* Timeout a bit bigger, by Jim Ramsay */
    ctx->timeout = 10000;       //10 seconds
    /* Kerberos authentication by Esben Nielsen */
#if defined(SMB_CTX_FLAG_USE_KERBEROS) && defined(SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS)
    ctx->flags |=
        SMB_CTX_FLAG_USE_KERBEROS | SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS;
#endif
    //ctx->options.one_share_per_server = 1;
    ctx = smbc_init_context(ctx);
    return ctx;
}

SMBCCTX *fusesmb_cache_new_context(config_t *cf)
{
    fusesmb_auth_fn_cfg = cf;
    return fusesmb_context(fusesmb_cache_auth_fn);
}

SMBCCTX *fusesmb_new_context(config_t *cf, pthread_mutex_t *mutex)
{
    fusesmb_auth_fn_cfg = cf;
    fusesmb_auth_fn_cfg_mutex = mutex;
    return fusesmb_context(fusesmb_auth_fn);
}

