#ifndef GLITE_LB_SERVER_STATS_H
#define GLITE_LB_SERVER_STATS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include "glite/lb/context.h"

typedef struct _edg_wll_server_statistics{
	uint64_t value;
	time_t start;
} edg_wll_server_statistics;

// Add new values at the end of the enum (before SERVER_STATISTICS_COUNT)
// Also for new vales, add text descriptions to server_stats.c
typedef enum _edg_wll_server_statistics_type{
	SERVER_STATS_GLITEJOB_REGS = 0,
	SERVER_STATS_PBSJOB_REGS,
	SERVER_STATS_CONDOR_REGS,
	SERVER_STATS_CREAM_REGS,
	SERVER_STATS_SANDBOX_REGS,
	SERVER_STATS_JOB_EVENTS,
	SERVER_STATS_HTML_VIEWS,
	SERVER_STATS_TEXT_VIEWS,
	SERVER_STATS_RSS_VIEWS,
	SERVER_STATS_NOTIF_LEGACY_REGS,
	SERVER_STATS_NOTIF_MSG_REGS,
	SERVER_STATS_NOTIF_LEGACY_SENT,
	SERVER_STATS_NOTIF_MSG_SENT,
	SERVER_STATS_WS_QUERIES,
	SERVER_STATS_LBPROTO,
	SERVER_STATS_VM_REGS,
	SERVER_STATISTICS_COUNT
} edg_wll_server_statistics_type;

extern char     *edg_wll_server_statistics_type_title[];
extern char     *edg_wll_server_statistics_type_key[];


int edg_wll_InitServerStatistics(edg_wll_Context ctx, char *prefix);

int edg_wll_ServerStatisticsIncrement(edg_wll_Context ctx, edg_wll_server_statistics_type type);
int edg_wll_ServerStatisticsGetValue(edg_wll_Context ctx, edg_wll_server_statistics_type type);
time_t* edg_wll_ServerStatisticsGetStart(edg_wll_Context ctx, edg_wll_server_statistics_type type);
int edg_wll_ServerStatisticsInTmp();

#endif /* GLITE_LB_SERVER_STATS_H */
