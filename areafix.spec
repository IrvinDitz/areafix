%define reldate 20190108
%define reltype C
# may be one of: C (current), R (release), S (stable)

Name: areafix
Version: 1.9.%{reldate}%{reltype}
Release: 1
Group: Libraries/FTN
Summary: FTN areafix library
URL: http://husky.sf.net
License: GPL
Requires: perl >= 5.8.8
BuildRequires: fidoconf >= 1.9
Source: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Areafix library for the Husky Project software.

%prep
%setup -q -n %{name}

%build
make

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} install
chmod -R a+rX,u+w,go-w %{buildroot}

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%{_includedir}/%{name}/*
%{_libdir}/*.a
