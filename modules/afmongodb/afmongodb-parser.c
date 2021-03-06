/*
 * Copyright (c) 2010-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2014 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "afmongodb.h"
#include "cfg-parser.h"
#include "afmongodb-grammar.h"

extern int afmongodb_debug;
int afmongodb_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword afmongodb_keywords[] = {
  { "mongodb",			KW_MONGODB },
  { "servers",                  KW_SERVERS },
  { "database",			KW_DATABASE },
  { "collection",		KW_COLLECTION },
  { "username",			KW_USERNAME },
  { "password",			KW_PASSWORD },
  { "safe_mode",		KW_SAFE_MODE },
  { "host",                     KW_HOST, 0, KWS_OBSOLETE, "Use the servers() option instead of host() and port()" },
  { "port",                     KW_PORT, 0, KWS_OBSOLETE, "Use the servers() option instead of host() and port()" },
  { "path",                     KW_PATH },
  { NULL }
};

CfgParser afmongodb_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &afmongodb_debug,
#endif
  .name = "afmongodb",
  .keywords = afmongodb_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) afmongodb_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afmongodb_, LogDriver **)
