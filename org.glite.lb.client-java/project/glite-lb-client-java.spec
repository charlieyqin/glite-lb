%global distver %(rpm -q --quiet redhat-release && rpm -q --queryformat "%{VERSION}" redhat-release || rpm -q --quiet centos-release && rpm -q --queryformat "%{VERSION}" centos-release || rpm -q --quiet sl-release && rpm -q --queryformat "%{VERSION}" sl-release | sed 's/^\\([0-9]*\\).*/\\1/')

Summary: @SUMMARY@
Name: glite-lb-client-java
Version: @MAJOR@.@MINOR@.@REVISION@
Release: @AGE@%{?dist}
Url: @URL@
License: ASL 2.0
Vendor: EMI
Group: System Environment/Libraries
BuildRequires: ant
BuildRequires: axis1.4
BuildRequires: chrpath
BuildRequires: emi-trustmanager
BuildRequires: emi-trustmanager-axis
BuildRequires: glite-jobid-api-java
BuildRequires: glite-lb-types
BuildRequires: glite-lb-ws-interface
BuildRequires: jakarta-commons-lang
%if 0%{?distver} >= 6
BuildRequires: java-1.6.0-openjdk-devel
%else
BuildRequires: java-devel
%endif
BuildRequires: libtool
Requires: emi-trustmanager-axis
Requires: emi-trustmanager
Requires: glite-jobid-api-java
Requires: jakarta-commons-lang
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
AutoReqProv: yes
Source: http://eticssoft.web.cern.ch/eticssoft/repository/emi/emi.lb.client-java/%{version}/src/%{name}-@VERSION@.src.tar.gz


%description
@DESCRIPTION@


%prep
%setup -q


%build
/usr/bin/perl ./configure --thrflavour= --nothrflavour= --root=/ --prefix=/usr --libdir=%{_lib} --project=emi --module lb.client-java --with-axis=/usr/local/axis1.4
make


%check
make check


%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
find $RPM_BUILD_ROOT -name '*.la' -exec rm -rf {} \;
find $RPM_BUILD_ROOT -name '*.a' -exec rm -rf {} \;
find $RPM_BUILD_ROOT -name '*' -print | xargs -I {} -i bash -c "chrpath -d {} > /dev/null 2>&1" || echo 'Stripped RPATH'


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
/usr/%{_lib}/libglite_lb_sendviasocket.so
/usr/%{_lib}/libglite_lb_sendviasocket.so.0
/usr/%{_lib}/libglite_lb_sendviasocket.so.0.0.0
/usr/share/java/lb-client-java.jar
/usr/share/java/lb-client-java-examples.jar


%changelog
* @SPEC_DATE@ @MAINTAINER@ - @MAJOR@.@MINOR@.@REVISION@-@AGE@%{?dist}
- automatically generated package
