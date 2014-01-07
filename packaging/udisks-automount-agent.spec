%bcond_with ivi
Name:       udisks-automount-agent
Summary:    Udisk automount agent
Version:    0.1.0
Release:    1
Group:      Base/Device Management
License:    GPL-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    %{name}.manifest
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(udisks2)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: udisks-devel
BuildRequires: cmake
BuildRequires: linux-kernel-headers

Requires:      glib2
Requires:      udisks

%description
TIZEN udisks automount agent.


%if %{with ivi}
%define _target_name weston
%else
%define _target_name core-efl
%endif


%prep
%setup -q
cp -a %{SOURCE1} .
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DSYSTEMD_SERVICE_DIR=%{_unitdir_user} 


%build
make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}%{_unitdir_user}/%{_target_name}.target.wants/
ln -s ../udisks-automount-agent.service %{buildroot}%{_unitdir_user}/%{_target_name}.target.wants/udisks-automount-agent.service


%post


%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%{_prefix}/bin/udisks-automount-agent
%{_unitdir_user}/udisks-automount-agent.service
%{_unitdir_user}/%{_target_name}.target.wants/udisks-automount-agent.service
%{_sysconfdir}/polkit-1/localauthority/10-vendor.d/10-udisks.pkla
%license LICENSE
