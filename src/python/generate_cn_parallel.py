#!/usr/bin/env python

"""
Script: parallel_cn.py
Author: Abdullah Azzam
Email: abdazzam@nmsu.edu

Description:
This script generates Curve Numbers (CN) in parallel, blockwise, using multiprocessing to enhance processing speed. Block information is derived from the ESA extent blocks shapefile (`esa_extent_blocks.shp`). For each block, the script performs the following operations:

1. **Load and Preprocess Data**:
   - Extracts raster data for the block from the HYSOGs (soil data) and ESA WorldCover (land cover data) datasets.
   - Resamples HYSOGs data to match the spatial resolution and extent of ESA raster data.

2. **Hydrologic Soil Group (HSG) Adjustments**:
   - Modifies HYSOGs data based on two conditions:
     - **Drained**: Adjusts soil group classifications accordingly.
     - **Undrained**: Applies different adjustments for undrained conditions.
   - Handles missing or invalid HYSOGs data by assigning default values and skipping blocks without valid HSG data.

3. **Curve Number Calculation**:
   - Applies a lookup table based on hydrologic condition (`hc`) and antecedent runoff condition (`arc`) to compute Curve Numbers.
   - Generates **9 CN rasters per block**, representing the combinations of:
     - Hydrologic conditions (`hc`): `p`, `f`, `g`
     - Antecedent runoff conditions (`arc`): `i`, `ii`, `iii`
   - These combinations yield the following rasters:
     - `p_i`, `p_ii`, `p_iii`
     - `f_i`, `f_ii`, `f_iii`
     - `g_i`, `g_ii`, `g_iii`
   - This is done for both drained and undrained conditions on the areas with dual HSGs. 

4. **Parallel Processing**:
   - Utilizes the `multiprocessing` library to distribute block processing across multiple CPU cores.
   - The number of processes can be specified via a command-line argument or defaults to the number of available CPU cores.

5. **Output**:
   - Saves generated CN rasters as Cloud Optimized GeoTIFF files with compression in separate directories:
     - `cn_rasters_drained/` for drained conditions.
     - `cn_rasters_undrained/` for undrained conditions.
   - Each output file follows the naming convention: `cn_{hc}_{arc}_{block_id}.tif`.

6. **Efficiency**:
   - Skips processing for blocks where all output files already exist.
   - Progress is displayed using a progress bar for better tracking.

Inputs:
- ESA WorldCover dataset (`esa_worldcover_2021.vrt`): Global land cover data.
- HYSOGs250m dataset (`HYSOGs250m.tif`): Soil properties raster data.
- Lookup tables (`default_lookup_{hc}_{arc}.csv`): Mapping between land cover, soil groups, and Curve Numbers.
- ESA extent blocks shapefile (`esa_extent_blocks.shp`): Defines the spatial extent of each block.

Outputs:
- **18 GeoTIFF files per block**:
  - 9 rasters for drained conditions.
  - 9 rasters for undrained conditions.

Usage:
# run the script to process all blocks starting from a specific block ID:
# (Replace <block_id> with the desired starting block ID)
  python parallel_cn.py --start_block_id <block_id>

# process specific blocks by providing a file containing block IDs:
# (Replace <file_path> with the path to your block IDs file)
  python parallel_cn.py --block_ids_file <file_path>

# specify the number of parallel processes to use:
# (Replace <num_processes> with the number of processes you want)
  python parallel_cn.py --processes <num_processes>

"""

from osgeo import gdal, ogr, osr
import geopandas as gpd
import numpy as np
import pandas as pd
import os
from tqdm import tqdm
from multiprocessing import Pool, cpu_count

# paths
HYSOGS_DATA_PATH = "../../hsg/HYSOGs250m.tif"
ESA_DATA_PATH = "../../landcover/esa_worldcover_2021.vrt"
LOOKUP_TABLE_PATH = "../../lookups"
OUTPUT_DIR_DRAINED = "cn_rasters_drained"
OUTPUT_DIR_UNDRAINED = "cn_rasters_undrained"
BLOCKS_SHP_PATH = "../../shps/esa_extent_blocks.shp"

# create output directories if they do not exist
os.makedirs(OUTPUT_DIR_DRAINED, exist_ok=True)
os.makedirs(OUTPUT_DIR_UNDRAINED, exist_ok=True)

#def save_raster(data, transform, crs, output_path):
#    """Save raster data to a GeoTIFF file using GDAL, treating 255 as the no-data value."""
#    driver = gdal.GetDriverByName('GTiff')
#    dst_ds = driver.Create(output_path, data.shape[1], data.shape[0], 1, gdal.GDT_Byte, ['COMPRESS=LZW'])
#    dst_ds.SetGeoTransform(transform)
#    dst_ds.SetProjection(crs.ExportToWkt())
#    dst_ds.GetRasterBand(1).WriteArray(data)
#    dst_ds.GetRasterBand(1).SetNoDataValue(255)
#    dst_ds.FlushCache()

def save_raster(data, transform, crs, output_path):
    """Save raster data as a Cloud Optimized GeoTIFF with compression and tiling."""
    driver = gdal.GetDriverByName('GTiff')
    options = [
        'TILED=YES',                # required for COG
        'BLOCKXSIZE=512',           # standard tile width
        'BLOCKYSIZE=512',           # standard tile height
        'COMPRESS=DEFLATE',         # better than LZW (did some tests)
        'PREDICTOR=2',              # good for integer data
        'ZLEVEL=9',                 # maximum compression
        'COPY_SRC_OVERVIEWS=YES'    # needed for cloud optimization
    ]
    dst_ds = driver.Create(output_path, data.shape[1], data.shape[0], 1, gdal.GDT_Byte, options)
    dst_ds.SetGeoTransform(transform)
    dst_ds.SetProjection(crs.ExportToWkt())
    band = dst_ds.GetRasterBand(1)
    band.WriteArray(data)
    band.SetNoDataValue(255)
    dst_ds.BuildOverviews('NEAREST', [2, 4, 8, 16])  # add overviews (required for COG)
    dst_ds.FlushCache()

def load_raster_data(file_path, bbox):
    """Load raster data using GDAL for the specified bounding box, with clipping to raster dimensions."""
    src_ds = gdal.Open(file_path)
    gt = src_ds.GetGeoTransform()
    if src_ds is None:
        print(f"Failed to load raster data from {file_path}")
        return None, None, None

    # bounding box coordinates
    minx, miny, maxx, maxy = bbox
    
    # get raster dimensions and bounds
    raster_x_max = gt[0] + (src_ds.RasterXSize * gt[1])
    raster_y_min = gt[3] + (src_ds.RasterYSize * gt[5])

    # clip bounding box to raster bounds if necessary
    maxx = min(maxx, raster_x_max)
    miny = max(miny, raster_y_min)

    # calculate pixel offsets for the adjusted bounding box
    x_offset = int((minx - gt[0]) / gt[1])
    y_offset = int((maxy - gt[3]) / gt[5])  # y_offset is top-left, so maxy is used

    # calculate window size (x_size and y_size) based on the adjusted bounding box
    x_size = int((maxx - minx) / gt[1])
    y_size = int((maxy - miny) / abs(gt[5]))

    # check if calculated window size is valid
    if x_size <= 0 or y_size <= 0:
        print(f"Invalid window size for {file_path} with bbox {bbox}: x_size={x_size}, y_size={y_size}")
        return None, None, None

    # ensure that the access window is within raster bounds
    if x_offset + x_size > src_ds.RasterXSize:
        x_size = src_ds.RasterXSize - x_offset
    if y_offset + y_size > src_ds.RasterYSize:
        y_size = src_ds.RasterYSize - y_offset

    # read the data within the valid window
    data = src_ds.ReadAsArray(x_offset, y_offset, x_size, y_size)
    transform = (minx, gt[1], 0, maxy, 0, gt[5])
    crs = osr.SpatialReference()
    crs.ImportFromWkt(src_ds.GetProjection())
    
    return data, transform, crs

def resample_raster(src_file, target_shape, target_transform):
    """Resample raster data to match the target shape and transform."""
    src_ds = gdal.Open(src_file)
    src_band = src_ds.GetRasterBand(1)
    src_nodata = src_band.GetNoDataValue()

    # create an in-memory raster to store the resampled data
    mem_drv = gdal.GetDriverByName('MEM')
    mem_ds = mem_drv.Create('', target_shape[1], target_shape[0], 1, gdal.GDT_Byte)
    mem_ds.SetGeoTransform(target_transform)
    mem_ds.SetProjection(src_ds.GetProjection())

    # perform the resampling
    gdal.ReprojectImage(src_ds, mem_ds, None, None, gdal.GRA_NearestNeighbour)

    # read the resampled data into a NumPy array
    resampled_data = mem_ds.GetRasterBand(1).ReadAsArray()

    # handle the NoData value (replace with 255 instead of np.nan)
    if src_nodata is not None:
        resampled_data[resampled_data == src_nodata] = 255  # use 255 or another integer as NoData

    return resampled_data

def load_lookup_table(hc, arc):
    filename = f"default_lookup_{hc.lower()}_{arc.lower()}.csv"
    filepath = os.path.join(LOOKUP_TABLE_PATH, filename)
    df = pd.read_csv(filepath)
    lookup_table = {}
    for index, row in df.iterrows():
        land_cover = int(row['grid_code'].split('_')[0])
        #soil_group = int(row['grid_code'].split('_')[1])
        soil_group_str = row['grid_code'].split('_')[1]
        soil_map = {'A': 1, 'B': 2, 'C': 3, 'D': 4}
        soil_group = soil_map.get(soil_group_str.upper(), 4)  # default to D if unknown
        cn_value = int(row['cn'])
        lookup_table[(land_cover, soil_group)] = cn_value
    return lookup_table

def modify_hysogs_data(hysogs_data, condition):
    """Modify HYSOGs data based on the specified condition (drained or undrained)."""
    if condition == 'drained':
        hysogs_data = np.where(hysogs_data == 11, 4, hysogs_data)
        hysogs_data = np.where(hysogs_data == 12, 4, hysogs_data)
        hysogs_data = np.where(hysogs_data == 13, 4, hysogs_data)
        hysogs_data = np.where(hysogs_data == 14, 4, hysogs_data)
    elif condition == 'undrained':  
        hysogs_data = np.where(hysogs_data == 11, 1, hysogs_data)
        hysogs_data = np.where(hysogs_data == 12, 2, hysogs_data)
        hysogs_data = np.where(hysogs_data == 13, 3, hysogs_data)
        hysogs_data = np.where(hysogs_data == 14, 4, hysogs_data)
    return hysogs_data

def calculate_cn(esa_data, hysogs_data, lookup_table):
    cn_raster = np.full(esa_data.shape, 255, dtype=np.uint8)  # initialize with no-data value
    for (land_cover, soil_group), cn_value in lookup_table.items():
        mask = (esa_data == land_cover) & (hysogs_data == soil_group)
        cn_raster[mask] = cn_value
    return cn_raster

def process_block(block):                                 
    block_id, block_geom = block                                                    
    bbox = block_geom.bounds     
                                                          
    # define the conditions                                                         
    conditions = [('p', 'i'), ('p', 'ii'), ('p', 'iii'), ('f', 'i'), ('f', 'ii'), ('f', 'iii'), ('g', 'i'), ('g', 'ii'), ('g', 'iii')]   
                                                                                
    for condition in ['drained', 'undrained']:                                    
        output_paths = [                                                                       
            os.path.join(OUTPUT_DIR_DRAINED if condition == 'drained' else OUTPUT_DIR_UNDRAINED, f"cn_{hc}_{arc}_{block_id}.tif")                                             
            for hc, arc in conditions                                                          
        ]                                                                          
        if all(os.path.exists(output_path) for output_path in output_paths):  
            print(f"All rasters for block {block_id} ({condition}) already exist. Skipping.")
            continue                                                                         
                                                                 
        try:                                                   
            esa_data, esa_transform, esa_crs = load_raster_data(ESA_DATA_PATH, bbox)
            if esa_data is None:
                print(f"ESA data could not be loaded for block {block_id}. Skipping block.")
                continue
        except Exception as e:                                                        
            print(f"Error loading ESA data for block {block_id}: {e}. Skipping block.")
            continue                                                                   
                                                                                      
        try:                                                           
            hysogs_data, hysogs_transform, hysogs_crs = load_raster_data(HYSOGS_DATA_PATH, bbox)
            if hysogs_data is None:
                print(f"HYSOGS data could not be loaded for block {block_id}. Skipping block.")
                continue
        except Exception as e:                                                                  
            print(f"Error loading HYSOGS data for block {block_id}: {e}. Skipping block.")
            continue                                                                      
                                                            
        # resample HYSOGS data to match ESA data shape and transform
        hysogs_data = resample_raster(HYSOGS_DATA_PATH, esa_data.shape, esa_transform)

        # handle missing or invalid HYSOGS data
        missing_hysogs_mask = np.isnan(hysogs_data) | ~(np.isin(hysogs_data, [1, 2, 3, 4]))
        if np.any(missing_hysogs_mask):         
            print(f"Missing or invalid HYSOGS data in block {block_id}. Assigning HSG D to missing pixels.")                       
            hysogs_data[missing_hysogs_mask] = 4                              
                                                
        # skip CN generation if no valid HSG data is present
        if not np.any(np.isin(hysogs_data, [1, 2, 3, 4])):                
            print(f"No valid HSG data in block {block_id}. Skipping CN generation.")
            continue

        # modify HYSOGS data based on condition
        modified_hysogs_data = modify_hysogs_data(hysogs_data.copy(), condition)

        # generate and save CN rasters
        for hc, arc in conditions:
            lookup_table = load_lookup_table(hc, arc)
            cn_raster = calculate_cn(esa_data, modified_hysogs_data, lookup_table)
            output_dir = OUTPUT_DIR_DRAINED if condition == 'drained' else OUTPUT_DIR_UNDRAINED
            output_path = os.path.join(output_dir, f"cn_{hc}_{arc}_{block_id}.tif")
            save_raster(cn_raster, esa_transform, esa_crs, output_path)

    print(f"Block {block_id} processed successfully.")
    return "done"

def main(block_ids=None, start_block_id=1, num_processes=None):
    blocks_gdf = gpd.read_file(BLOCKS_SHP_PATH)
    
    # filter based on start_block_id and block_ids if provided
    if block_ids is not None:
        blocks_gdf = blocks_gdf[blocks_gdf['ID'].isin(block_ids)]
    else:
        blocks_gdf = blocks_gdf[blocks_gdf['ID'] >= start_block_id]

    block_data = [(block['ID'], block.geometry) for _, block in blocks_gdf.iterrows()]

    num_cores = num_processes if num_processes else cpu_count()
    with Pool(num_cores) as pool:
        results = list(tqdm(pool.imap(process_block, block_data), total=len(block_data), desc="Processing blocks"))

    done = results.count("done")
    print(f"Processing complete: {done} blocks processed.")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Process blocks in parallel.")
    parser.add_argument('--processes', type=int, default=None, help='Number of parallel processes to run')
    parser.add_argument('--start_block_id', type=int, default=1, help='Start processing from this block ID')
    parser.add_argument('--block_ids_file', type=str, help='Path to a text file containing block IDs to process')
    args = parser.parse_args()

    # read block IDs from file if provided
    block_ids = None
    if args.block_ids_file:
        with open(args.block_ids_file, 'r') as f:
            block_ids = [int(line.strip()) for line in f if line.strip().isdigit()]
    
    # pass the list of block IDs to main if provided
    main(block_ids=block_ids, start_block_id=args.start_block_id, num_processes=args.processes)

