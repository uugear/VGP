all: debpkg

debpkg: vgp vgpw
	cp vgp debpkg/usr/bin/vgp
	cp vgpw debpkg/usr/bin/vgpw
	chmod 755 debpkg/DEBIAN/postinst
	dpkg --build debpkg "vgp_arm64.deb"

vgp: vgp.c vgplib
	gcc -o vgp vgp.c vgplib.o -lgpiod -pthread

vgpw: vgpw.c vgplib style.css
	xxd -i style.css > style.h
	gcc -o vgpw vgpw.c vgplib.o -lgpiod -pthread `pkg-config --cflags --libs gtk+-3.0`

vgplib: vgplib.c
	gcc -c vgplib.c

clean:
	rm -f *.deb
	rm -f debpkg/usr/bin/vgp
	rm -f debpkg/usr/bin/vgpw
	rm -f vgp
	rm -f vgpw
	rm -f style.h
	rm -f vgplib.o
