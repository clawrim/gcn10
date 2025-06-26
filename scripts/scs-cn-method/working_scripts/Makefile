ifeq ($(OS),Windows_NT)
    GDAL_CFLAGS=-I/c/OSGeo4W/include
    GDAL_LIBS=/c/OSGeo4W/lib/gdal_i.lib
    EXT=.exe
else
    GDAL_LIBS=`gdal-config --libs`
    GDAL_CFLAGS=`gdal-config --cflags`
endif

CFLAGS=-Wall -O3 -fopenmp $(GDAL_CFLAGS)
LDFLAGS=-O3 -fopenmp -lm

# Default target
all: scs_runoff$(EXT)

# Clean target
clean:
	$(RM) *.o scs_runoff$(EXT)

# Build the executable
scs_runoff$(EXT): main.o raster.o
	$(CC) $(LDFLAGS) -o $@ $^ $(GDAL_LIBS)

# Compile main.c
main.o: main.c header.h
	$(CC) $(CFLAGS) -c main.c

# Compile rasterio.c
rasterio.o: rasterio.c header.h
	$(CC) $(CFLAGS) -c raster.c

# Mark clean as a phony target
.PHONY: clean

