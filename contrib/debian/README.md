
Debian
====================
This directory contains files used to package nbxd/netboxwallet
for Debian-based Linux systems. If you compile nbxd/netboxwallet yourself, there are some useful files here.

## nbx: URI support ##


netboxwallet.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install netboxwallet.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your netboxwallet binary to `/usr/bin`
and the `../../share/pixmaps/nbx128.png` to `/usr/share/pixmaps`

netboxwallet.protocol (KDE)

