Summary: collaborative screensaver
Name: electricsheep
Version: 2.5
Release: 1
Copyright: GPL
Group: Graphics
Source: http://electricsheep.org/electricsheep-2.5.tar.gz
Url: http://electricsheep.org
Packager: Scott Draves <rpm@electricsheep.org>
Requires: xloadimage, xscreensaver, expat, curl
%description

Electric Sheep is a screensaver that realizes the collective dream of
sleeping computers from all over the internet.  It's an xscreensaver
module that displays mpeg video of an animated fractal flame.  In the
background it contributes render cycles to the next animation.
Periodically it uploades completed frames to the server, where they
are compressed for distribution to all clients.

%prep
%setup
./configure

%build
make

%install
make install

%undefine	__check_files

%files
/usr/local/bin/electricsheep
/usr/local/bin/anim-flame
/usr/local/bin/mpeg2dec_onroot 
/usr/local/share/electricsheep-splash-0.tif
/usr/local/share/electricsheep-splash-1.tif
/usr/local/share/electricsheep-smile.tif
/usr/local/share/electricsheep-frown.tif
/usr/share/control-center/screensavers/electricsheep.xml
/usr/local/man/man1/electricsheep.1
