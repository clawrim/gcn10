Download the Global 250m Hydrologic Soil Group from
the following  URL and keep it in this directory.
https://daac.ornl.gov/cgi-bin/dsviewer.pl?ds_id=1566

To fix the EPSG issue and reduce the size,
it is recommended to run the following command
in the command line with in hsg/ directory.
`gdalwarp -t_srs EPSG:4326 -of GTiff -co COMPRESS=LZW -co \
TILED=YES -co PREDICTOR=2 HYSOGs250m.tif HYSOGs250m_4326_lzw.tif`

Any script or program related to Curve Number Generation
will not run unless the HYSOGs250m.tif is present in hsg/ directory.
