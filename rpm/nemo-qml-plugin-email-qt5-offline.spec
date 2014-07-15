Name:       nemo-qml-plugin-email-qt5-offline
Summary:    Offline email plugin for Nemo Mobile
Version:    0.1.41
Release:    1
Group:      System/Libraries
License:    BSD
URL:        https://github.com/nemomobile/nemo-qml-plugin-email
Source0:    %{name}-%{version}.tar.bz2
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(qmfclient5)
BuildRequires:  pkgconfig(qmfmessageserver5)
BuildRequires:  pkgconfig(mlocale5)
Conflicts: nemo-qml-plugin-email-qt5
Provides: nemo-qml-plugin-email-qt5

%description
%{summary}.

%package devel
Summary:    Nemo email plugin support for C++ applications
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary}.

%package tests
Summary:    QML email plugin tests
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description tests
%{summary}.

%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre
echo "DEFINES+=OFFLINE" > .qmake.conf
%qmake5 DEFINES+=OFFLINE

make %{?_smp_mflags}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%qmake5_install

# >> install post
# << install post

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libnemoemail-qt5.so.*
%{_libdir}/qt5/qml/org/nemomobile/email/libnemoemail.so
%{_libdir}/qt5/qml/org/nemomobile/email/qmldir
%{_sysconfdir}/xdg/nemo-qml-plugin-email/domainSettings.conf
%{_sysconfdir}/xdg/nemo-qml-plugin-email/serviceSettings.conf
# >> files
# << files

%files devel
%defattr(-,root,root,-)
%{_libdir}/libnemoemail-qt5.so
%{_libdir}/libnemoemail-qt5.prl
%{_includedir}/nemoemail-qt5/*.h
%{_libdir}/pkgconfig/nemoemail-qt5.pc
# >> files devel
# << files devel

%files tests
%defattr(-,root,root,-)
/opt/tests/nemo-qml-plugins/email/*
# >> files tests
# << files tests
