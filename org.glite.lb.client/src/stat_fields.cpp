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

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
//#include <classad.h>

#include "glite/lb/LoggingExceptions.h"
#include "JobStatus.h"
#include "notification.h"

#include "stat_fields.h"

using namespace glite::lb;

typedef std::pair<JobStatus::Attr,std::string> FieldPair;

char * glite_lb_parse_stat_fields(const char *arg,void **out)
{
	char	*aux = strdup(arg),*p;
	std::vector<FieldPair>	*fields = new std::vector<FieldPair>;

	for (p = strtok(aux,","); p; p = strtok(NULL,",")) {
		/*special treatment for JDL (and possibly other) two-valued fields with ':' used as a separator  */
		if (strncasecmp("jdl:", p, 4)) {
			try { fields->push_back(std::make_pair(JobStatus::attrByName(p), "")); }
			catch (std::runtime_error &e) { delete fields; return p; };
		}
		else {
			try { fields->push_back(std::make_pair(JobStatus::attrByName("jdl"), p + 4)); }
			catch (std::runtime_error &e) { delete fields; return p; };
		}
	}
	
	*out = (void *) fields;
	return NULL;
}


static std::string & escape(std::string &s)
{
	for (std::string::iterator p = s.begin(); p < s.end(); p++) switch (*p) {
		case '\n':
			s.insert(p-s.begin(),"\\"); *(++p) = 'n';
			break;
		case '\t':
			s.insert(p-s.begin(),"\\"); *(++p) = 't';
			break;
		default: break;
	}
	return s;
}

typedef std::vector<std::pair<JobStatus::Attr,JobStatus::AttrType> > attrs_t;

void glite_lb_dump_stat_fields(void)
{
	JobStatus	s;
	attrs_t 	a = s.getAttrs(); 
	for (attrs_t::iterator i=a.begin(); i != a.end(); i++) {
		switch (i->second) {
			case JobStatus::INT_T:
			case JobStatus::STRING_T:
			case JobStatus::TIMEVAL_T:
				std::cerr << JobStatus::getAttrName(i->first) << ", ";
			default: break;
		}
	}
}

extern "C" { char * TimeToStr(time_t); }

void glite_lb_print_stat_fields(void **ff,edg_wll_JobStat *s)
{
	std::vector<FieldPair>	*fields = (std::vector<FieldPair> *) ff;
	JobStatus	stat(*s,0);
	attrs_t 	attrs = stat.getAttrs();
	attrs_t::iterator a;
	std::vector<FieldPair>::iterator f;
	std::string val;
	struct timeval t;
	JobStatus::Attr attr;
	char *jdl_param = NULL,*jobid_s = NULL;

	std::cout << (jobid_s = glite_jobid_unparse(s->jobId)) << '\t' << stat.name() << '\t';
	free(jobid_s);

	for (f = fields->begin(); f != fields->end(); f++) {
		for (a = attrs.begin(); a != attrs.end() && a->first != f->first; a++);
		if (a != attrs.end() ) {
			attr = (a->first);
			switch (a->second) {
				case (JobStatus::INT_T):
					std::cout << stat.getValInt(attr) << '\t';
					break;
				case (JobStatus::STRING_T):
					if (attr != JobStatus::JDL) {
						val = stat.getValString(attr);
						std::cout << (val.empty() ? "(null)" : escape(val)) << '\t'; 
					}
					else {
                                                val = f->second;
						if ((jdl_param = edg_wll_JDLField(s, val.c_str()))) {
							std::string	s_param(jdl_param);

        	                                       	std::cout << escape(s_param); 
	                                                free(jdl_param); jdl_param = NULL;
						} else
        	                                       	std::cout << "(null)"; 
						std::cout << '\t';
					}
					break;
				case (JobStatus::TIMEVAL_T): 
					t = stat.getValTime(attr);
					std::cout << TimeToStr(t.tv_sec) << '\t'; 
					break;
				default:
					std::cout << "(unsupported)";
					break;
			}
		}
	}
	std::cout << std::endl;
}
