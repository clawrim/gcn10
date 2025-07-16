CC = mpicc
CFLAGS = -Wall -O3 -fopenmp `gdal-config --cflags`
LIBS = `gdal-config --libs` -lm

all: scs_runoff_mpi

scs_runoff_mpi: main.o raster.o runoff.o
	$(CC) $(CFLAGS) -o scs_runoff_mpi main.o raster.o runoff.o $(LIBS)

main.o: main.c global.h
	$(CC) $(CFLAGS) -c main.c

raster.o: raster.c global.h
	$(CC) $(CFLAGS) -c raster.c

runoff.o: runoff.c global.h
	$(CC) $(CFLAGS) -c runoff.c

clean:
	rm -f *.o scs_runoff_mpi
