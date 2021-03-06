#!/bin/sh
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

#
# an example script for getting LB dumps by rsync
# (it does not remove files on the LB server!)
#

GLITE_LOCATION=${GLITE_LOCATION:-/opt/glite}
GLITE_LOCATION_VAR=${GLITE_LOCATION_VAR:-${GLITE_LOCATION}/var}
GLITE_LB_DUMPDIR=${GLITE_LB_DUMPDIR:-${GLITE_LOCATION_VAR}/dump}

GLITE_LB_SERVER=scientific.civ.zcu.cz
GLITE_LB_EXPORT_DUMPDIR=/home/glite/LB/export

progname=`basename $0`

function syslog () {
        if [ ! -z "$*" ]; then
                echo `date +'%b %d %H:%M:%S'` `hostname` $progname: $*
        fi
}

if [ ! -d ${GLITE_LB_DUMPDIR} ]; then
        syslog "Creating directory ${GLITE_LB_DUMPDIR}"
        mkdir -vp ${GLITE_LB_DUMPDIR} || exit 1
fi

syslog "getting new dumps to ${GLITE_LB_DUMPDIR}"
rsync -rlptoDuv ${GLITE_LB_SERVER}:${GLITE_LB_EXPORT_DUMPDIR}/ ${GLITE_LB_DUMPDIR}/

syslog "done"
