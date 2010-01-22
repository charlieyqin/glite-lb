#ifndef GLITE_LB_PERFTEST_H
#define GLITE_LB_PERFTEST_H 

#ident "$Header$"

#include "glite/jobid/cjobid.h"
#include "glite/jobid/strmd5.h"

#ifdef BUILDING_LB_COMMON
#include "events.h"
#else
#include "glite/lb/events.h"
#endif

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
			       glite_jobid_t *jobid);

#endif /* GLITE_LB_PERFTEST_H */
