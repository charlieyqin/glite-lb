#!/usr/bin/perl

# version known to support enough from document/literal to work
use SOAP::Lite 0.69;

use Data::Dumper;

$ENV{HTTPS_CA_DIR}='/etc/grid-security/certificates';
$ENV{HTTPS_VERSION}='3';

$ENV{HTTPS_CA_FILE}= $ENV{HTTPS_CERT_FILE} = $ENV{HTTPS_KEY_FILE} =
	$ENV{X509_USER_PROXY} ? $ENV{X509_USER_PROXY} : "/tmp/x509up_u$<";


$srv = shift;
$when = shift;

die "usage: $0 service when\n" unless $when;

$c = SOAP::Lite
	-> uri('http://glite.org/wsdl/services/lb')
	-> proxy($srv) ;


# TODO: replace with $srv/lb/?wsdl once it works
service $c 'http://egee.cesnet.cz/en/WSDL/HEAD/LB.wsdl';

$c->serializer->register_ns('http://glite.org/wsdl/types/lb','lbt');

ns $c 'http://glite.org/wsdl/elements/lb';

$req = SOAP::Data->value(
		SOAP::Data->name(conditions => \SOAP::Data->value(
			SOAP::Data->name(attr => 'TIME')->type('lbt:queryAttr'),
			SOAP::Data->name(statName => 'SUBMITTED')->type('lbt:statName'),
			SOAP::Data->name(record=>\SOAP::Data->value(
				SOAP::Data->name(op => 'LESS')->type('lbt:queryOp'),
				SOAP::Data->name(value1 => \SOAP::Data->value(
					SOAP::Data->name(t => \SOAP::Data->value(
						SOAP::Data->name(tvSec => $when+1),
						SOAP::Data->name(tvUsec => 0),
					))
				))
			))
		)),
		SOAP::Data->name(conditions => \SOAP::Data->value(
			SOAP::Data->name(attr => 'EXITCODE')->type('lbt:queryAttr'),
			SOAP::Data->name(record=>\SOAP::Data->value(
				SOAP::Data->name(op => 'EQUAL')->type('lbt:queryOp'),
				SOAP::Data->name(value1 => \SOAP::Data->value(
					SOAP::Data->name(i => $when)
				))
			))
		)),
		SOAP::Data->name(flags => \SOAP::Data->value(
				SOAP::Data->name(flag => 'CLASSADS')->type('lbt:jobFlagsValue'),
				SOAP::Data->name(flag => 'CHILDSTAT')->type('lbt:jobFlagsValue')
		)),
	);

on_fault $c sub { print Dumper($_[1]->fault); $fault = 1; };

$resp = $c -> QueryJobs($req);

@res = $resp->paramsout;

print Dumper(@res),"\n";

