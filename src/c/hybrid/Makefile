ifeq ($(OS),Windows_NT)
    CC = mpicc
    GDAL_CFLAGS = -I/c/OSGeo4W/include
    GDAL_LIBS   = /c/OSGeo4W/lib/gdal_i.lib
    EXT         = .exe
else
    CC = mpicc
    GDAL_CFLAGS = $(shell gdal-config --cflags)
    GDAL_LIBS   = $(shell gdal-config --libs)
    EXT         =
endif

CFLAGS  = -Wall -Werror -O3 -march=native -fopenmp $(GDAL_CFLAGS)
LDFLAGS = -O3 -march=native -fopenmp $(GDAL_LIBS) -lm

SRCS    = main.c config.c raster.c cn.c
OBJS    = main.o config.o raster.o cn.o
TARGET  = gcn10$(EXT)

.PHONY: all clean

all: $(TARGET)

clean:
	$(RM) *.o $(TARGET)

$(TARGET): \
	main.o \
	config.o \
	raster.o \
	cn.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c global.h
	$(CC) $(CFLAGS) -c $< -o $@
