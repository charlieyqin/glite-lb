#ifndef GLITE_LB_PROD_PROTO_H
#define GLITE_LB_PROD_PROTO_H

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


#ifdef __cplusplus
extern "C" {
#endif

#include "glite/security/glite_gss.h"
#include "glite/lb/log_proto.h"
#include "glite/lb/context-int.h"

#define PROXY_CONNECT_RETRY 10 /* ms */

/**
 * connect to local-logger
 * \param[in,out] ctx		context to work with
 * \param[out] conn		opened connection (index in the connection pool)
 * \return errno
 */
int edg_wll_log_connect(edg_wll_Context ctx, int *conn);

/**
 * close connection to local-logger
 * \param[in,out] ctx		context to work with
 * \param[in] conn		opened connection
 * \return errno
 */
int edg_wll_log_close(edg_wll_Context ctx, int conn);

/**
 * write/send to local-logger
 * \param[in,out] ctx		context to work with
 * \param[in] conn		connection to use
 * \param[in] logline		message to send
 * \return 	the number of bytes written (zero indicates nothing was written) or -1 on error
 */
int edg_wll_log_write(edg_wll_Context ctx, int conn, edg_wll_LogLine logline);

/**
 * read/receive answer (stored in context) from local-logger
 * \param[in,out] ctx		context to work with
 * \param[in] conn		connection to use
 * \return 	the number of bytes read (zero indicates nothing was read) or -1 on error
 */
int edg_wll_log_read(edg_wll_Context ctx, int conn);


/**
 * connect to lbproxy
 * \param[in,out] ctx		context to work with
 * \param[out] conn		opened connection
 * \return errno
 */
int edg_wll_log_proxy_connect(edg_wll_Context ctx, edg_wll_PlainConnection *conn);

/**
 * close connection to lbproxy
 * \param[in,out] ctx		context to work with
 * \param[out] conn		opened connection
 * \return errno
 */
int edg_wll_log_proxy_close(edg_wll_Context ctx, edg_wll_PlainConnection *conn);

/**
 * write/send to lbproxy
 * \param[in,out] ctx		context to work with
 * \param[in] conn		connection to use
 * \param[in] logline		message to send
 * \return 	the number of bytes written (zero indicates nothing was written) or -1 on error
 */
int edg_wll_log_proxy_write(edg_wll_Context ctx, edg_wll_PlainConnection *conn, edg_wll_LogLine logline);

/**
 * read/receive from lbproxy
 * \param[in,out] ctx		context to work with
 * \param[in] conn		connection to use
 * \return 	the number of bytes read (zero indicates nothing was read) or -1 on error
 */
int edg_wll_log_proxy_read(edg_wll_Context ctx, edg_wll_PlainConnection *conn);


/** 
 * connect to bkserver
 * \param[in,out] ctx		context to work with
 * \param[out] conn		opened connection
 * \return errno
 */
int edg_wll_log_direct_connect(edg_wll_Context ctx, edg_wll_GssConnection *conn);

/**
 * close connection to bkserver
 * \param[in,out] ctx		context to work with
 * \param[out] conn		opened connection
 * \return errno
 */
int edg_wll_log_direct_close(edg_wll_Context ctx, edg_wll_GssConnection *conn);

/**
 * write/send to bkserver
 * \param[in,out] ctx		context to work with
 * \param[in] conn		connection to use
 * \param[in] logline		message to send
 * \return 	the number of bytes written (zero indicates nothing was written) or -1 on error
 */
int edg_wll_log_direct_write(edg_wll_Context ctx, edg_wll_GssConnection *conn, edg_wll_LogLine logline);

/**
 * read/receive from bkserver
 * \param[in,out] ctx		context to work with
 * \param[in] conn		connection to use
 * \return 	the number of bytes read (zero indicates nothing was read) or -1 on error
 */
int edg_wll_log_direct_read(edg_wll_Context ctx, edg_wll_GssConnection *conn);

#ifdef __cplusplus
}
#endif

#endif /* GLITE_LB_PROD_PROTO_H */
