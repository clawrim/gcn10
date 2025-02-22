#include "global.h"

Raster *allocate_raster(int nrows, int ncols, double no_data_value) {
    Raster *rast = malloc(sizeof(Raster));
    if (!rast) {
        fprintf(stderr, "Error: Unable to allocate memory for raster structure\n");
        return NULL;
    }
    rast->nrows = nrows;
    rast->ncols = ncols;
    rast->no_data_value = no_data_value;
    rast->data = malloc(nrows * ncols * sizeof(double));
    if (!rast->data) {
        fprintf(stderr, "Error: Unable to allocate memory for raster data\n");
        free(rast);
        return NULL;
    }
    return rast;
}

void free_raster(Raster *rast) {
    if (rast) {
        free(rast->data);
        free(rast);
    }
}

Raster *read_raster(const char *filename) {
    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open raster file %s\n", filename);
        return NULL;
    }

    int rows = GDALGetRasterYSize(dataset);
    int cols = GDALGetRasterXSize(dataset);
    double no_data_value = GDALGetRasterNoDataValue(GDALGetRasterBand(dataset, 1), NULL);

    Raster *rast = allocate_raster(rows, cols, no_data_value);
    if (!rast) {
        GDALClose(dataset);
        return NULL;
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    if (GDALRasterIO(band, GF_Read, 0, 0, cols, rows, rast->data, cols, rows, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "Error: Failed to read raster data from %s\n", filename);
        free_raster(rast);
        GDALClose(dataset);
        return NULL;
    }

    GDALClose(dataset);
    return rast;
}

void write_raster(const char *filename, Raster *rast, const char *ref_file) {
    // init and open reference //
    GDALAllRegister(); GDALDatasetH ref=GDALOpen(ref_file,GA_ReadOnly); if(!ref) {fprintf(stderr,"Error: Unable to open %s\n",ref_file);return;}
    
    // get transforms and setup options //
    double gt[6]; char *opts[]={"COMPRESS=DEFLATE","ZLEVEL=6","TILED=YES","BLOCKXSIZE=256","BLOCKYSIZE=256",NULL};
    GDALGetGeoTransform(ref,gt); const char *proj=GDALGetProjectionRef(ref);
    
    /* create output dataset */
    GDALDatasetH ds=GDALCreate(GDALGetDriverByName("GTiff"),filename,rast->ncols,rast->nrows,1,GDT_Float32,opts);
    if(!ds) {fprintf(stderr,"Error: Unable to create %s\n",filename);GDALClose(ref);return;}
    
    /* set metadata and allocate float buffer */
    GDALSetGeoTransform(ds,gt); GDALSetProjection(ds,proj); GDALRasterBandH band=GDALGetRasterBand(ds,1);
    float *fdata=malloc(rast->nrows*rast->ncols*sizeof(float)); if(!fdata) {GDALClose(ds);GDALClose(ref);return;}
    
    // convert to float and write //
    for(int i=0;i<rast->nrows*rast->ncols;i++) fdata[i]=(float)rast->data[i];
    GDALSetRasterNoDataValue(band,(float)rast->no_data_value);
    if(GDALRasterIO(band,GF_Write,0,0,rast->ncols,rast->nrows,fdata,rast->ncols,rast->nrows,GDT_Float32,0,0)!=CE_None)
        fprintf(stderr,"Error: Failed to write %s\n",filename);
    
    free(fdata); GDALClose(ds); GDALClose(ref);
}

GDALDatasetH open_rainfall_dataset(const char *filename) {
    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open rainfall raster %s\n", filename);
    }
    return dataset;
}

