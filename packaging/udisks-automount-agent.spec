%bcond_with ivi
# enable or disable notifications for mount/umount events
%define enable_notifications 0

Name:          udisks-automount-agent
Summary:       Udisk automount agent
Version:       0.1.0
Release:       1
Group:         Base/Device Management
License:       GPL-2.0
Source0:       %{name}-%{version}.tar.gz
Source1:       %{name}.manifest
Source1001:    %{name}.service
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
%define _target_name weston
%else
%define _target_name core-efl
%endif

%prep
%setup -q
cp -a %{SOURCE1} .
cp -a %{SOURCE1001} .
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DSYSTEMD_SERVICE_DIR=%{_unitdir_user} -DENABLE_NOTIF=%{enable_notifications}

%build
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}%{_libdir}/systemd/system
install -m0644 %{SOURCE1001} %{buildroot}%{_libdir}/systemd/system/

%post
systemctl daemon-reload
systemctl restart udisks-automount-agent.service 
systemctl enable udisks-automount-agent.service 

%preun
systemctl stop udisks-automount-agent.service
systemctl disable udisks-automount-agent.service

%postun
/sbin/ldconfig
systemctl daemon-reload

%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%{_prefix}/bin/udisks-automount-agent
%{_sysconfdir}/polkit-1/localauthority/10-vendor.d/10-udisks.pkla
%license LICENSE
%defattr(0644,root,root)
%{_libdir}/systemd/system/udisks-automount-agent.service

%files devel
%defattr(-,root,root,-)
%{_bindir}/sample-udisks-agent-notif-listener
%{_bindir}/sample-udisks-agent-mount-listener
