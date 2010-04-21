#ifndef __EDG_WORKLOAD_LOGGING_LOCALLOGGER_LOGD_PROTO_H__
#define __EDG_WORKLOAD_LOGGING_LOCALLOGGER_LOGD_PROTO_H__

#ident "$Header$"
/*
Copyright (c) Members of the EGEE Collaboration. 2004-2010.
See http://www.eu-egee.org/partners for details on the copyright holders.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/


/**
 * \file edg/workload/logging/locallogger/logd_proto.h
 * \brief server part of the logging protocol
 * \note private
 */

#ifdef __cplusplus
extern "C" {
#endif


#include <syslog.h>

#include "glite/lb/log_proto.h"
#include "glite/security/glite_gss.h"

int edg_wll_log_proto_server(edg_wll_GssConnection *con, struct timeval *timeout, char *name, char *prefix, int noipc, int noparse);
int edg_wll_log_proto_server_failure(int code, edg_wll_GssStatus *gss_code, const char *text);

#define SYSTEM_ERROR(my_err) { \
	if (errno !=0 ) \
		edg_wll_ll_log(LOG_ERR,"%s: %s\n",my_err,strerror(errno)); \
	else \
		edg_wll_ll_log(LOG_ERR,"%s\n",my_err); }

/* locallogger daemon error handling */
extern int edg_wll_ll_log_level;
void edg_wll_ll_log_init(int level);
void edg_wll_ll_log(int level, const char *fmt, ...);


/* fcntl defaults */
#define FCNTL_ATTEMPTS		5
#define FCNTL_TIMEOUT		1

/* connect defaults */
#define CONNECT_ATTEMPTS	5

/* accept defaults */
#define ACCEPT_TIMEOUT		30

/* connection defaults */
#define CONNECTION_TIMEOUT	EDG_WLL_LOG_SYNC_TIMEOUT_MAX

/* locallogger daemon listen and connect functions prototypes */
int do_listen(int port);
int do_connect(char *hostname, int port);


#ifdef __cplusplus
}
#endif

#endif /* __EDG_WORKLOAD_LOGGING_LOCALLOGGER_LOGD_PROTO_H__ */