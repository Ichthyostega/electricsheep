all: anim-flame pick-flame
other: hqi cmap-anim pvm_anim
clean:
	rm -f gll hqi xflame anim cmap-anim pvm_anim libflame.a pick-flame anim-flame *.o
CFLAGS = -g -O
XLDFLAGS = -L/usr/X11R6/lib/

TCL = -I /usr/local/include

anim.o: anim.c libifs.h rect.h
libifs.o: libifs.c libifs.h
cmap.o: cmap.c

anim-flame: anim.o libifs.o cmap.o rect.o
	$(CC) -o anim-flame anim.o cmap.o libifs.o rect.o -lm

pick-flame: pick-flame.o libifs.o cmap.o rect.o
	$(CC) -o pick-flame pick-flame.o cmap.o libifs.o rect.o -lm

hqi: hqi.o libifs.o cmap.o rect.o
	$(CC) -o hqi hqi.o cmap.o libifs.o rect.o -lm

tohirez-flame: tohirez-flame.o libifs.o cmap.o rect.o 
	$(CC) -o tohirez-flame tohirez-flame.o cmap.o libifs.o rect.o -lm 
 
install:
	install anim-flame pick-flame /usr/local/bin
