Google Earth Engine Script
==========================

This script generates Curve Number (CN) rasters using Google Earth Engine (GEE) by combining ESA WorldCover 2021 land cover and HYSOGs250m soil datasets.

Key Features
------------

- Reprojects and resamples HYSOGs to match ESA resolution.
- Applies CN lookup table logic using land cover and soil group combinations.
- Outputs a 10m resolution CN raster.
- Can export results to Google Drive or Earth Engine assets.

Notes
-----
- The HYSOGs250m soil dataset must be uploaded as an asset to your Earth Engine project before running the script.

- Designed for regional-scale dynamic CN generation.
- Lookup tables and classification logic are embedded in the script.
