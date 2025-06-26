## This is for gcn10. it takes vrt as input and produces a tif clipped to the watershed.
import os
from osgeo import gdal, ogr

# Use relative paths from the current working directory (samsung/validation/shps/)
current_dir = os.getcwd()
watersheds_dir = "./"  # Current directory (samsung/validation/shps/)
vrts_base_dir = "../../global_rasters/parallelization/cn_rasters_drained/vrt/"  # Relative path to VRT base directory
output_dir = "./watersheds-cn"  # Create watersheds-cn in the current directory
os.makedirs(output_dir, exist_ok=True)

print(f"Checking watersheds in: {os.path.abspath(watersheds_dir)}")
print(f"Checking VRTs base in: {os.path.abspath(vrts_base_dir)}")
print(f"Output directory: {os.path.abspath(output_dir)}")

# Define VRT subdirectories to process
vrt_subdirs = ["f_i", "f_ii", "f_iii", "p_i", "p_ii", "p_iii", "g_i", "g_ii", "g_iii"]

# Loop through watershed folders (only directories, not files like the script itself)
for watershed_folder in [f for f in os.listdir(watersheds_dir) if os.path.isdir(os.path.join(watersheds_dir, f))]:
    watershed_path = os.path.join(watersheds_dir, watershed_folder, "watershed.gpkg")  # Use .gpkg instead of .shp
    print(f"Processing watershed folder: {watershed_folder}")
    if os.path.exists(watershed_path):
        print(f"Found watershed GeoPackage: {watershed_path}")
        # Open the watershed GeoPackage
        watershed_ds = ogr.Open(watershed_path)
        if watershed_ds is None:
            print(f"Could not open {watershed_path}")
            continue
        watershed_layer = watershed_ds.GetLayer()
        
        # Loop through each VRT subdirectory
        for subdir in vrt_subdirs:
            vrts_dir = os.path.join(vrts_base_dir, subdir)
            # Check if VRT directory exists
            if not os.path.exists(vrts_dir):
                print(f"VRT directory not found: {vrts_dir}")
                continue
            
            # List and print VRT files for debugging
            print(f"Contents of VRT directory {subdir}: {os.listdir(vrts_dir)}")
            
            # Loop through VRT files in this subdirectory
            for vrt_file in os.listdir(vrts_dir):
                if vrt_file.endswith(".vrt"):
                    vrt_path = os.path.join(vrts_dir, vrt_file)
                    print(f"Processing VRT file: {vrt_file} in {subdir}")
                    
                    # Open the VRT file
                    vrt_ds = gdal.Open(vrt_path)
                    if vrt_ds is None:
                        print(f"Could not open {vrt_path}")
                        continue
                    
                    # Create output filename (e.g., trinity_river_gcn_10_f_i.tif)
                    watershed_name = watershed_folder.replace("-", "_")  # Handle hyphens in names
                    vrt_base = os.path.splitext(vrt_file)[0]  # Remove .vrt extension
                    output_filename = f"{watershed_name}_{vrt_base}.tif"
                    output_path = os.path.join(output_dir, output_filename)
                    print(f"Generating output: {output_path}")
                    
                    # Check if output raster already exists
                    if os.path.exists(output_path):
                        print(f"Output file {output_path} already exists, skipping...")
                        continue
                    
                    try:
                        # Clip the VRT using the shapefile with gdal.Warp
                        gdal.Warp(
                            output_path,
                            vrt_path,
                            cutlineDSName=watershed_path,
                            cropToCutline=True,
                            dstNodata=None,  # Adjust if you need a specific no-data value
                            format="GTiff"
                        )
                        print(f"Successfully clipped and saved: {output_path}")
                    except Exception as e:
                        print(f"Error clipping {vrt_file} with {watershed_path} in {subdir}: {e}")
                    
                    # Clean up
                    vrt_ds = None
        watershed_ds = None
    else:
        print(f"No watershed.gpkg found in {watershed_folder}")

print("Clipping and saving completed!")
