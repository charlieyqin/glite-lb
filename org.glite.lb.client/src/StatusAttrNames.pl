#
# Copyright (c) Members of the EGEE Collaboration. 2004-2010.
# See http://www.eu-egee.org/partners for details on the copyright holders.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# The order of strings in this array determines assigned numeric value in the JobStatus::Attr enum.
# It must not be changed unless API major number is incremented

@main::StatusAttrNames = qw/
	ACL
	CANCEL_REASON
	CANCELLING
	CE_NODE
	CHILDREN
	CHILDREN_HIST
	CHILDREN_NUM
	CHILDREN_STATES
	CONDOR_ID
	CONDOR_DEST_HOST
	CONDOR_ERROR_DESC
	CONDOR_JDL
	CONDOR_JOB_EXIT_STATUS
	CONDOR_JOB_PID
	CONDOR_OWNER
	CONDOR_PREEMPTING
	CONDOR_REASON
	CONDOR_SHADOW_EXIT_STATUS
	CONDOR_SHADOW_PID
	CONDOR_STARTER_EXIT_STATUS
	CONDOR_STARTER_PID
	CONDOR_STATUS
	CONDOR_UNIVERSE
	CPU_TIME
	DESTINATION
	DONE_CODE
	EXIT_CODE
	EXPECT_FROM
	EXPECT_UPDATE
	FAILURE_REASONS
	GLOBUS_ID
	JDL
	JOB_ID
	JOBTYPE
	LAST_UPDATE_TIME
	LOCAL_ID
	LOCATION
	MATCHED_JDL
	NETWORK_SERVER
	OWNER
	PARENT_JOB
	PAYLOAD_RUNNING
	PBS_DEST_HOST
	PBS_ERROR_DESC
	PBS_EXIT_STATUS
	PBS_NAME
	PBS_OWNER
	PBS_PID
	PBS_QUEUE
	PBS_REASON
	PBS_RESOURCE_USAGE
	PBS_SCHEDULER
	PBS_STATE
	POSSIBLE_CE_NODES
	POSSIBLE_DESTINATIONS
	REASON
	RESUBMITTED
	RSL
	SEED
	STATE_ENTER_TIME
	STATE_ENTER_TIMES
	SUBJOB_FAILED
	SUSPEND_REASON
	SUSPENDED
	USER_TAGS
	REMOVE_FROM_PROXY
	UI_HOST
	USER_FQANS
	SANDBOX_RETRIEVED
	JW_STATUS
	JDL_CLASSAD
/;