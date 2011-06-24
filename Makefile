INCLUDE = ./include
PATHSRC = ./src
GCCFLAGS= -Wall -I -Wunused -ansi -pedantic -ggdb 
LINKERFLAGS=-lpthread -lm

all:  Appmobile.exe Appfixed.exe Monitor.exe LBmobile.exe LBfixed.exe

Appmobile.exe: Appmobile.o Util.o CheckPkt.o PktGen.o
	gcc -o Appmobile.exe ${GCCFLAGS} ${LINKERFLAGS} Appmobile.o Util.o CheckPkt.o PktGen.o

Appmobile.o: $(PATHSRC)/Appmobile.c $(INCLUDE)/Util.h $(INCLUDE)/VoIPpkt.h $(INCLUDE)/CheckPkt.h $(INCLUDE)/PktGen.h
	gcc -c ${GCCFLAGS} $(PATHSRC)/Appmobile.c

Appfixed.exe: Appfixed.o Util.o CheckPkt.o PktGen.o
	gcc -o Appfixed.exe ${GCCFLAGS} ${LINKERFLAGS} Appfixed.o Util.o CheckPkt.o PktGen.o

Appfixed.o: $(PATHSRC)/Appfixed.c $(INCLUDE)/Util.h $(INCLUDE)/VoIPpkt.h $(INCLUDE)/CheckPkt.h $(INCLUDE)/PktGen.h
	gcc -c ${GCCFLAGS} $(PATHSRC)/Appfixed.c

CheckPkt.o: $(PATHSRC)/CheckPkt.c $(INCLUDE)/VoIPpkt.h
	gcc -c ${GCCFLAGS} $(PATHSRC)/CheckPkt.c

PktGen.o: $(PATHSRC)/PktGen.c $(INCLUDE)/Util.h $(INCLUDE)/VoIPpkt.h
	gcc -c ${GCCFLAGS} $(PATHSRC)/PktGen.c

Monitor.o: $(PATHSRC)/Monitor.c $(INCLUDE)/Util.h $(INCLUDE)/VoIPpkt.h
	gcc -c ${GCCFLAGS} -pg $(PATHSRC)/Monitor.c

Monitor.exe: Monitor.o Util.o
	gcc -o Monitor.exe ${GCCFLAGS} ${LINKERFLAGS} -pg Util.o Monitor.o 

LBmobile.exe: LBmobile.o Util.o CheckPkt.o PktGen.o LBliste.o LB.o
	gcc -o LBmobile.exe ${GCCFLAGS} ${LINKERFLAGS} LBmobile.o Util.o CheckPkt.o PktGen.o LBliste.o LB.o

LBmobile.o: $(PATHSRC)/LBmobile.c $(INCLUDE)/Util.h $(INCLUDE)/VoIPpkt.h $(INCLUDE)/CheckPkt.h $(INCLUDE)/PktGen.h $(INCLUDE)/LBliste.h $(INCLUDE)/LB.h $(INCLUDE)/const.h
	gcc -c ${GCCFLAGS} $(PATHSRC)/LBmobile.c

LBfixed.exe: LBfixed.o Util.o CheckPkt.o PktGen.o LBliste.o LB.o
	gcc -o LBfixed.exe ${GCCFLAGS} ${LINKERFLAGS} LBfixed.o Util.o CheckPkt.o PktGen.o LBliste.o LB.o

LBfixed.o: $(PATHSRC)/LBfixed.c $(INCLUDE)/Util.h $(INCLUDE)/VoIPpkt.h $(INCLUDE)/CheckPkt.h $(INCLUDE)/PktGen.h $(INCLUDE)/LBliste.h $(INCLUDE)/LB.h $(INCLUDE)/const.h
	gcc -c ${GCCFLAGS} $(PATHSRC)/LBfixed.c

Util.o: $(PATHSRC)/Util.c $(INCLUDE)/Util.h
	gcc -c ${GCCFLAGS} $(PATHSRC)/Util.c

LBliste.o: $(PATHSRC)/LBliste.c $(INCLUDE)/LBliste.h $(INCLUDE)/LB.h
	gcc -c ${GCCFLAGS} $(PATHSRC)/LBliste.c

LB.o: $(PATHSRC)/LB.c
	gcc -c ${GCCFLAGS} $(PATHSRC)/LB.c

clean:	
	rm -f core* *.stackdump
	rm -f *.exe
	rm -f *.txt
	rm -f *.out
	rm -f *.o
	rm -rf ./doc

cleanall:	clean
	rm -f *~

doc:
	-doxygen

