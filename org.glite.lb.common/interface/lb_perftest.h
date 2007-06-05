#ifndef __GLITE_LB_PERFTEST_H__
#define __GLITE_LB_PERFTEST_H__ 

#ident "$Header$"

#include "glite/wmsutils/jobid/cjobid.h"
#include "glite/wmsutils/jobid/strmd5.h"
#include "events.h"

#define PERFTEST_END_TAG_NAME "lb_perftest"
#define PERFTEST_END_TAG_VALUE "+++ konec testu +++"

int
glite_wll_perftest_init(const char *host,         /** hostname */
			const char *user,         /** user running this test */
			const char *testname,     /** name of the test */
			const char *filename,     /** file with events for job source */
			int n);                   /** number of jobs for job source */

char *
glite_wll_perftest_produceJobId();

int
glite_wll_perftest_produceEventString(char **event, char **jobid);

int
glite_wll_perftest_consumeEvent(edg_wll_Event *event);

int
glite_wll_perftest_consumeEventString(const char *event_string);

int
glite_wll_perftest_consumeEventIlMsg(const char *msg);

int 
glite_wll_perftest_createJobId(const char *bkserver,
			       int port,
			       const char *test_user,
			       const char *test_name,
			       int job_num,
			       edg_wlc_JobId *jobid);

#endif /* __GLITE_LB_PERFTEST_H__ */
