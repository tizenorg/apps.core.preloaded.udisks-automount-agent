%bcond_with ivi
# enable or disable notifications for mount/umount events
%define enable_notifications 0

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

BuildRequires: pkgconfig(notification)
BuildRequires: pkgconfig(eina)
BuildRequires: pkgconfig(ecore)
BuildRequires: bundle-devel


Requires:      glib2
Requires:      udisks

%description
TIZEN udisks automount agent.


%package devel
Summary:    Development files for udisks-automount-agent
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}


%description devel
%{summary}.


%if %{with ivi}
%define _target_name default
%else
%define _target_name core-efl
%endif


%prep
%setup -q
cp -a %{SOURCE1} .



%build
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DSYSTEMD_SERVICE_DIR=%{_unitdir_user} -DENABLE_NOTIF=%{enable_notifications}
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
%{_sysconfdir}/polkit-1/rules.d/10-udisks.rules
%license LICENSE


%files devel
%defattr(-,root,root,-)
%{_bindir}/sample-udisks-agent-notif-listener
%{_bindir}/sample-udisks-agent-mount-listener

