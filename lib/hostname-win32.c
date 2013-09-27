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
#include <iphlpapi.h>
#include <windns.h>
#include <stdio.h>

char *
get_ip_for_dns_request(void)
{
  struct in_addr addr;
  char accumlator[NI_MAXHOST] = {0};
  struct hostent *local_host = NULL;
  gethostname(accumlator,NI_MAXHOST);
  local_host = gethostbyname(accumlator);
  addr.s_addr = *(u_long *)local_host->h_addr;
  sprintf(accumlator,"%d.%d.%d.%d.IN-ADDR.ARPA",addr.S_un.S_un_b.s_b4,addr.S_un.S_un_b.s_b3,addr.S_un.S_un_b.s_b2,addr.S_un.S_un_b.s_b1);
  return g_strdup(accumlator);
}

int
has_dns_server(void)
{
  DWORD length = 0;
  FIXED_INFO *buf = NULL;
  int result = 0;
  if (GetNetworkParams(NULL,&length) == ERROR_BUFFER_OVERFLOW)
     buf = malloc(length);
  if (buf && GetNetworkParams(buf,&length) == NO_ERROR)
    {
      result = strlen(buf->DnsServerList.IpAddress.String) ? 1 : 0;
    }
  free(buf);
  return result;
}

gchar *
get_dnsname(void)
{
  DWORD status = 0;
  char *result = NULL;
  PDNS_RECORD dns_record;
  if (has_dns_server())
    {
      char *ip_for_dns_request = get_ip_for_dns_request();
      status = DnsQuery(
                        ip_for_dns_request,
                        DNS_TYPE_PTR,
	                DNS_QUERY_BYPASS_CACHE | DNS_QUERY_NO_LOCAL_NAME,
                        NULL,
                        &dns_record,
                        NULL);
      g_free(ip_for_dns_request);
      if (status == 0)
        {
          result = g_strdup(dns_record->Data.PTR.pNameHost);
          DnsRecordListFree(dns_record,DnsFreeRecordListDeep);
        }
    }
  return result;
}
