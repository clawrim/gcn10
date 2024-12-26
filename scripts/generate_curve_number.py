"""
Script: generate_curve_number.py
Author: Abdullah Azzam
Email: abdazzam@nmsu.edu

Description:
This script generates Curve Numbers (CN) serially, blockwise, based on block information provided in the ESA extent blocks shapefile (`esa_extent_blocks.shp`). 
For each block, it performs the following operations:
1. Loads raster data from the HYSOGs (soil data) and ESA WorldCover (land cover data) datasets.
2. Resamples the HYSOGs data to match the resolution and extent of the ESA raster data.
3. Modifies the HYSOGs data based on hydrologic soil group (HSG) conditions:
   - `Drained` conditions: Soil groups are adjusted based on specific rules.
   - `Undrained` conditions: Soil groups are modified differently for this condition.
4. Applies a lookup table based on hydrologic condition (`hc`) and antecedent runoff condition (`arc`) to compute Curve Numbers.
5. Generates 9 Curve Number rasters for each block, corresponding to combinations of:
   - Hydrologic conditions (`hc`): `poor (p)`, `fair (f)`, `good (g)`
   - Antecedent runoff conditions (`arc`): `i`, `ii`, `iii`

   In total, these combinations are:
   - `p_i`, `p_ii`, `p_iii`
   - `f_i`, `f_ii`, `f_iii`
   - `g_i`, `g_ii`, `g_iii`

6. Saves the output Curve Number rasters as GeoTIFF files in separate directories for `drained` and `undrained` conditions, using the naming convention `cn_{hc}_{arc}_{block_id}.tif`.

Additional Features:
- Progress bar: Tracks the processing of blocks.
- Skips blocks that have already been processed (if all expected output files exist).
- Skips blocks without relevant hydrologic soil group (HSG) data to improve efficiency.

Inputs:
- ESA WorldCover dataset (`esa_worldcover_2021.vrt`): Global land cover data.
- HYSOGs250m dataset (`HYSOGs250m.tif`): Soil properties raster data.
- Lookup tables (`default_lookup_{hc}_{arc}.csv`): Provide the mapping between land cover, soil groups, and Curve Numbers.
- ESA extent blocks shapefile (`esa_extent_blocks.shp`): Defines the spatial extent of each block.

Outputs:
- 9 GeoTIFF files per block (total of 18 per block, including `drained` and `undrained` conditions).
- Output directories:
  - `cn_rasters_drained/`
  - `cn_rasters_undrained/`

Usage:
- Process all blocks starting from a specific block ID:
  ```python
  main()

"""

from osgeo import gdal, ogr, osr
import geopandas as gpd
import numpy as np
import pandas as pd
import os
from tqdm import tqdm  # Progress bar

# Paths
HYSOGS_DATA_PATH = "../hsg/HYSOGs250m.tif"
ESA_DATA_PATH = "../landcover/esa_worldcover_2021.vrt"
LOOKUP_TABLE_PATH = "../lookups"
OUTPUT_DIR_DRAINED = "cn_rasters_drained"
OUTPUT_DIR_UNDRAINED = "cn_rasters_undrained"
BLOCKS_SHP_PATH = "shps/esa_extent_blocks.shp"

# Create output directories if they do not exist
os.makedirs(OUTPUT_DIR_DRAINED, exist_ok=True)
os.makedirs(OUTPUT_DIR_UNDRAINED, exist_ok=True)

def load_raster_data(file_path, bbox):
    """Load raster data using GDAL for the specified bounding box."""
    src_ds = gdal.Open(file_path)
    gt = src_ds.GetGeoTransform()
    
    # Calculate pixel offsets
    minx, miny, maxx, maxy = bbox
    x_offset = int((minx - gt[0]) / gt[1])
    y_offset = int((maxy - gt[3]) / gt[5])
    x_size = int((maxx - minx) / gt[1])
    y_size = int((maxy - miny) / abs(gt[5]))
    
    data = src_ds.ReadAsArray(x_offset, y_offset, x_size, y_size)
    transform = (minx, gt[1], 0, maxy, 0, gt[5])
    crs = osr.SpatialReference()
    crs.ImportFromWkt(src_ds.GetProjection())
    
    return data, transform, crs

def resample_raster(source_path, target_shape, target_transform):
    """Resample raster data using GDAL to match the target shape and transform."""
    src_ds = gdal.Open(source_path)
    mem_driver = gdal.GetDriverByName('MEM')
    
    # Create memory dataset for the destination raster
    dst_ds = mem_driver.Create('', target_shape[1], target_shape[0], 1, gdal.GDT_Float32)
    dst_ds.SetGeoTransform(target_transform)
    dst_ds.SetProjection(src_ds.GetProjection())
    
    # Resample
    gdal.ReprojectImage(src_ds, dst_ds, src_ds.GetProjection(), src_ds.GetProjection(), gdal.GRA_NearestNeighbour)
    data = dst_ds.ReadAsArray()
    
    return data

def load_lookup_table(hc, arc):
    filename = f"default_lookup_{hc.lower()}_{arc.lower()}.csv"
    filepath = os.path.join(LOOKUP_TABLE_PATH, filename)
    df = pd.read_csv(filepath)
    lookup_table = {}
    for index, row in df.iterrows():
        land_cover = int(row['grid_code'].split('_')[0])
        soil_group = int(row['grid_code'].split('_')[1])
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
    cn_raster = np.full(esa_data.shape, 255, dtype=np.uint8)
    for (land_cover, soil_group), cn_value in lookup_table.items():
        mask = (esa_data == land_cover) & (hysogs_data == soil_group)
        cn_raster[mask] = cn_value
    return cn_raster

def save_raster(data, transform, crs, output_path):
    """Save raster data to a GeoTIFF file using GDAL, treating 255 as the no-data value."""
    driver = gdal.GetDriverByName('GTiff')
    dst_ds = driver.Create(output_path, data.shape[1], data.shape[0], 1, gdal.GDT_Byte, ['COMPRESS=LZW'])
    dst_ds.SetGeoTransform(transform)
    dst_ds.SetProjection(crs.ExportToWkt())
    dst_ds.GetRasterBand(1).WriteArray(data)
    dst_ds.GetRasterBand(1).SetNoDataValue(255)
    dst_ds.FlushCache()

def process_block(block_id, block_geom):
    """Process a single block using its ID and geometry."""
    bbox = block_geom.bounds  # Get bounding box for the block
    
    # Define the conditions
    conditions = [('p', 'i'), ('p', 'ii'), ('p', 'iii'), ('f', 'i'), ('f', 'ii'), ('f', 'iii'), ('g', 'i'), ('g', 'ii'), ('g', 'iii')]
    
    # Iterate over drained and undrained conditions
    for condition in ['drained', 'undrained']:
        # Check if all expected output files exist
        output_paths = [
            os.path.join(OUTPUT_DIR_DRAINED if condition == 'drained' else OUTPUT_DIR_UNDRAINED, f"cn_{hc}_{arc}_{block_id}.tif")
            for hc, arc in conditions
        ]
        if all(os.path.exists(output_path) for output_path in output_paths):
            print(f"All rasters for block {block_id} ({condition}) already exist. Skipping.")
            continue

        # Load ESA and HYSOGS raster data for the block
        try:
            esa_data, esa_transform, esa_crs = load_raster_data(ESA_DATA_PATH, bbox)
            if esa_data is None:
                raise ValueError(f"ESA data for block {block_id} is unavailable.")
        except Exception as e:
            print(f"Error loading ESA data for block {block_id}: {e}. Skipping block.")
            continue

        try:
            hysogs_data, hysogs_transform, hysogs_crs = load_raster_data(HYSOGS_DATA_PATH, bbox)
            if hysogs_data is None:
                raise ValueError(f"HYSOGS data for block {block_id} is unavailable.")
        except Exception as e:
            print(f"Error loading HYSOGS data for block {block_id}: {e}. Skipping block.")
            continue

        # Resample HYSOGS data to match ESA data resolution
        hysogs_data = resample_raster(HYSOGS_DATA_PATH, esa_data.shape, esa_transform)
        
        # Skip block if no HSG data is present
        if not np.any(np.isin(hysogs_data, [1, 2, 3, 4])):
            print(f"No HSG data in block {block_id}. Skipping CN generation.")
            continue
        
        modified_hysogs_data = modify_hysogs_data(hysogs_data.copy(), condition)
        
        for hc, arc in conditions:
            lookup_table = load_lookup_table(hc, arc)
            cn_raster = calculate_cn(esa_data, modified_hysogs_data, lookup_table)
            
            output_dir = OUTPUT_DIR_DRAINED if condition == 'drained' else OUTPUT_DIR_UNDRAINED
            output_path = os.path.join(output_dir, f"cn_{hc}_{arc}_{block_id}.tif")
            
            save_raster(cn_raster, esa_transform, esa_crs, output_path)

def main(block_ids=None, start_block_id=2643):
    # Load the blocks shapefile
    blocks_gdf = gpd.read_file(BLOCKS_SHP_PATH)
    
    # Filter blocks to start from a specific block ID
    blocks_gdf = blocks_gdf[blocks_gdf['ID'] >= start_block_id]
    
    # Filter blocks by IDs if provided, otherwise process all blocks from start_block_id
    if block_ids:
        blocks_gdf = blocks_gdf[blocks_gdf['ID'].isin(block_ids) & (blocks_gdf['ID'] >= start_block_id)]
    
    # Iterate through the blocks with a progress bar
    for _, block in tqdm(blocks_gdf.iterrows(), total=len(blocks_gdf), desc="Processing blocks"):
        process_block(block['ID'], block.geometry)

if __name__ == "__main__":
    # Example: To process all blocks starting from block ID 990, use main().
    # Example: To process specific blocks, use main(block_ids=[990, 991, 992]).
    main()

