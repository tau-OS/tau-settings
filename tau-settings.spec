%define gnome_online_accounts_version 3.44.0
%define glib2_version 2.72.0
%define gnome_desktop_version 42.0
%define gsd_version 42.1
%define gsettings_desktop_schemas_version 42.0
%define upower_version 0.99.14
%define gtk4_version 4.6.2
%define gnome_bluetooth_version 42.0
%define libadwaita_version 1.1.0
%define nm_version 1.36.4

# it's like 11pm at night i don't care about adding debug packages
%global debug_package %{nil}
 
Summary:        Utilities to configure the GNOME desktop
Name:           tau-settings
Version:        1.1
Release:        1
License:        GPLv2+ and CC-BY-SA
URL:            https://tauos.co
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  chrpath
BuildRequires:  cups-devel
BuildRequires:  desktop-file-utils
BuildRequires:  docbook-style-xsl libxslt
BuildRequires:  gcc
BuildRequires:  gettext
BuildRequires:  meson
BuildRequires:  git
BuildRequires:  pkgconfig(accountsservice)
BuildRequires:  pkgconfig(clutter-gtk-1.0)
BuildRequires:  pkgconfig(colord)
BuildRequires:  pkgconfig(colord-gtk4)
BuildRequires:  pkgconfig(gcr-base-3)
BuildRequires:  pkgconfig(gdk-pixbuf-2.0)
BuildRequires:  pkgconfig(gdk-wayland-3.0)
BuildRequires:  pkgconfig(gio-2.0) >= %{glib2_version}
BuildRequires:  pkgconfig(gnome-desktop-4) >= %{gnome_desktop_version}
BuildRequires:  pkgconfig(gnome-settings-daemon) >= %{gsd_version}
BuildRequires:  pkgconfig(goa-1.0) >= %{gnome_online_accounts_version}
BuildRequires:  pkgconfig(goa-backend-1.0)
BuildRequires:  pkgconfig(gsettings-desktop-schemas) >= %{gsettings_desktop_schemas_version}
BuildRequires:  pkgconfig(gsound)
BuildRequires:  pkgconfig(gtk4) >= %{gtk4_version}
BuildRequires:  pkgconfig(gudev-1.0)
BuildRequires:  pkgconfig(ibus-1.0)
BuildRequires:  pkgconfig(libadwaita-1) >= %{libadwaita_version}
BuildRequires:  pkgconfig(libgtop-2.0)
BuildRequires:  pkgconfig(libnm) >= %{nm_version}
BuildRequires:  pkgconfig(libnma-gtk4)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(libpulse-mainloop-glib)
BuildRequires:  pkgconfig(libsecret-1)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(malcontent-0)
BuildRequires:  pkgconfig(mm-glib)
BuildRequires:  pkgconfig(polkit-gobject-1)
BuildRequires:  pkgconfig(pwquality)
BuildRequires:  pkgconfig(smbclient)
BuildRequires:  pkgconfig(upower-glib) >= %{upower_version}
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xi)
BuildRequires:  pkgconfig(udisks2)
BuildRequires:  pkgconfig(gnome-bluetooth-3.0) >= %{gnome_bluetooth_version}
BuildRequires:  pkgconfig(libwacom)
BuildArch: x86_64
 
# Versioned library deps
Requires: libadwaita%{?_isa} >= %{libadwaita_version}
Requires: glib2%{?_isa} >= %{glib2_version}
Requires: gnome-desktop4%{?_isa} >= %{gnome_desktop_version}
Requires: gnome-online-accounts%{?_isa} >= %{gnome_online_accounts_version}
Requires: gnome-settings-daemon%{?_isa} >= %{gsd_version}
Requires: gsettings-desktop-schemas%{?_isa} >= %{gsettings_desktop_schemas_version}
Requires: gtk4%{?_isa} >= %{gtk4_version}
Requires: upower%{?_isa} >= %{upower_version}
Requires: gnome-bluetooth%{?_isa} >= 1:%{gnome_bluetooth_version}
 
Requires: %{name}-filesystem = %{version}-%{release}
# For user accounts
Requires: accountsservice
Requires: alsa-lib
# For the thunderbolt panel
Recommends: bolt
# For the color panel
Requires: colord
# For the printers panel
Requires: cups-pk-helper
Requires: dbus
# For the info/details panel
Requires: glx-utils
# For the user languages
Requires: iso-codes
# For parental controls support
Requires: malcontent
Requires: malcontent-control
# For the network panel
Recommends: NetworkManager-wifi
Recommends: nm-connection-editor
# For Show Details in the color panel
Recommends: gnome-color-manager
# For the sharing panel
Recommends: gnome-remote-desktop
Recommends: rygel
# For the info/details panel
Recommends: switcheroo-control
# For the keyboard panel
Requires: /usr/bin/gkbd-keyboard-display
# For the power panel
Recommends: power-profiles-daemon
 
# Renamed in F28
Provides: control-center = 1:%{version}-%{release}
Provides: control-center%{?_isa} = 1:%{version}-%{release}
Obsoletes: control-center < 1:%{version}-%{release}
Provides: gnome-control-center > %{version}-%{release}
Obsoletes: gnome-control-center < %{version}-%{release}
 
%description
This package contains configuration utilities for the GNOME desktop, which
allow to configure accessibility options, desktop fonts, keyboard and mouse
properties, sound setup, desktop theme and background, user interface
properties, screen resolution, and other settings.

Features modifications for tauOS.
 
%package filesystem
Summary: GNOME Control Center directories
# NOTE: this is an "inverse dep" subpackage. It gets pulled in
# NOTE: by the main package and MUST not depend on the main package
BuildArch: noarch
# Renamed in F28
Provides: control-center-filesystem = 1:%{version}-%{release}
Obsoletes: control-center-filesystem < 1:%{version}-%{release}
 
%description filesystem
The GNOME control-center provides a number of extension points
for applications. This package contains directories where applications
can install configuration files that are picked up by the control-center
utilities.
 
%prep
%setup -q
 
%build
%meson \
  -Ddocumentation=true \
%if 0%{?fedora}
  -Ddistributor_logo=%{_datadir}/pixmaps/tau-banner.png \
  -Ddark_mode_distributor_logo=%{_datadir}/pixmaps/tau-banner.png \
  -Dmalcontent=true \
%endif
  %{nil}
%meson_build

%install
%meson_install
# We do want this
mkdir -p $RPM_BUILD_ROOT%{_datadir}/gnome/wm-properties
 
# We don't want these
rm -rf $RPM_BUILD_ROOT%{_datadir}/gnome/autostart
rm -rf $RPM_BUILD_ROOT%{_datadir}/gnome/cursor-fonts
 
# Remove rpath
chrpath --delete $RPM_BUILD_ROOT%{_bindir}/gnome-control-center
 
%find_lang %{name} --all-name --with-gnome
 
%files -f %{name}.lang
%license COPYING
%doc NEWS README.md
%{_bindir}/gnome-control-center
%{_datadir}/applications/*.desktop
%{_datadir}/bash-completion/completions/gnome-control-center
%{_datadir}/dbus-1/services/org.gnome.Settings.SearchProvider.service
%{_datadir}/dbus-1/services/org.gnome.Settings.service
%{_datadir}/gettext/
%{_datadir}/glib-2.0/schemas/org.gnome.Settings.gschema.xml
%{_datadir}/gnome-control-center/keybindings/*.xml
%{_datadir}/gnome-control-center/pixmaps
%{_datadir}/gnome-shell/search-providers/org.gnome.Settings.search-provider.ini
%{_datadir}/icons/gnome-logo-text*.svg
%{_datadir}/icons/hicolor/*/*/*
%{_datadir}/man/man1/gnome-control-center.1*
%{_metainfodir}/org.gnome.Settings.appdata.xml
%{_datadir}/pixmaps/faces
%{_datadir}/pkgconfig/gnome-keybindings.pc
%{_datadir}/polkit-1/actions/org.gnome.controlcenter.*.policy
%{_datadir}/polkit-1/rules.d/gnome-control-center.rules
%{_datadir}/sounds/gnome/default/*/*.ogg
%{_libexecdir}/cc-remote-login-helper
%{_libexecdir}/gnome-control-center-goa-helper
%{_libexecdir}/gnome-control-center-search-provider
%{_libexecdir}/gnome-control-center-print-renderer
 
%files filesystem
%dir %{_datadir}/gnome-control-center
%dir %{_datadir}/gnome-control-center/keybindings
%dir %{_datadir}/gnome/wm-properties
 
%changelog
* Sun Apr 3 2022 Jamie Murphy <jamie@fyralabs.com> - 1.1-1
- Initial Release