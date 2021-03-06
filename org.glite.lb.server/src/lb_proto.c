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


#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <expat.h>

#include "glite/lb/context-int.h"
#include "glite/lb/mini_http.h"
#include "glite/lb/xml_conversions.h"
#include "glite/jobid/strmd5.h"
#include "glite/lbu/trio.h"
#include "glite/lbu/escape.h"
#include "glite/lbu/log.h"

#include "lb_proto.h"
#include "lb_text.h"
#include "lb_html.h"
#include "lb_rss.h"
#include "stats.h"
#include "server_stats.h"
#include "jobstat.h"
#include "get_events.h"
#include "purge.h"
#include "lb_xml_parse.h"
#include "lb_xml_parse_V21.h"
#include "db_supp.h"
#include "server_notification.h"
#include "authz_policy.h"
#include "lb_authz.h"


#define METHOD_GET      "GET "
#define METHOD_POST     "POST "

#define KEY_QUERY_JOBS		"/queryJobs "
#define KEY_QUERY_EVENTS	"/queryEvents "
#define KEY_PURGE_REQUEST	"/purgeRequest "
#define KEY_DUMP_REQUEST	"/dumpRequest "
#define KEY_LOAD_REQUEST	"/loadRequest "
#define KEY_INDEXED_ATTRS	"/indexedAttrs "
#define KEY_NOTIF_REQUEST	"/notifRequest "
#define KEY_QUERY_SEQUENCE_CODE	"/querySequenceCode "
#define KEY_STATS_REQUEST	"/statsRequest "
#define KEY_HTTP        	"HTTP/1.1"


#define KEY_ACCEPT      "Accept:"
#define KEY_APP         "application/x-dglb"
#define KEY_AGENT	"User-Agent"

#define WSDL_PATH "share/wsdl/glite-lb"

static const char* const response_headers_dglb[] = {
        "Cache-Control: no-cache",
        "Server: edg_wll_Server/" PROTO_VERSION "/" COMP_PROTO,
        "Content-Type: application/x-dglb",
        NULL
};

static const char* const response_headers_html[] = {
        "Cache-Control: no-cache",
        "Server: edg_wll_Server/" PROTO_VERSION "/" COMP_PROTO,
        "Content-Type: text/html",
        NULL
};

static const char* const response_headers_plain[] = {
        "Cache-Control: no-cache",
        "Server: edg_wll_Server/" PROTO_VERSION "/" COMP_PROTO,
        "Content-Type: text/plain",
        NULL
};

volatile sig_atomic_t purge_quit = 0;


const char *edg_wll_HTTPErrorMessage(int errCode)
{
	const char *msg;
	
	switch (errCode) {
		case HTTP_OK: msg = "OK"; break;
		case HTTP_ACCEPTED: msg = "Accepted"; break;
		case HTTP_BADREQ: msg = "Bad Request"; break;
		case HTTP_UNAUTH: msg = "Unauthorized"; break;
		case HTTP_NOTFOUND: msg = "Not Found"; break;
		case HTTP_NOTALLOWED: msg = "Method Not Allowed"; break;
		case HTTP_UNSUPPORTED: msg = "Unsupported Media Type"; break;
		case HTTP_NOTIMPL: msg = "Not Implemented"; break;
		case HTTP_INTERNAL: msg = "Internal Server Error"; break;
		case HTTP_UNAVAIL: msg = "Service Unavailable"; break;
		case HTTP_INVALID: msg = "Invalid Data"; break;
		case HTTP_GONE: msg = "Gone"; break;
		default: msg = "Unknown error"; break;
	}

	return msg;
}


/* returns non-zero if old style (V21) protocols incompatible */
static int is_protocol_incompatibleV21(char *user_agent)
{
	const char *comp_proto;
	char *version, *needle;
        double  v, c, my_v = strtod(PROTO_VERSION_V21, (char **) NULL), my_c;


	/* get version od the other side */
        if ((version = strstr(user_agent,"/")) == NULL) return(-1);
        else v = strtod(++version, &needle);

	/* sent the other side list of compatible protocols? */
	if ( needle[0] == '\0' ) return(-2);

	/* test compatibility if server newer*/
        if (my_v > v) {
                comp_proto=COMP_PROTO_V21;
                do {
                        my_c = strtod(comp_proto, &needle);
                        if (my_c == v) return(0);
                        comp_proto = needle + 1;
                } while (needle[0] != '\0');
                return(1);
        }

	/* test compatibility if server is older */
        else if (my_v < v) {
                do {
                        comp_proto = needle + 1;
                        c = strtod(comp_proto, &needle);
                        if (my_v == c) return(0);
                } while (needle[0] != '\0');
                return(1);
        }

	/* version match */
        return(0);
}


/* returns non-zero if protocols incompatible */
static int is_protocol_incompatible(char *user_agent)
{
	const char *comp_proto;
        char *version, *needle;
        double  v, c, my_v = strtod(PROTO_VERSION, (char **) NULL), my_c;


	/* get version od the other side */
        if ((version = strstr(user_agent,"/")) == NULL) return(-1);
        else v = strtod(++version, &needle);

	/* sent the other side list of compatible protocols? */
	if ( needle[0] == '\0' ) return(-2);

	/* test compatibility if server newer*/
        if (my_v > v) {
                comp_proto=COMP_PROTO;
                do {
                        my_c = strtod(comp_proto, &needle);
                        if (my_c == v) return(0);
                        comp_proto = needle + 1;
                } while (needle[0] != '\0');
                return(1);
        }

	/* test compatibility if server is older */
        else if (my_v < v) {
                do {
                        comp_proto = needle + 1;
                        c = strtod(comp_proto, &needle);
                        if (my_v == c) return(0);
                } while (needle[0] != '\0');
                return(1);
        }

	/* version match */
        return(0);
}


static int outputHTML(char **headers)
{
	int i;

	if (!headers) return 0;

	for (i=0; headers[i]; i++)
		if (!strncmp(headers[i], KEY_ACCEPT, sizeof(KEY_ACCEPT) - 1)) {
			if (strstr(headers[i],KEY_APP)) 
				return 0; 		/* message sent by edg_wll_Api */
			else 
				return 1;		/* message sent by other application */
		}
	return 1;

}


static int check_request_for_query(char *request, const char *query) {
	char *found = strstr(request, query);

	if (found && (!strcspn(found+strlen(query),"? \f\n\r\t\v"))) return 1;
	else return 0;
}

static int strip_request_of_queries(char *request) {
	int front, back;
	char *tail;

	front = strcspn(request, "?");
	if (front < strlen(request)) {
		back = strcspn(request+front, " \f\n\r\t\v"); //POSIX set of whitespaces
		tail = strdup(request+front+back);
		strcpy(request+front, tail);
		free(tail);
	}

	return 0;
}

static int getUserNotifications(edg_wll_Context ctx, char *user, char ***notifids, http_admin_option option, int adm_output){
        char *q = NULL;
	char *where_clause = NULL;
        glite_lbu_Statement notifs = NULL;
        char *notifc[1] = {NULL};

	if ((option != HTTP_ADMIN_OPTION_MY) && adm_output) {
		if (option == HTTP_ADMIN_OPTION_ALL) asprintf(&where_clause, "1");
		else if (option == HTTP_ADMIN_OPTION_FOREIGN) asprintf(&where_clause, "userid!='%s'", user);
	}
	if (!where_clause) asprintf(&where_clause, "userid='%s'", user);

        trio_asprintf(&q, "select notifid "
                "from notif_registrations "
		"where %s",
                where_clause);
	glite_common_log_msg(LOG_CATEGORY_LB_SERVER_DB, LOG_PRIORITY_DEBUG, q);
        if (edg_wll_ExecSQL(ctx, q, &notifs) < 0) goto err;
        free(q); q = NULL;

        int n = 0;
        *notifids = NULL;
        while(edg_wll_FetchRow(ctx, notifs, 1, NULL, notifc)){
                n++;
                *notifids = realloc(*notifids, n*sizeof(**notifids));
                (*notifids)[n-1] = strdup(notifc[0]);
		glite_common_log(LOG_CATEGORY_LB_SERVER, LOG_PRIORITY_DEBUG,
			"Notif %s found", notifc[0]);
        }
	if (n){
		*notifids = realloc(*notifids, (n+1)*sizeof(**notifids));
		(*notifids)[n] = NULL;
	}
        return n;

err:
        return 0;
}

static int getNotifInfo(edg_wll_Context ctx, char *notifId, notifInfo *ni){
	char *q = NULL;
        glite_lbu_Statement notif = NULL;
	char *notifc[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

	char *can_peername = NULL;
	int retval = HTTP_OK;

	trio_asprintf(&q, "select n.destination, n.valid, n.conditions, n.flags, u.cert_subj, j.jobid "
                "from notif_registrations as n, users as u, notif_jobs as j "
                "where (n.notifid='%s') AND (n.userid=u.userid) AND (n.notifid=j.notifid)", notifId);
	glite_common_log_msg(LOG_CATEGORY_LB_SERVER_DB, LOG_PRIORITY_DEBUG, q);
	if (edg_wll_ExecSQL(ctx, q, &notif) < 0) return HTTP_INTERNAL;
        free(q); q = NULL;

	can_peername = edg_wll_gss_normalize_subj(ctx->peerName, 0);

	if (edg_wll_FetchRow(ctx, notif, sizeof(notifc)/sizeof(notifc[0]), NULL, notifc)){

        	if (edg_wll_gss_equal_subj(can_peername, notifc[4]) || ctx->noAuth || edg_wll_amIroot(can_peername, ctx->fqans, &ctx->authz_policy)) {
			ni->notifid = strdup(notifId);
			ni->destination = notifc[0];
			ni->valid = notifc[1];
			ni->conditions_text = notifc[2];
			parseJobQueryRec(ctx, notifc[2], strlen(notifc[2]), &(ni->conditions));
			ni->flags = atoi(notifc[3]);
			ni->owner = notifc[4];
			ni->jobid = notifc[5];
		}
		else {
			retval = HTTP_UNAUTH;
				edg_wll_SetError(ctx, EPERM, "You are not authorized to view details for this Notification ID.");
		}
	}
	else {
		retval = HTTP_NOTFOUND;
		edg_wll_SetError(ctx, ENOENT, "Notification ID not known.");
	}

	free(can_peername);
	return  retval;
}

static void freeNotifInfo(notifInfo *ni){
	if (ni->notifid) free(ni->notifid);
	if (ni->destination) free(ni->destination);
	if (ni->valid) free(ni->valid);
	if (ni->conditions){
		edg_wll_QueryRec **p;
		int i;
		for (p = ni->conditions; *p; p++){
			for (i = 0; (*p)[i].attr; i++)
				edg_wll_QueryRecFree((*p)+i);
			free(*p);
		}
		free(ni->conditions);
	}
	if (ni->conditions_text) free(ni->conditions_text);
	if (ni->owner) free(ni->owner);
	if (ni->jobid) free(ni->jobid);
}

static int getJobsRSS(edg_wll_Context ctx, char *feedType, edg_wll_JobStat **statesOut, edg_wll_QueryRec **qconditions){
	edg_wlc_JobId *jobsOut;
        //edg_wll_JobStat *statesOut;
        edg_wll_QueryRec **conds;
	int i;
	char *fullrssquery = NULL;

	char *can_peername = edg_wll_gss_normalize_subj(ctx->peerName, 0);

	if (strcmp(feedType, "finished") == 0) { asprintf(&fullrssquery, "owner=%s&status=done|=aborted|=cancelled", can_peername); }
	else if ((strcmp(feedType, "runningVM") == 0)) { asprintf(&fullrssquery, "owner=%s&status=running&jobtype=virtual_machine", can_peername); }
	else if ((strcmp(feedType, "doneVM") == 0)) { asprintf(&fullrssquery, "owner=%s&status=done&donecode=ok&jobtype=virtual_machine", can_peername); }
	else if (strcmp(feedType, "running") == 0) { asprintf(&fullrssquery, "owner=%s&status=running", can_peername); }
	else if (strcmp(feedType, "aborted") == 0) { asprintf(&fullrssquery, "owner=%s&status=aborted", can_peername); }

	if (fullrssquery) {
		i = edg_wll_ParseQueryConditions(ctx, fullrssquery, &conds);
		free(fullrssquery);
		if (i) return i;
	}
	else {  
		if (!qconditions) {
			*statesOut = NULL;
			free(can_peername);
			edg_wll_SetError(ctx, EINVAL, "Invalid RSS feed specified");
			return EINVAL;
		}
		else {
			conds = qconditions;
		}
	}

	for ( i=0; conds[i]; i++ ); // Look up the trailing position in the array: the time condition will be added to it

	glite_common_log(LOG_CATEGORY_LB_SERVER_REQUEST, LOG_PRIORITY_DEBUG, "RSS: adding time condition (No. %d) to query. rssTime = %ld", i, (long)ctx->rssTime);

	conds = realloc(conds,(i+2)*sizeof(*conds)); // Extend array by 1
	
	conds[i] = malloc(2*sizeof(**conds));
	conds[i][0].attr = EDG_WLL_QUERY_ATTR_LASTUPDATETIME;
	conds[i][0].op = EDG_WLL_QUERY_OP_GREATER;
	conds[i][0].value.t.tv_sec = time(NULL) - ctx->rssTime;
	conds[i][0].value.t.tv_usec = 0;
	conds[i][1].attr = EDG_WLL_QUERY_ATTR_UNDEF;
	conds[i+1] = NULL;


	if (edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conds, 0, &jobsOut, statesOut)){
		*statesOut = NULL;
	}

	if (!qconditions) {
		for (i = 0; conds[i]; i++)
			free(conds[i]);
		free(conds);
	}
	free(can_peername);

	return 0;
}

static void hup_handler(int sig) {
	purge_quit = 1;
}

static const char *glite_location() {
	const char *location = NULL;
	struct stat info;

	if (!(location = getenv("GLITE_LB_LOCATION")))
	if (!(location = getenv("GLITE_LOCATION")))
	{
		if (stat("/opt/glite/" WSDL_PATH, &info) == 0 && S_ISDIR(info.st_mode))
			location = "/opt/glite";
		else 
			if (stat("/usr/" WSDL_PATH, &info) == 0 && S_ISDIR(info.st_mode))
				location = "/usr";
	}

	return location;
}

int edg_wll_ParseQueryConditions(edg_wll_Context ctx, const char *query, edg_wll_QueryRec ***conditions) {
	edg_wll_QueryRec  **conds = NULL;
	char *q = glite_lbu_UnescapeURL(query);
	char *vartok = NULL, *vartok2 = NULL, *cond, *attribute, *op, *operator, *value, *orvals, *errmsg = NULL;
	int len, i=0, j, attr, shift;
	edg_wll_ErrorCode err = 0;

	glite_common_log(LOG_CATEGORY_LB_SERVER_REQUEST, LOG_PRIORITY_DEBUG, "Query over HTML \"%s\"", q);

	for( cond = strtok_r(q, "&", &vartok); cond ; cond = strtok_r(NULL, "&", &vartok) ) {
		conds=(edg_wll_QueryRec**)realloc(conds, sizeof(edg_wll_QueryRec*) * (i+2));
		conds[i] = NULL;
		conds[i+1] = NULL;


		len = strcspn(cond, "=<>");
		if (len == strlen(cond)) {
			asprintf(&errmsg, "Missing operator in condition \"%s\"", cond);
			err = edg_wll_SetError(ctx, EINVAL, errmsg);
			goto err;
		}
		attribute=(char*)calloc((len+1),sizeof(char));
		strncpy(attribute, cond, len);
		orvals=cond+len;
		
		for (attr=1; attr<EDG_WLL_QUERY_ATTR__LAST && strcasecmp(attribute,edg_wll_QueryAttrNames[attr]); attr++);
		if (attr == EDG_WLL_QUERY_ATTR__LAST) {
			asprintf(&errmsg, "Unknown argument \"%s\" in query", attribute);
			err = edg_wll_SetError(ctx, EINVAL, errmsg);
			goto err;
		} // No else here. Don't worry, attribute will be assigned to the query structure later

		j=0;
		for( op = strtok_r(orvals, "|", &vartok2); op ; op = strtok_r(NULL, "|", &vartok2) ) {
			if (strlen(op) == 0) continue;
			conds[i]=(edg_wll_QueryRec*)realloc(conds[i], sizeof(edg_wll_QueryRec) * (j+2));
			conds[i][j].value.c = NULL;
			conds[i][j+1].attr = EDG_WLL_QUERY_ATTR_UNDEF;

			conds[i][j].attr = (edg_wll_QueryAttr)attr;

			len = strspn(op, "=<>");
			if (len == 0) {
				asprintf(&errmsg, "Missing operator before \"%s\"", op);
				err = edg_wll_SetError(ctx, EINVAL, errmsg);
				goto err;
			}
			operator = (char*)calloc((len+1),sizeof(char));
			strncpy(operator, op, len);
			value=op+len;
			if (strlen(value) == 0) {
				asprintf(&errmsg, "No value given for attribute \"%s\"", attribute);
				err = edg_wll_SetError(ctx, EINVAL, errmsg);
				goto err;
			}

			if (!strcmp(operator, "<")) conds[i][j].op = EDG_WLL_QUERY_OP_LESS;
			else if (!strcmp(operator, ">")) conds[i][j].op = EDG_WLL_QUERY_OP_GREATER;
			else if (!strcmp(operator, "=")) conds[i][j].op = EDG_WLL_QUERY_OP_EQUAL;
			else if ((!strcmp(operator, "<>"))||(!strcmp(operator, "><"))) conds[i][j].op = EDG_WLL_QUERY_OP_UNEQUAL;
			else {
				asprintf(&errmsg, "Unknown operator \"%s\" in query", operator);
				err = edg_wll_SetError(ctx, EINVAL, errmsg);
				goto err;
			}

			switch(attr) {
//				case EDG_WLL_QUERY_ATTR_LEVEL:
//				case EDG_WLL_QUERY_ATTR_EVENT_TYPE:
//				case EDG_WLL_QUERY_ATTR_CHKPT_TAG:
				case EDG_WLL_QUERY_ATTR_JOBID:
				case EDG_WLL_QUERY_ATTR_PARENT:
					if ( glite_jobid_parse(value, (glite_jobid_t *)&conds[i][j].value.j) ) {
						asprintf(&errmsg, "Cannot parse jobid \"%s\" in query", value);
						err = edg_wll_SetError(ctx, EINVAL, errmsg);
						goto err;
					}
					break;
				case EDG_WLL_QUERY_ATTR_OWNER:
				case EDG_WLL_QUERY_ATTR_LOCATION:
				case EDG_WLL_QUERY_ATTR_DESTINATION:
				case EDG_WLL_QUERY_ATTR_HOST:
				case EDG_WLL_QUERY_ATTR_INSTANCE:
				case EDG_WLL_QUERY_ATTR_USERTAG:
				case EDG_WLL_QUERY_ATTR_JDL_ATTR:
				case EDG_WLL_QUERY_ATTR_NETWORK_SERVER:
				case EDG_WLL_QUERY_ATTR_SOURCE:
					conds[i][j].value.c = strdup(value);
					break;
				case EDG_WLL_QUERY_ATTR_STATUS:
					if ( 0 > (conds[i][j].value.i = edg_wll_StringToStat(value))) {
						asprintf(&errmsg, "Unknown job state \"%s\" in query", value);
						err = edg_wll_SetError(ctx, EINVAL, errmsg);
						goto err;
					}
					break;
				case EDG_WLL_QUERY_ATTR_DONECODE:
				case EDG_WLL_QUERY_ATTR_EXITCODE:
				case EDG_WLL_QUERY_ATTR_RESUBMITTED:
					conds[i][j].value.i = strtol(value, NULL, 10);
					switch (errno) {
						case EINVAL: 
							asprintf(&errmsg, "Cannot parse numeric value \"%s\" for attribute %s in query", value, attribute);
							err = edg_wll_SetError(ctx, EINVAL, errmsg);
	                                                goto err;
						case ERANGE: 
							asprintf(&errmsg, "Numeric value \"%s\" for attribute %s in query exceeds range", value, attribute);
							err = edg_wll_SetError(ctx, EINVAL, errmsg);
	                                                goto err;
					}
					break;
				case EDG_WLL_QUERY_ATTR_TIME:
				case EDG_WLL_QUERY_ATTR_STATEENTERTIME:
				case EDG_WLL_QUERY_ATTR_LASTUPDATETIME:
					if ((!strncmp("last",value,4)) || (!strncmp("past",value,4))) shift = 4;
					conds[i][j].value.t.tv_sec = strtol(value+shift, NULL, 10);
					switch (errno) {
						case EINVAL: 
							asprintf(&errmsg, "Cannot parse numeric value \"%s\" for attribute %s in query", value, attribute);
							err = edg_wll_SetError(ctx, EINVAL, errmsg);
	                                                goto err;
						case ERANGE: 
							asprintf(&errmsg, "Numeric value \"%s\" for attribute %s in query exceeds range", value, attribute);
							err = edg_wll_SetError(ctx, EINVAL, errmsg);
	                                                goto err;
					}
					if (shift) {
						struct timeval tvnow;
						gettimeofday(&tvnow,NULL);
						conds[i][j].value.t.tv_sec = tvnow.tv_sec - conds[i][j].value.t.tv_sec;
					}
					conds[i][j].value.t.tv_usec = 0;
					break;
				case EDG_WLL_QUERY_ATTR_JOB_TYPE:
					if ( 0 > (conds[i][j].value.i = edg_wll_JobtypeStrToCode(value))) {
						asprintf(&errmsg, "Unknown job type \"%s\" in query", value);
						err = edg_wll_SetError(ctx, EINVAL, errmsg);
						goto err;
					}
					break;
				case EDG_WLL_QUERY_ATTR_VM_STATUS:
					if ( 0 > (conds[i][j].value.i = edg_wll_StringToVMStat(value))) {
                                                asprintf(&errmsg, "Unknown VM job state \"%s\" in query", value);
                                                err = edg_wll_SetError(ctx, EINVAL, errmsg);
                                                goto err;
                                        }
					break;
				default:
					asprintf(&errmsg, "Value conversion for attribute \"%s\" not supported in current implementation", attribute);
					err = edg_wll_SetError(ctx, ENOSYS, errmsg);
					goto err;
					break;
			}
			free(operator);
			j++;
		}

		free(attribute);	
		i++;
	}

err:
	if (err) {
		if (conds) {
			for (j = 0; conds[j]; j++) {
				for (i = 0 ; (conds[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
					edg_wll_QueryRecFree(&conds[j][i]);
				free(conds[j]);
			}
			free(conds);
		}
	}
	else *conditions=conds;

	free(q);
	free(errmsg);
	return err;
}

edg_wll_ErrorCode edg_wll_ProtoV21(edg_wll_Context ctx,
	char *request,char **headers,char *messageBody,
	char **response,char ***headersOut,char **bodyOut)
{
	char *requestPTR, *message = NULL;
	int	ret = HTTP_OK;
	int 	html = outputHTML(headers);
	int	i;

	edg_wll_ResetError(ctx);

	for (i=0; headers[i]; i++) /* find line with version number in headers */
		if ( strstr(headers[i], KEY_AGENT) ) break;
  
	if (headers[i] == NULL) { ret = HTTP_BADREQ; goto errV21; } /* if not present */
	switch (is_protocol_incompatibleV21(headers[i])) { 
		case 0  : /* protocols compatible */
			  ctx->is_V21 = 1;
			  break;
		case -1 : /* malformed 'User Agent:' line */
			  ret = HTTP_BADREQ;
			  goto errV21;
			  break;
		case -2 : /* version of one protocol unknown */
			  /* fallthrough */
		case 1  : /* protocols incompatible */
			  /* fallthrough */
		default : ret = HTTP_UNSUPPORTED; 
			  edg_wll_SetError(ctx,ENOTSUP,"Protocol versions are incompatible.");
			  goto errV21; 
			  break;
	}


/* GET */
	if (!strncmp(request, METHOD_GET, sizeof(METHOD_GET)-1)) {
		// Not supported
		ret = HTTP_BADREQ;
/* POST */
	} else if (!strncmp(request,METHOD_POST,sizeof(METHOD_POST)-1)) {

		requestPTR = request + sizeof(METHOD_POST)-1;
	
		if (!strncmp(requestPTR,KEY_QUERY_EVENTS,sizeof(KEY_QUERY_EVENTS)-1)) { 
        	        edg_wll_Event *eventsOut = NULL;
			edg_wll_QueryRec **job_conditions = NULL, **event_conditions = NULL;
			int i,j;

        	        if (parseQueryEventsRequestV21(ctx, messageBody, &job_conditions, &event_conditions)) 
				ret = HTTP_BADREQ;
				
	                else {
				int	fatal = 0;

				switch (edg_wll_QueryEventsServer(ctx,ctx->noAuth,
				    (const edg_wll_QueryRec **)job_conditions, 
				    (const edg_wll_QueryRec **)event_conditions, &eventsOut)) {

					case 0: if (html) ret = HTTP_NOTIMPL;
						else      ret = HTTP_OK; 
						break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EIDRM: ret = HTTP_GONE; break;
					case EPERM : ret = HTTP_UNAUTH; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_INTERNAL; break;
				}
				/* glue errors (if eny) to XML responce */ 
				if (!html && !fatal)
					if (edg_wll_QueryEventsToXMLV21(ctx, eventsOut, &message))
						ret = HTTP_INTERNAL;
			}

			if (job_conditions) {
	                        for (j = 0; job_conditions[j]; j++) {
	                                for (i = 0 ; (job_conditions[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
        	                                edg_wll_QueryRecFree(&job_conditions[j][i]);
                	                free(job_conditions[j]);
                        	}
	                        free(job_conditions);
        	        }

	                if (event_conditions) {
        	                for (j = 0; event_conditions[j]; j++) {
                	                for (i = 0 ; (event_conditions[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
                        	                edg_wll_QueryRecFree(&event_conditions[j][i]);
                                	free(event_conditions[j]);
	                        }
        	                free(event_conditions);
        	        }
	
			if (eventsOut != NULL) {
	                	for (i=0; eventsOut[i].type != EDG_WLL_EVENT_UNDEF; i++) 
					edg_wll_FreeEvent(&(eventsOut[i]));
	                	edg_wll_FreeEvent(&(eventsOut[i])); /* free last line */
				free(eventsOut);
			}
        	}
		else if (!strncmp(requestPTR,KEY_QUERY_JOBS,sizeof(KEY_QUERY_JOBS)-1)) { 
        	        edg_wlc_JobId *jobsOut = NULL;
			edg_wll_JobStat *statesOut = NULL;
			edg_wll_QueryRec **conditions = NULL;
			int i,j, flags = 0;

        	        if (parseQueryJobsRequestV21(ctx, messageBody, &conditions, &flags))
				ret = HTTP_BADREQ;

	                else { 
				int		fatal = 0,
						retCode;
 
				if (flags & EDG_WLL_STAT_NO_JOBS) { 
					flags -= EDG_WLL_STAT_NO_JOBS;
					jobsOut = NULL;
					if (flags & EDG_WLL_STAT_NO_STATES) {
						flags -= EDG_WLL_STAT_NO_STATES;
						statesOut = NULL;
						retCode = edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conditions, flags, NULL, NULL);
					}
					else
						retCode = edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conditions, flags, NULL, &statesOut);
				}
				else {
					if (flags & EDG_WLL_STAT_NO_STATES) {
						flags -= EDG_WLL_STAT_NO_STATES;
						statesOut = NULL;
						retCode = edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conditions, flags, &jobsOut, NULL);
					}
					else
						retCode = edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conditions, flags, &jobsOut, &statesOut);
				}
				
				switch ( retCode ) {
					// case EPERM : ret = HTTP_UNAUTH;
					//              /* soft-error fall through */
					case 0: if (html) ret =  HTTP_NOTIMPL;
						else ret = HTTP_OK;

						break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EPERM: ret = HTTP_UNAUTH; break;
					case EIDRM: ret = HTTP_GONE; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_INTERNAL; break;
				}
				if (!html && !fatal)
					if (edg_wll_QueryJobsToXMLV21(ctx, jobsOut, statesOut, &message))
						ret = HTTP_INTERNAL;
			}

	                if (conditions) {
        	                for (j = 0; conditions[j]; j++) {
                	                for (i = 0; (conditions[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
                        	                edg_wll_QueryRecFree(&conditions[j][i]);
                                	free(conditions[j]);
	                        }
        	                free(conditions);
                	}

			if (jobsOut) {
				for (i=0; jobsOut[i]; i++) edg_wlc_JobIdFree(jobsOut[i]);
				free(jobsOut);
			}
			if (statesOut) {
	                	for (i=0; statesOut[i].state != EDG_WLL_JOB_UNDEF; i++) 
					edg_wll_FreeStatus(&(statesOut[i]));
	                	edg_wll_FreeStatus(&(statesOut[i])); /* free last line */
				free(statesOut);
			}
        	}
        /* POST [something else]: not understood */
		else ret = HTTP_BADREQ;

/* other HTTP methods */
	} else ret = HTTP_NOTALLOWED;

errV21:	asprintf(response,"HTTP/1.1 %d %s",ret,edg_wll_HTTPErrorMessage(ret));
	*headersOut = (char **) (html? response_headers_html : response_headers_dglb);
	if ((ret != HTTP_OK) && html)
		*bodyOut = edg_wll_ErrorToHTML(ctx,ret);
	else
		*bodyOut = message;

	return edg_wll_Error(ctx,NULL,NULL);
}



edg_wll_ErrorCode edg_wll_Proto(edg_wll_Context ctx,
	char *request,char **headers,char *messageBody,
	char **response,char ***headersOut,char **bodyOut,
	int *httpErr)
{
	char *requestPTR = NULL, *message = NULL, *requestMeat = NULL, *querystr, *queryconds = NULL, *flagstr, *queryflags = NULL, *feedType = NULL;
	int	ret = HTTP_OK;
	int 	html = outputHTML(headers);
	int 	text = 0; //XXX: Plain text communication is special case of html here, hence when text=1, html=1 too
	int	i, j, rflags = 0;
	int	isadm;
	http_admin_option adm_opt = HTTP_ADMIN_OPTION_MY;
	http_extra_option extra_opt = HTTP_EXTRA_OPTION_NONE;
	edg_wll_QueryRec **job_conditions = NULL;

	edg_wll_ResetError(ctx);

	for (i=0; headers[i]; i++) /* find line with version number in headers */
		if ( strstr(headers[i], KEY_AGENT) ) break;
  
	if (headers[i] == NULL) { ret = HTTP_BADREQ; goto err; } /* if not present */
	if (!html) {
		switch (is_protocol_incompatible(headers[i])) { 
			case 0  : /* protocols compatible */
				  ctx->is_V21 = 0;
				  break;
			case -1 : /* malformed 'User Agent:' line */
				  ret = HTTP_BADREQ;
				  goto err;
				  break;
			case 1  : /* protocols incompatible */
				  /* try old (V21) version compatibility */
				  edg_wll_ProtoV21(ctx, request, headers, messageBody, 
						  response, headersOut, bodyOut);
						  
				  /* and propagate errors or results */
				  return edg_wll_Error(ctx,NULL,NULL);
				  break;
			case -2 : /* version of one protocol unknown */
				  /* fallthrough */
			default : ret = HTTP_UNSUPPORTED; 
				  edg_wll_SetError(ctx,ENOTSUP,"Protocol versions are incompatible.");
				  goto err; 
				  break;
		}
	}


/* GET */
	if (!strncmp(request, METHOD_GET, sizeof(METHOD_GET)-1)) {
		
		requestPTR = strdup(request + sizeof(METHOD_GET)-1);
		
		if (html) {
			text = check_request_for_query(requestPTR, "?text");

			if (check_request_for_query(requestPTR, "?wsdl")) extra_opt = HTTP_EXTRA_OPTION_WSDL;
                        else if (check_request_for_query(requestPTR, "?types")) extra_opt = HTTP_EXTRA_OPTION_TYPES;
                        else if (check_request_for_query(requestPTR, "?agu")) extra_opt = HTTP_EXTRA_OPTION_AGU;
                        else if (check_request_for_query(requestPTR, "?version")) extra_opt = HTTP_EXTRA_OPTION_VERSION;
                        else if (check_request_for_query(requestPTR, "?configuration")) extra_opt = HTTP_EXTRA_OPTION_CONFIGURATION;
                        else if (check_request_for_query(requestPTR, "?stats")) extra_opt = HTTP_EXTRA_OPTION_STATS;

			if (check_request_for_query(requestPTR, "?all")) adm_opt = HTTP_ADMIN_OPTION_ALL;
			else if (check_request_for_query(requestPTR, "?foreign")) adm_opt = HTTP_ADMIN_OPTION_FOREIGN;

			if ((querystr = strstr(requestPTR, "?query="))) {
				int len = strcspn(querystr+strlen("?query="),"? \f\n\r\t\v");
				if (len) {
					extra_opt = HTTP_EXTRA_OPTION_QUERY;
					queryconds = (char*)calloc((len+1),sizeof(char));
					queryconds = strncpy(queryconds, querystr+strlen("?query="), len);

					switch(edg_wll_ParseQueryConditions(ctx, queryconds, &job_conditions)) {
						case 0:
							break;
						case EINVAL:
							ret = HTTP_INVALID; goto err;
						case ENOSYS:
							ret = HTTP_NOTIMPL; goto err;
						default:
							ret = HTTP_BADREQ; goto err;
					}
				}
			}

                        if ((flagstr = strstr(requestPTR, "?flags="))) {
                                int len = strcspn(flagstr+strlen("?flags="),"? \f\n\r\t\v");
                                if (len) {
                                        queryflags = (char*)calloc((len+1),sizeof(char));
                                        queryflags = strncpy(queryflags, flagstr+strlen("?flags="), len);
					glite_common_log(LOG_CATEGORY_LB_SERVER_REQUEST, LOG_PRIORITY_DEBUG, "HTML query flags: \"%s\"", queryflags);
					rflags = edg_wll_string_to_stat_flags(queryflags);
					free(queryflags);
                                }
                        }

			strip_request_of_queries(requestPTR);
		}

		requestMeat = strdup(requestPTR);
		requestMeat[strcspn(requestPTR, " \f\n\r\t\v")] = '\0';

	/* Is user authorised? */
		if (!ctx->peerName){
			ret = HTTP_UNAUTH;
			edg_wll_SetError(ctx, EPERM, "user not authenticated");
		}

	/* GET /: Current User Jobs */
		else if ((!strcmp(requestMeat, "/")) && ((extra_opt == HTTP_EXTRA_OPTION_NONE) || ( extra_opt == HTTP_EXTRA_OPTION_QUERY))) {
                	edg_wlc_JobId *jobsOut = NULL;
			edg_wll_JobStat *statesOut = NULL;
			int	i;

			switch ( extra_opt == HTTP_EXTRA_OPTION_QUERY ?
                                edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)job_conditions, rflags, &jobsOut, &statesOut) :
                                edg_wll_UserJobsServer(ctx, EDG_WLL_STAT_CHILDREN, &jobsOut, &statesOut)) {

				case 0: edg_wll_UserInfoToHTML(ctx, jobsOut, statesOut, &message, text);
					edg_wll_ServerStatisticsIncrement(ctx, text ? SERVER_STATS_TEXT_VIEWS : SERVER_STATS_HTML_VIEWS);
					break;
				case ENOENT: ret = HTTP_NOTFOUND; break;
				case EPERM: ret = HTTP_UNAUTH; break;
				case EIDRM: ret = HTTP_GONE; break;
				case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAVAIL; break;
				default: ret = HTTP_INTERNAL; break;
			}
			if (!html && (ret != HTTP_INTERNAL)) 
				if (edg_wll_UserJobsToXML(ctx, jobsOut, &message))
					ret = HTTP_INTERNAL;

			if (jobsOut) {
				for (i=0; jobsOut[i]; i++) edg_wlc_JobIdFree(jobsOut[i]);
				free(jobsOut);
			}
			if (statesOut) {
				for (i=0; statesOut[i].state != EDG_WLL_JOB_UNDEF; i++)
					edg_wll_FreeStatus(&statesOut[i]);
				free(statesOut);
			}
	        } 

	/* GET /[jobId]: Job Status */
		else if (strncmp(requestMeat, "/RSS:", 5)
			&& strcmp(requestMeat, "/NOTIF")
			&& strncmp(requestMeat, "/NOTIF:", 7)
			&& strncmp(requestMeat, "/VMHOST:", 8)
			&& strcmp(requestMeat, "/favicon.ico")
			&& extra_opt == HTTP_EXTRA_OPTION_NONE
			) {
			edg_wlc_JobId jobId = NULL;
			char *pom1,*fullid = NULL;
			edg_wll_JobStat stat;
			char *pomCopy;

			if (ctx->srvName == NULL) {
				edg_wll_SetError(ctx, EDG_WLL_ERROR_SERVER_RESPONSE,
							"no server name on GET /jobid");
				ret = HTTP_INTERNAL;
				goto err;
			}
			memset(&stat,0,sizeof(stat));
			pomCopy = strdup(requestPTR + 1);
			for (pom1=pomCopy; *pom1 && !isspace(*pom1); pom1++);
			*pom1 = 0;

			asprintf(&fullid,GLITE_JOBID_PROTO_PREFIX"%s:%u/%s",ctx->srvName,ctx->srvPort,pomCopy);
			free(pomCopy);	

			if (edg_wlc_JobIdParse(fullid, &jobId)) {
				edg_wll_SetError(ctx,EDG_WLL_ERROR_JOBID_FORMAT,fullid);
				ret = HTTP_BADREQ;
			}
			else switch (edg_wll_JobStatusServer(ctx,jobId,EDG_WLL_STAT_CLASSADS | EDG_WLL_STAT_CHILDREN | rflags, &stat)) {
				case 0:
					edg_wll_GeneralJobStatusToHTML(ctx,stat,&message,text);
					edg_wll_ServerStatisticsIncrement(ctx, text ? SERVER_STATS_TEXT_VIEWS : SERVER_STATS_HTML_VIEWS);
					ret = HTTP_OK; break;
				case ENOENT: ret = HTTP_NOTFOUND; break;
				case EINVAL: ret = HTTP_INVALID; break;
				case EPERM : ret = HTTP_UNAUTH; break;
				case EIDRM : ret = HTTP_GONE; break;
				default: ret = HTTP_INTERNAL; break;
			}
			if (!html && (ret != HTTP_INTERNAL))
				if (edg_wll_JobStatusToXML(ctx,stat,&message))
					ret = HTTP_INTERNAL;

			free(fullid);
			edg_wlc_JobIdFree(jobId);
			edg_wll_FreeStatus(&stat);
	/*GET /NOTIF: All user's notifications*/
		} else if (!strcmp(requestMeat, "/NOTIF") || !strcmp(requestMeat, "/NOTIF:")) {
			char **notifids = NULL;
			char *can_peername = edg_wll_gss_normalize_subj(ctx->peerName, 0);
                        char *userid = strmd5(can_peername, NULL);

			isadm = ctx->noAuth || edg_wll_amIroot(ctx->peerName, ctx->fqans,&ctx->authz_policy);

			getUserNotifications(ctx, userid, &notifids, adm_opt, isadm);
			free(can_peername);
			if (text) {
	                        edg_wll_UserNotifsToText(ctx, notifids, &message);
				edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_TEXT_VIEWS);
			}
                        else if (html) {
	                        edg_wll_UserNotifsToHTML(ctx, notifids, &message, adm_opt, isadm);
				edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_HTML_VIEWS);
			}
                        else ret = HTTP_OK;

	/*GET /NOTIF:[notifId]: Notification info*/
		} else if (strncmp(requestPTR, "/NOTIF:", strlen("/NOTIF:")) == 0){
			notifInfo ni;
			char *pomCopy, *pom;
			pomCopy = strdup(requestPTR + 1);
                        for (pom=pomCopy; *pom && !isspace(*pom); pom++);
                        *pom = 0;
			
			ret = getNotifInfo(ctx, strrchr(pomCopy, ':')+1, &ni);
			if (ret != HTTP_OK) goto err;
			
			free(pomCopy);

			edg_wll_NotificationToHTML(ctx, &ni, &message, text);

			edg_wll_ServerStatisticsIncrement(ctx, text ? SERVER_STATS_TEXT_VIEWS : SERVER_STATS_HTML_VIEWS);

			freeNotifInfo(&ni);

	/*GET /RSS:[feed type] RSS feed */
		} else if (strncmp(requestPTR, "/RSS:", strlen("/RSS:")) == 0){
			edg_wll_JobStat *states;
			int i;
			int idx;

			feedType = strdup(requestPTR + strlen("/RSS:"));
			feedType[strrchr(feedType, ' ')-feedType] = 0;
			switch(getJobsRSS(ctx, feedType, &states, (edg_wll_QueryRec **)job_conditions)) {
				case 0:
					break;
				case EINVAL:
					ret = HTTP_INVALID;
                                	goto err;
				case ENOSYS:
					ret = HTTP_NOTIMPL;
                                	goto err;
				default:
					return HTTP_BADREQ;
                                	goto err;
			}

			// check if owner and lastupdatetime is indexed
			idx = 0;
			if (ctx->job_index) for (i = 0; ctx->job_index[i]; i++)
				if (ctx->job_index[i]->attr == EDG_WLL_QUERY_ATTR_OWNER)
					idx++;
				else if (ctx->job_index[i]->attr == EDG_WLL_QUERY_ATTR_LASTUPDATETIME)
					idx++;
			if (idx < 2){
				ret = HTTP_NOTFOUND;
	                        edg_wll_SetError(ctx, ENOENT, "current index configuration does not support RSS feeds");
			}
			else { 
				edg_wll_RSSFeed(ctx, states, requestPTR, &message);
				edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_RSS_VIEWS);
			}
		} else if (!strcmp(requestMeat, "/favicon.ico")) {
			message=NULL;
			ret = HTTP_NOTFOUND;
	/*GET /?wsdl */
#define WSDL_LB "LB.wsdl"
#define WSDL_LBTYPES "LBTypes.wsdl"
#define WSDL_LB4AGU "lb4agu.wsdl"
		} else if (extra_opt == HTTP_EXTRA_OPTION_WSDL) {
			edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_TEXT_VIEWS);
			char *filename;
			asprintf(&filename, "%s/" WSDL_PATH "/%s", glite_location(), WSDL_LB);
			if (edg_wll_WSDLOutput(ctx, &message, filename))
				ret = HTTP_INTERNAL;
			free(filename);
	/* GET /?types */
		} else if (extra_opt == HTTP_EXTRA_OPTION_TYPES) {
			edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_TEXT_VIEWS);
                        char *filename;
                        asprintf(&filename, "%s/" WSDL_PATH "/%s", glite_location(), WSDL_LBTYPES);
                        if (edg_wll_WSDLOutput(ctx, &message, filename))
                                ret = HTTP_INTERNAL;
			free(filename);
	/* GET /?agu */
                } else if (extra_opt == HTTP_EXTRA_OPTION_AGU) {
			edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_TEXT_VIEWS);
                        char *filename;
                        asprintf(&filename, "%s/" WSDL_PATH "/%s", glite_location(), WSDL_LB4AGU);
                        if (edg_wll_WSDLOutput(ctx, &message, filename))
                                ret = HTTP_INTERNAL;
			free(filename);
	/* GET /?version */
		} else if (extra_opt == HTTP_EXTRA_OPTION_VERSION) {
			edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_TEXT_VIEWS);
			asprintf(&message, "%s", VERSION);
	/* GET /?configuration*/
		} else if (extra_opt == HTTP_EXTRA_OPTION_CONFIGURATION) {
			// also browser-readable HTML version here?
			isadm = ctx->noAuth || edg_wll_amIroot(ctx->peerName, ctx->fqans,&ctx->authz_policy);
			edg_wll_ConfigurationToHTML(ctx, isadm, &message, text);
			edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_TEXT_VIEWS);
	/* GET /?stats*/
		} else if (extra_opt == HTTP_EXTRA_OPTION_STATS) {
			isadm = ctx->noAuth || edg_wll_amIroot(ctx->peerName, ctx->fqans,&ctx->authz_policy);
			if ( !isadm && ctx->count_server_stats == 2 ) {
				ret = HTTP_UNAUTH;
				edg_wll_SetError(ctx, EPERM, "Only superusers can view server usage statistics on this server.");
			}
			else {
				edg_wll_StatisticsToHTML(ctx, &message, text);
				edg_wll_ServerStatisticsIncrement(ctx, text ? SERVER_STATS_TEXT_VIEWS : SERVER_STATS_HTML_VIEWS);
			}
	/* GET [something else]: not understood */
		} else ret = HTTP_BADREQ;
		free(requestPTR); requestPTR = NULL;

/* POST */
	} else if (!strncmp(request,METHOD_POST,sizeof(METHOD_POST)-1)) {

		requestPTR = strdup(request + sizeof(METHOD_POST)-1);
		if (html) text = check_request_for_query(requestPTR, "?text");
		strip_request_of_queries(requestPTR);
	
		if (!strncmp(requestPTR,KEY_QUERY_EVENTS,sizeof(KEY_QUERY_EVENTS)-1)) { 
        	        edg_wll_Event *eventsOut = NULL;
			edg_wll_QueryRec **event_conditions = NULL;
			int i,j;

        	        if (parseQueryEventsRequest(ctx, messageBody, &job_conditions, &event_conditions)) 
				ret = HTTP_BADREQ;
				
	                else {
				int	fatal = 0;

				switch (edg_wll_QueryEventsServer(ctx,ctx->noAuth | check_authz_policy_ctx(ctx, READ_ALL),
				    (const edg_wll_QueryRec **)job_conditions, 
				    (const edg_wll_QueryRec **)event_conditions, &eventsOut)) {

					case 0: if (html) ret = HTTP_NOTIMPL;
						else      ret = HTTP_OK; 
						break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EIDRM: ret = HTTP_GONE; break;
					case EPERM : ret = HTTP_UNAUTH; break;
					case E2BIG: ret = HTTP_UNAUTH; break;
					case EINVAL: ret = HTTP_UNAUTH; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_INTERNAL; break;
				}
				/* glue errors (if eny) to XML responce */ 
				if (!html && !fatal)
					if (edg_wll_QueryEventsToXML(ctx, eventsOut, &message))
						ret = HTTP_INTERNAL;
			}

	                if (event_conditions) {
        	                for (j = 0; event_conditions[j]; j++) {
                	                for (i = 0 ; (event_conditions[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
                        	                edg_wll_QueryRecFree(&event_conditions[j][i]);
                                	free(event_conditions[j]);
	                        }
        	                free(event_conditions);
        	        }
	
			if (eventsOut != NULL) {
	                	for (i=0; eventsOut[i].type != EDG_WLL_EVENT_UNDEF; i++) 
					edg_wll_FreeEvent(&(eventsOut[i]));
	                	edg_wll_FreeEvent(&(eventsOut[i])); /* free last line */
				free(eventsOut);
			}
        	}
		else if (!strncmp(requestPTR,KEY_QUERY_JOBS,sizeof(KEY_QUERY_JOBS)-1)) { 
        	        edg_wlc_JobId *jobsOut = NULL;
			edg_wll_JobStat *statesOut = NULL;
			edg_wll_QueryRec **conditions = NULL;
			int i,j, flags = 0;

        	        if (parseQueryJobsRequest(ctx, messageBody, &conditions, &flags))
				ret = HTTP_BADREQ;

	                else { 
				int		fatal = 0,
						retCode;
 
				if (flags & EDG_WLL_STAT_NO_JOBS) { 
					jobsOut = NULL;
					if (flags & EDG_WLL_STAT_NO_STATES) {
						statesOut = NULL;
						retCode = edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conditions, flags, NULL, NULL);
					}
					else
						retCode = edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conditions, flags, NULL, &statesOut);
				}
				else {
					if (flags & EDG_WLL_STAT_NO_STATES) {
						statesOut = NULL;
						retCode = edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conditions, flags, &jobsOut, NULL);
					}
					else
						retCode = edg_wll_QueryJobsServer(ctx, (const edg_wll_QueryRec **)conditions, flags, &jobsOut, &statesOut);
				}
				
				switch ( retCode ) {
					case 0: if (html) ret =  HTTP_NOTIMPL;
						else ret = HTTP_OK;

						break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EPERM: ret = HTTP_UNAUTH; break;
					case E2BIG: ret = HTTP_UNAUTH; break;
					case EINVAL: ret = HTTP_UNAUTH; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					case EIDRM: ret = HTTP_GONE; break;
					default: ret = HTTP_INTERNAL; break;
				}
				if (!html && !fatal)
					if (edg_wll_QueryJobsToXML(ctx, jobsOut, statesOut, &message))
						ret = HTTP_INTERNAL;
			}

	                if (conditions) {
        	                for (j = 0; conditions[j]; j++) {
                	                for (i = 0; (conditions[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
                        	                edg_wll_QueryRecFree(&conditions[j][i]);
                                	free(conditions[j]);
	                        }
        	                free(conditions);
                	}

			if (jobsOut) {
				for (i=0; jobsOut[i]; i++) edg_wlc_JobIdFree(jobsOut[i]);
				free(jobsOut);
			}
			if (statesOut) {
	                	for (i=0; statesOut[i].state != EDG_WLL_JOB_UNDEF; i++) 
					edg_wll_FreeStatus(&(statesOut[i]));
	                	edg_wll_FreeStatus(&(statesOut[i])); /* free last line */
				free(statesOut);
			}
        	}
		else if (!strncmp(requestPTR,KEY_PURGE_REQUEST,sizeof(KEY_PURGE_REQUEST)-1)) {
			edg_wll_PurgeRequest    request;
			edg_wll_PurgeResult	result;
			struct sigaction	sa;
			sigset_t		sset;
			int			fatal = 0, retval;

			ctx->p_tmp_timeout.tv_sec = 86400;  

			memset(&request,0,sizeof(request));
			memset(&result,0,sizeof(result));

			if (parsePurgeRequest(ctx,messageBody,(int (*)()) edg_wll_StringToStat,&request))
				ret = HTTP_BADREQ;
			else {
				/* do throttled purge on background if requested */
				if ((request.flags & EDG_WLL_PURGE_BACKGROUND)) {
					retval = fork();

					switch (retval) {
					case 0: /* forked cleaner */
						memset(&sa, 0, sizeof(sa));
						sa.sa_handler = hup_handler;
						sigaction(SIGTERM, &sa, NULL);
						sigaction(SIGINT, &sa, NULL);

						sigemptyset(&sset);
						sigaddset(&sset, SIGTERM);
						sigaddset(&sset, SIGINT);
						sigprocmask(SIG_UNBLOCK, &sset, NULL);
						sigemptyset(&sset);
						sigaddset(&sset, SIGCHLD);
						sigprocmask(SIG_BLOCK, &sset, NULL);
						break;
					case -1: /* err */
						ret = HTTP_INTERNAL;
						edg_wll_SetError(ctx, errno, "can't fork purge process");
						goto err;
					default: /* client request handler */
						ret = HTTP_ACCEPTED;
						/* to end this parent */
						edg_wll_SetError(ctx, EDG_WLL_ERROR_ACCEPTED_OK, edg_wll_HTTPErrorMessage(ret));
						goto err;
					}
				}

				switch ( edg_wll_PurgeServer(ctx, (const edg_wll_PurgeRequest *)&request, &result)) {
					case 0: if (html) ret =  HTTP_NOTIMPL;
						else ret = HTTP_OK;

						break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EPERM: ret = HTTP_UNAUTH; break;
					case EINVAL: ret = HTTP_INVALID; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_INTERNAL; break;
				}
				if (!html && !fatal) {
					if (edg_wll_PurgeResultToXML(ctx, &result, &message))
						ret = HTTP_INTERNAL;
					else
						glite_common_log_msg(LOG_CATEGORY_CONTROL, LOG_PRIORITY_DEBUG, message);
				}				

				/* result is now packed in message, free it */	
				if ( result.server_file )
					free(result.server_file);
				if ( result.jobs )
				{
					for ( i = 0; result.jobs[i]; i++ )
						free(result.jobs[i]);
					free(result.jobs);
				}

				/* forked cleaner sends no results */
				if ((request.flags & EDG_WLL_PURGE_BACKGROUND)) {
					char *et, *ed;

					free(message);
					message = NULL;
					if (ret != HTTP_OK && ret != HTTP_ACCEPTED) {
						edg_wll_Error(ctx, &et, &ed);
						glite_common_log(LOG_CATEGORY_CONTROL, LOG_PRIORITY_ERROR, "Background purge failed, %s (%s)",et, ed);
						free(et);
						free(ed);
					} else {
						glite_common_log(LOG_CATEGORY_CONTROL, LOG_PRIORITY_INFO, "Background purge done successfully.");
					}
					*response = NULL;
					if (requestPTR) free(requestPTR);
					if (requestMeat) free(requestMeat);
					exit(0);
				}

			}

			if ( request.jobs )
			{
				int i;
				for ( i = 0; request.jobs[i]; i++ )
					free(request.jobs[i]);
				free(request.jobs);
			}

		}
		else if (!strncmp(requestPTR,KEY_DUMP_REQUEST,sizeof(KEY_DUMP_REQUEST)-1)) {
			edg_wll_DumpRequest	request;
			edg_wll_DumpResult	result;
		
			ctx->p_tmp_timeout.tv_sec = 86400;  

			memset(&request,0,sizeof(request));	
			memset(&result,0,sizeof(result));
			
        	        if (parseDumpRequest(ctx, messageBody, &request))
				ret = HTTP_BADREQ;
			else {
				int     fatal = 0;
				
				switch (edg_wll_DumpEventsServer(ctx,(const edg_wll_DumpRequest *) &request, &result)) {
					case 0: if (html) ret = HTTP_NOTIMPL;
						else      ret = HTTP_OK; 
						break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EPERM : ret = HTTP_UNAUTH; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_INTERNAL; break;
				}
				/* glue errors (if eny) to XML responce */ 
				if (!html && !fatal)
					if (edg_wll_DumpResultToXML(ctx, &result, &message))
						ret = HTTP_INTERNAL;
			}

			free(result.server_file);
		}
		else if (!strncmp(requestPTR,KEY_LOAD_REQUEST,sizeof(KEY_LOAD_REQUEST)-1)) {
			edg_wll_LoadRequest	request;
			edg_wll_LoadResult	result;
		
			ctx->p_tmp_timeout.tv_sec = 86400;  

			memset(&request,0,sizeof(request));	
			memset(&result,0,sizeof(result));
			
        	        if (parseLoadRequest(ctx, messageBody, &request))
				ret = HTTP_BADREQ;
			else {
				int     fatal = 0;
				
				switch (edg_wll_LoadEventsServer(ctx,(const edg_wll_LoadRequest *) &request, &result)) {
					case 0: if (html) ret = HTTP_NOTIMPL;
						else      ret = HTTP_OK; 
						break;
					case EEXIST: ret = HTTP_OK; break;
					case EINVAL: ret = HTTP_INVALID; break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EPERM : ret = HTTP_UNAUTH; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_INTERNAL; break;
				}
				/* glue errors (if eny) to XML responce */ 
				if (!html && !fatal)
					if (edg_wll_LoadResultToXML(ctx, &result, &message))
						ret = HTTP_INTERNAL;
			}

			free(result.server_file);
		}
		else if (!strncmp(requestPTR,KEY_INDEXED_ATTRS,sizeof(KEY_INDEXED_ATTRS)-1)) {
			if (!html)
				if (edg_wll_IndexedAttrsToXML(ctx, &message))
					ret = HTTP_INTERNAL;
		}
		else if (!strncmp(requestPTR,KEY_NOTIF_REQUEST,sizeof(KEY_NOTIF_REQUEST)-1)) {
			char *function, *address;
			edg_wll_NotifId	notifId;
			edg_wll_NotifChangeOp op;
			edg_wll_QueryRec **conditions;
			int flags;
			time_t validity = -1;
			int i,j;
			
			
        	        if (parseNotifRequest(ctx, messageBody, &function, &notifId, 
						&address, &op, &validity, &conditions, &flags))
				ret = HTTP_BADREQ;
			else {
				int     fatal = 0, err = 0;
				
				// XXX presne poradi parametru zatim neni znamo
				// navratove chyby nejsou zname, nutno predelat dle aktualni situace
				if (!strcmp(function,"New")) 
					err = edg_wll_NotifNewServer(ctx,
								(edg_wll_QueryRec const * const *)conditions, flags,
								address, notifId, &validity);
				else if (!strcmp(function,"Bind"))
					err = edg_wll_NotifBindServer(ctx, notifId, address, &validity);
				else if (!strcmp(function,"Change"))
					err = edg_wll_NotifChangeServer(ctx, notifId,
								(edg_wll_QueryRec const * const *)conditions, op);
				else if (!strcmp(function,"Refresh"))
					err = edg_wll_NotifRefreshServer(ctx, notifId, &validity);
				else if (!strcmp(function,"Drop"))
					err = edg_wll_NotifDropServer(ctx, notifId);
				
				switch (err) {
					case 0: if (html) ret = HTTP_NOTIMPL;
						else      ret = HTTP_OK; 
						break;
					case EEXIST: ret = HTTP_OK; break;
					case EINVAL: ret = HTTP_INVALID; break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EPERM : ret = HTTP_UNAUTH; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_UNAVAIL; break;
				}
				/* glue errors (if eny) to XML responce */ 
				if (!html && !fatal)
					if (edg_wll_NotifResultToXML(ctx, validity, &message))
						ret = HTTP_INTERNAL;
			}
			
			free(function);
			free(address);
			edg_wll_NotifIdFree(notifId);
	                if (conditions) {
        	                for (j = 0; conditions[j]; j++) {
                	                for (i = 0; (conditions[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
                        	                edg_wll_QueryRecFree(&conditions[j][i]);
                                	free(conditions[j]);
	                        }
        	                free(conditions);
                	}
		}
		else if (!strncmp(requestPTR,KEY_QUERY_SEQUENCE_CODE,sizeof(KEY_QUERY_SEQUENCE_CODE)-1)) {
			char		*source = NULL;
			char		*seqCode = NULL;
			edg_wlc_JobId	jobId = NULL;
			

			if (parseQuerySequenceCodeRequest(ctx, messageBody, &jobId, &source))
				ret = HTTP_BADREQ;
			else {
				int     fatal = 0;
				
				switch (edg_wll_QuerySequenceCodeServer(ctx, jobId, source, &seqCode)) {
					case 0: if (html) ret = HTTP_NOTIMPL;
						else      ret = HTTP_OK; 
						break;
					case EEXIST: ret = HTTP_OK; break;
					case EINVAL: ret = HTTP_INVALID; break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EPERM : ret = HTTP_UNAUTH; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_INTERNAL; break;
				}
				
				/* glue errors (if eny) to XML responce */ 
				if (!html && !fatal)
					if (edg_wll_QuerySequenceCodeResultToXML(ctx, seqCode, &message))
						ret = HTTP_INTERNAL;
			}

			if ( source ) free(source);
			if ( seqCode ) free(seqCode);
			edg_wlc_JobIdFree(jobId);
		}
		else if (!strncmp(requestPTR,KEY_STATS_REQUEST,sizeof(KEY_STATS_REQUEST)-1)) {
			char *function;
			edg_wll_QueryRec **conditions;
			edg_wll_JobStatCode base = EDG_WLL_JOB_UNDEF;
			edg_wll_JobStatCode final = EDG_WLL_JOB_UNDEF;
			time_t from, to;
			int i, j, minor, res_from = 0, res_to = 0;
			float *rates = NULL, *durations = NULL , *dispersions = NULL;
			char **groups = NULL;
			
        	        if (parseStatsRequest(ctx, messageBody, &function, &conditions, 
						&base, &final, &minor, &from, &to))
				ret = HTTP_BADREQ;
			else {
				int     fatal = 0, err = 0;
				
				// XXX presne poradi parametru zatim neni znamo
				// navratove chyby nejsou zname, nutno predelat dle aktualni situace
				if (!strcmp(function,"Rate")) 
					err = edg_wll_StateRateServer(ctx,
						conditions[0], base, minor, 
						&from, &to, &rates, &groups, 
						&res_from, &res_to); 
				else if (!strcmp(function,"Duration"))
					err = edg_wll_StateDurationServer(ctx,
						conditions[0], base, minor, 
						&from, &to, &durations, &groups,
						&res_from, &res_to); 
				else if (!strcmp(function, "DurationFromTo"))
					err = edg_wll_StateDurationFromToServer(
						ctx, conditions[0], base, final,
                                                minor, &from, &to, &durations,
                                                &dispersions, &groups, 
						&res_from, &res_to);
				
				switch (err) {
					case 0: if (html) ret = HTTP_NOTIMPL;
						else      ret = HTTP_OK; 
						break;
					case ENOSYS: ret = HTTP_NOTIMPL; break;
					case EEXIST: ret = HTTP_OK; break;
					case EINVAL: ret = HTTP_INVALID; break;
					case ENOENT: ret = HTTP_NOTFOUND; break;
					case EPERM : ret = HTTP_UNAUTH; break;
					case EDG_WLL_ERROR_NOINDEX: ret = HTTP_UNAUTH; break;
					case ENOMEM: fatal = 1; ret = HTTP_INTERNAL; break;
					default: ret = HTTP_INTERNAL; break;
				}
				/* glue errors (if eny) to XML responce */ 
				if (!html && !fatal)
					if (edg_wll_StatsResultToXML(ctx, from,
						to, rates, durations, 
						dispersions, groups,
						res_from, res_to, &message))
						ret = HTTP_INTERNAL;
				if (rates) free(rates);
				if (durations) free(durations);
				if (dispersions) free(dispersions);
				if (groups){
					for(i = 0; groups[i]; i++)
						free(groups[i]);
					free(groups);
				}
			}
			
			free(function);
	                if (conditions) {
        	                for (j = 0; conditions[j]; j++) {
                	                for (i = 0; (conditions[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
                        	                edg_wll_QueryRecFree(&conditions[j][i]);
                                	free(conditions[j]);
	                        }
        	                free(conditions);
                	}
		}

			
        /* POST [something else]: not understood */
		else ret = HTTP_BADREQ;

		if (ret != HTTP_BADREQ)
			edg_wll_ServerStatisticsIncrement(ctx, SERVER_STATS_LBPROTO);

		free(requestPTR); requestPTR = NULL;
		free(requestMeat); requestMeat = NULL;

/* other HTTP methods */
	} else ret = HTTP_NOTALLOWED;

err:	asprintf(response,"HTTP/1.1 %d %s",ret,edg_wll_HTTPErrorMessage(ret));
	*headersOut = (char **) (html ? (text ? response_headers_plain : response_headers_html) : response_headers_dglb);
	if ((ret != HTTP_OK && ret != HTTP_ACCEPTED) && text)
                *bodyOut = edg_wll_ErrorToText(ctx,ret);
	else if ((ret != HTTP_OK && ret != HTTP_ACCEPTED) && html)
		*bodyOut = edg_wll_ErrorToHTML(ctx,ret);
	else
		*bodyOut = message;

	if (job_conditions) {
		for (j = 0; job_conditions[j]; j++) {
			for (i = 0 ; (job_conditions[j][i].attr != EDG_WLL_QUERY_ATTR_UNDEF); i++ )
				edg_wll_QueryRecFree(&job_conditions[j][i]);
			free(job_conditions[j]);
		}
		free(job_conditions);
	}

	free(queryconds);
	free(feedType);

	if (requestPTR) free(requestPTR);
	if (requestMeat) free(requestMeat);

	*httpErr = ret;

	return edg_wll_Error(ctx,NULL,NULL);
}

