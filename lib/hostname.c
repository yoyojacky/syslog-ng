/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "hostname.h"
#include "cfg.h"
#include "gsockaddr.h"
#include "messages.h"
#include "dnscache.h"

#include <arpa/inet.h>

static gchar local_hostname_fqdn[256];
static gchar local_hostname_short[256];
static gchar *custom_domain;
G_LOCK_DEFINE(resolv_lock);

#ifdef _WIN32
#include "hostname-win32.c"
#else
#include "hostname-unix.c"
#endif


void
getlonghostname(gchar *buf, gsize buflen)
{
  if (!local_hostname_fqdn[0])
    reset_cached_hostname();
  strncpy(buf, local_hostname_fqdn, buflen);
  buf[buflen - 1] = 0;
}

void
apply_custom_domain(gchar *fqdn, int length, gchar *domain)
{
  if (domain)
    {
      gchar *p = strchr(fqdn,'.');
      int index;
      int rest_length;
      if (p)
        {
          p++;
          index = (p - fqdn);
        }
      else
        {
          index = strlen(fqdn);
          p = fqdn + index;
          p[0] = '.';
          p++;
          index++;
        }
      rest_length = MIN(strlen(domain),length - index - 1);
      strncpy(p, domain,  rest_length);
      p[rest_length] = '\0';
    }
}

void reset_cached_hostname(void)
{
  gchar *s;

  gethostname(local_hostname_fqdn, sizeof(local_hostname_fqdn) - 1);
  local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
  if (strchr(local_hostname_fqdn, '.') == NULL)
    {
      /* not fully qualified, resolve it using DNS */
      char *result = get_dnsname();
      if (result)
        {
          strncpy(local_hostname_fqdn, result, sizeof(local_hostname_fqdn) - 1);
          local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
          g_free(result);
        }
    }
  apply_custom_domain(local_hostname_fqdn,sizeof(local_hostname_fqdn),custom_domain);
  /* NOTE: they are the same size, they'll fit */
  strcpy(local_hostname_short, local_hostname_fqdn);
  s = strchr(local_hostname_short, '.');
  if (s != NULL)
    *s = '\0';
}


void normalize_hostname(gchar *result, gsize *result_len, const gchar *hostname)
{
  gsize i;

  for (i = 0; hostname[i] && i < ((*result_len) - 1); i++)
    {
      result[i] = g_ascii_tolower(hostname[i]);
    }
  result[i] = '\0'; /* the closing \0 is not copied by the previous loop */
  *result_len = i;
}


const char *
get_cached_longhostname()
{
  return local_hostname_fqdn;
}

const char *
get_cached_shorthostname()
{
  return local_hostname_short;
}

void
set_custom_domain(const gchar *new_custom_domain)
{
  g_free(custom_domain);
  custom_domain = g_strdup(new_custom_domain);
  return;
}

const gchar *
get_custom_domain()
{
  return custom_domain;
}

gboolean
resolve_hostname(GSockAddr **addr, gchar *name)
{
  if (addr)
    {
#if HAVE_GETADDRINFO
      struct addrinfo hints;
      struct addrinfo *res;

      memset(&hints, 0, sizeof(hints));
      hints.ai_family = (*addr)->sa.sa_family;
      hints.ai_socktype = 0;
      hints.ai_protocol = 0;

      if (getaddrinfo(name, NULL, &hints, &res) == 0)
        {
          /* we only use the first entry in the returned list */
          switch ((*addr)->sa.sa_family)
            {
            case AF_INET:
              g_sockaddr_inet_set_address((*addr), ((struct sockaddr_in *) res->ai_addr)->sin_addr);
              break;
#if ENABLE_IPV6
            case AF_INET6:
              {
                guint16 port;

                /* we need to copy the whole sockaddr_in6 structure as it
                 * might contain scope and other required data */
                port = g_sockaddr_get_port(*addr);
                *g_sockaddr_inet6_get_sa(*addr) = *((struct sockaddr_in6 *) res->ai_addr);

                /* we need to restore the port number as it is zeroed out by the previous assignment */
                g_sockaddr_set_port(*addr, port);
                break;
              }
#endif
            default:
              g_assert_not_reached();
              break;
            }
          freeaddrinfo(res);
        }
      else
        {
          msg_error("Error resolving hostname",
                    evt_tag_str("host", name),
                    NULL);
          return FALSE;
        }
#else
      struct hostent *he;

      G_LOCK(resolv_lock);
      he = gethostbyname(name);
      if (he)
        {
          switch ((*addr)->sa.sa_family)
            {
            case AF_INET:
              g_sockaddr_inet_set_address((*addr), *(struct in_addr *) he->h_addr);
              break;
            default:
              g_assert_not_reached();
              break;
            }
          G_UNLOCK(resolv_lock);
        }
      else
        {
          G_UNLOCK(resolv_lock);
          msg_error("Error resolving hostname",
                    evt_tag_str("host", name),
                    NULL);
          return FALSE;
        }
#endif
    }
  return TRUE;
}

void
resolve_sockaddr(gchar *result, gsize *result_len, GSockAddr *saddr, gboolean usedns, gboolean usefqdn, gboolean use_dns_cache, gboolean normalize_hostnames)
{
  const gchar *hname;
  gboolean positive;
  gchar buf[256];
#ifndef HAVE_GETNAMEINFO
  gchar hostent_buf[256];
#endif

  if (saddr && saddr->sa.sa_family != AF_UNIX)
    {
      if (saddr->sa.sa_family == AF_INET
#if ENABLE_IPV6
          || saddr->sa.sa_family == AF_INET6
#endif
         )
        {
          void *addr;
          socklen_t addr_len G_GNUC_UNUSED;

          if (saddr->sa.sa_family == AF_INET)
            {
              addr = &((struct sockaddr_in *) &saddr->sa)->sin_addr;
              addr_len = sizeof(struct in_addr);
            }
#if ENABLE_IPV6
          else
            {
              addr = &((struct sockaddr_in6 *) &saddr->sa)->sin6_addr;
              addr_len = sizeof(struct in6_addr);
            }
#endif

          hname = NULL;
          if (usedns)
            {
              if ((!use_dns_cache || !dns_cache_lookup(saddr->sa.sa_family, addr, (const gchar **) &hname, &positive)) && usedns != 2)
                {
#ifdef HAVE_GETNAMEINFO
                  if (getnameinfo(&saddr->sa, saddr->salen, buf, sizeof(buf), NULL, 0, NI_NAMEREQD) == 0)
                    hname = buf;
#else
                  struct hostent *hp;
                  G_LOCK(resolv_lock);
                  hp = gethostbyaddr(addr, addr_len, saddr->sa.sa_family);
                  if (hp && hp->h_name)
                    {
                      strncpy(hostent_buf, hp->h_name, sizeof(hostent_buf));
                      hname = hostent_buf;
                    }
                  G_UNLOCK(resolv_lock);
#endif
                  if (hname)
                    positive = TRUE;

                  if (use_dns_cache && hname)
                    {
                      /* resolution success, store this as a positive match in the cache */
                      dns_cache_store(FALSE, saddr->sa.sa_family, addr, hname, TRUE);
                    }
                }
            }

          if (!hname)
            {
              inet_ntop(saddr->sa.sa_family, addr, buf, sizeof(buf));
              hname = buf;
              if (use_dns_cache)
                dns_cache_store(FALSE, saddr->sa.sa_family, addr, hname, FALSE);
            }
          else
            {
              if (!usefqdn && positive)
                {
                  /* we only truncate hostnames if they were positive
                   * matches (e.g. real hostnames and not IP
                   * addresses) */
                  const gchar *p;
                  p = strchr(hname, '.');

                  if (p)
                    {
                      if (p - hname > sizeof(buf))
                        p = &hname[sizeof(buf)] - 1;
                      memcpy(buf, hname, p - hname);
                      buf[p - hname] = 0;
                      hname = buf;
                    }
                }
            }
        }
      else
        {
          g_assert_not_reached();
        }
    }
  else
    {
      const char *hostname_fqdn = get_cached_longhostname();
      if (!hostname_fqdn[0])
        reset_cached_hostname();
      if (usefqdn)
        {
          /* avoid copy */
          hname = get_cached_longhostname();
        }
      else
        {
          hname = get_cached_shorthostname();
        }
    }
  if (normalize_hostnames)
    {
      normalize_hostname(result, result_len, hname);
    }
  else
    {
      gsize len = strlen(hname);

      if (*result_len < len - 1)
        len = *result_len - 1;
      memcpy(result, hname, len);
      result[len] = 0;
      *result_len = len;
    }
}
