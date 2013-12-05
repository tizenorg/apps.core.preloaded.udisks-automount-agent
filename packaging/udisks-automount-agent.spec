%bcond_with ivi
Name:       udisks-automount-agent
Summary:    Udisk automount agent
Version:    0.1.0
Release:    1
Group:      Base/Device Management
License:    GPL-2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(udisks2)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: udisks-devel
BuildRequires: cmake
BuildRequires: linux-kernel-headers

Requires:      glib2
Requires:      udisks

%description
TIZEN udisks automount sample agent.  

%prep
%setup -q
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} 

%build
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
%if %{with ivi}
%install_service graphical.target.wants udisks-automount-agent.service
%else
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system/core-efl.target.wants/
ln -s ../udisks-automount-agent.service %{buildroot}/usr/lib/systemd/system/core-efl.target.wants/udisks-automount-agent.service
%endif

%post

%files
%{_prefix}/bin/udisks-automount-agent
%if %{with ivi}
%{_unitdir}/graphical.target.wants/udisks-automount-agent.service
%{_unitdir}/udisks-automount-agent.service
%else
%{_prefix}/lib/systemd/system/udisks-automount-agent.service
%{_prefix}/lib/systemd/system/core-efl.target.wants/udisks-automount-agent.service
%endif
%{_sysconfdir}/polkit-1/localauthority/10-vendor.d/10-udisks.pkla
%license LICENSE