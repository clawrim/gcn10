// Load ESA WorldCover dataset (v200)
var dataset = ee.ImageCollection('ESA/WorldCover/v200').first();

// Load HSG raster from user assets
var hsg = ee.Image('projects/ee-gcn10/assets/HYSOGs250m');

// Helper function to define lookup tables
function getLookups() {
  var esaLookup = {
    '10,1,poor': 45, '10,2,poor': 66, '10,3,poor': 77, '10,4,poor': 83,
    '10,1,fair': 36, '10,2,fair': 60, '10,3,fair': 73, '10,4,fair': 79,
    '10,1,good': 30, '10,2,good': 55, '10,3,good': 70, '10,4,good': 77,
    '20,1,poor': 63, '20,2,poor': 77, '20,3,poor': 85, '20,4,poor': 88,
    '20,1,fair': 55, '20,2,fair': 72, '20,3,fair': 81, '20,4,fair': 86,
    '20,1,good': 49, '20,2,good': 68, '20,3,good': 79, '20,4,good': 84,
    '30,1,poor': 68, '30,2,poor': 79, '30,3,poor': 86, '30,4,poor': 89,
    '30,1,fair': 49, '30,2,fair': 69, '30,3,fair': 79, '30,4,fair': 84,
    '30,1,good': 39, '30,2,good': 61, '30,3,good': 74, '30,4,good': 80,
    '40,1,poor': 72, '40,2,poor': 81, '40,3,poor': 88, '40,4,poor': 91,
    '40,1,fair': 70, '40,2,fair': 80, '40,3,fair': 87, '40,4,fair': 90,
    '40,1,good': 67, '40,2,good': 78, '40,3,good': 85, '40,4,good': 89,
    '50,1,poor': 89, '50,2,poor': 92, '50,3,poor': 94, '50,4,poor': 95,
    '50,1,fair': 89, '50,2,fair': 92, '50,3,fair': 94, '50,4,fair': 95,
    '50,1,good': 89, '50,2,good': 92, '50,3,good': 94, '50,4,good': 95,
    '60,1,poor': 65, '60,2,poor': 79, '60,3,poor': 87, '60,4,poor': 90,
    '60,1,fair': 65, '60,2,fair': 79, '60,3,fair': 87, '60,4,fair': 90,
    '60,1,good': 65, '60,2,good': 79, '60,3,good': 87, '60,4,good': 90,
    '70,1,poor': 0, '70,2,poor': 0, '70,3,poor': 0, '70,4,poor': 0,
    '70,1,fair': 0, '70,2,fair': 0, '70,3,fair': 0, '70,4,fair': 0,
    '70,1,good': 0, '70,2,good': 0, '70,3,good': 0, '70,4,good': 0,
    '80,1,poor': 100, '80,2,poor': 100, '80,3,poor': 100, '80,4,poor': 100,
    '80,1,fair': 100, '80,2,fair': 100, '80,3,fair': 100, '80,4,fair': 100,
    '80,1,good': 100, '80,2,good': 100, '80,3,good': 100, '80,4,good': 100,
    '90,1,poor': 80, '90,2,poor': 80, '90,3,poor': 80, '90,4,poor': 80,
    '90,1,fair': 80, '90,2,fair': 80, '90,3,fair': 80, '90,4,fair': 80,
    '90,1,good': 80, '90,2,good': 80, '90,3,good': 80, '90,4,good': 80,
    '95,1,poor': 0, '95,2,poor': 0, '95,3,poor': 0, '95,4,poor': 0,
    '95,1,fair': 0, '95,2,fair': 0, '95,3,fair': 0, '95,4,fair': 0,
    '95,1,good': 0, '95,2,good': 0, '95,3,good': 0, '95,4,good': 0,
    '100,1,poor': 74, '100,2,poor': 77, '100,3,poor': 78, '100,4,poor': 79,
    '100,1,fair': 74, '100,2,fair': 77, '100,3,fair': 78, '100,4,fair': 79,
    '100,1,good': 74, '100,2,good': 77, '100,3,good': 78, '100,4,good': 79
  };
  
  var arcConversion = {
    100: {'i': 100, 'ii': 100, 'iii': 100},
    99: {'i': 97, 'ii': 99, 'iii': 100},
    98: {'i': 94, 'ii': 98, 'iii': 99},
    97: {'i': 91, 'ii': 97, 'iii': 99},
    96: {'i': 89, 'ii': 96, 'iii': 99},
    95: {'i': 87, 'ii': 95, 'iii': 98},
    94: {'i': 85, 'ii': 94, 'iii': 98},
    93: {'i': 83, 'ii': 93, 'iii': 98},
    92: {'i': 81, 'ii': 92, 'iii': 97},
    91: {'i': 80, 'ii': 91, 'iii': 97},
    90: {'i': 78, 'ii': 90, 'iii': 96},
    89: {'i': 76, 'ii': 89, 'iii': 96},
    88: {'i': 75, 'ii': 88, 'iii': 95},
    87: {'i': 73, 'ii': 87, 'iii': 95},
    86: {'i': 72, 'ii': 86, 'iii': 94},
    85: {'i': 70, 'ii': 85, 'iii': 94},
    84: {'i': 68, 'ii': 84, 'iii': 93},
    83: {'i': 67, 'ii': 83, 'iii': 93},
    82: {'i': 66, 'ii': 82, 'iii': 92},
    81: {'i': 64, 'ii': 81, 'iii': 92},
    80: {'i': 63, 'ii': 80, 'iii': 91},
    79: {'i': 62, 'ii': 79, 'iii': 91},
    78: {'i': 60, 'ii': 78, 'iii': 90},
    77: {'i': 59, 'ii': 77, 'iii': 89},
    76: {'i': 58, 'ii': 76, 'iii': 89},
    75: {'i': 57, 'ii': 75, 'iii': 88},
    74: {'i': 55, 'ii': 74, 'iii': 88},
    73: {'i': 54, 'ii': 73, 'iii': 87},
    72: {'i': 53, 'ii': 72, 'iii': 86},
    71: {'i': 52, 'ii': 71, 'iii': 86},
    70: {'i': 51, 'ii': 70, 'iii': 85},
    69: {'i': 50, 'ii': 69, 'iii': 84},
    68: {'i': 48, 'ii': 68, 'iii': 84},
    67: {'i': 47, 'ii': 67, 'iii': 83},
    66: {'i': 46, 'ii': 66, 'iii': 82},
    65: {'i': 45, 'ii': 65, 'iii': 82},
    64: {'i': 44, 'ii': 64, 'iii': 81},
    63: {'i': 43, 'ii': 63, 'iii': 80},
    62: {'i': 42, 'ii': 62, 'iii': 79},
    61: {'i': 41, 'ii': 61, 'iii': 78},
    60: {'i': 40, 'ii': 60, 'iii': 78},
    59: {'i': 39, 'ii': 59, 'iii': 77},
    58: {'i': 38, 'ii': 58, 'iii': 76},
    57: {'i': 37, 'ii': 57, 'iii': 75},
    56: {'i': 36, 'ii': 56, 'iii': 75},
    55: {'i': 35, 'ii': 55, 'iii': 74},
    54: {'i': 34, 'ii': 54, 'iii': 73},
    53: {'i': 33, 'ii': 53, 'iii': 72},
    52: {'i': 32, 'ii': 52, 'iii': 71},
    51: {'i': 31, 'ii': 51, 'iii': 70},
    50: {'i': 31, 'ii': 50, 'iii': 70},
    49: {'i': 30, 'ii': 49, 'iii': 69},
    48: {'i': 29, 'ii': 48, 'iii': 68},
    47: {'i': 28, 'ii': 47, 'iii': 67},
    46: {'i': 27, 'ii': 46, 'iii': 66},
    45: {'i': 26, 'ii': 45, 'iii': 65},
    44: {'i': 25, 'ii': 44, 'iii': 64},
    43: {'i': 25, 'ii': 43, 'iii': 63},
    42: {'i': 24, 'ii': 42, 'iii': 62},
    41: {'i': 23, 'ii': 41, 'iii': 61},
    40: {'i': 22, 'ii': 40, 'iii': 60},
    39: {'i': 21, 'ii': 39, 'iii': 59},
    38: {'i': 21, 'ii': 38, 'iii': 58},
    37: {'i': 20, 'ii': 37, 'iii': 57},
    36: {'i': 19, 'ii': 36, 'iii': 56},
    35: {'i': 18, 'ii': 35, 'iii': 55},
    34: {'i': 18, 'ii': 34, 'iii': 54},
    33: {'i': 17, 'ii': 33, 'iii': 53},
    32: {'i': 16, 'ii': 32, 'iii': 52},
    31: {'i': 16, 'ii': 31, 'iii': 51},
    30: {'i': 15, 'ii': 30, 'iii': 50},
    25: {'i': 12, 'ii': 25, 'iii': 43},
    20: {'i': 9, 'ii': 20, 'iii': 37},
    15: {'i': 6, 'ii': 15, 'iii': 30},
    10: {'i': 4, 'ii': 10, 'iii': 22},
    5: {'i': 2, 'ii': 5, 'iii': 13},
    0: {'i': 0, 'ii': 0, 'iii': 0}
  };
  return {esaLookup: esaLookup, arcConversion: arcConversion};
}

// Load lookup tables
var lookups = getLookups();
var esaLookup = lookups.esaLookup;
var arcConversion = lookups.arcConversion;

// Function to create CN image for a given HC, ARC, and drainage condition
var createCNImage = function(hc, arc, drainage) {
  var cnImage = ee.Image(0).float();
  
  // Iterate over lookup table keys
  Object.keys(esaLookup).forEach(function(key) {
    var parts = key.split(',');
    var lc = parseInt(parts[0]);
    var hsgVal = parseInt(parts[1]);
    var lookupHC = parts[2];
    
    if (lookupHC === hc) {
      var baseCN = esaLookup[key];
      var adjustedCN = arc === 'ii' ? baseCN : arcConversion[baseCN][arc];
      // Apply drained/undrained adjustment (undrained increases CN by 5)
      adjustedCN = drainage === 'undrained' ? Math.min(100, adjustedCN + 5) : adjustedCN;
      
      var mask = dataset.eq(lc).and(hsg.eq(hsgVal));
      cnImage = cnImage.where(mask, adjustedCN);
    }
  });
  
  return cnImage.rename('CN_' + hc + '_' + arc + '_' + drainage);
};

// Generate CN images for all combinations
var hcs = ['poor', 'fair', 'good'];
var arcs = ['i', 'ii', 'iii'];
var drainages = ['drained', 'undrained'];

var cnImages = [];
hcs.forEach(function(hc) {
  arcs.forEach(function(arc) {
    drainages.forEach(function(drainage) {
      cnImages.push(createCNImage(hc, arc, drainage));
    });
  });
});

// Combine all CN images into a single multi-band image
var cnResult = ee.Image.cat(cnImages);

// Visualization parameters
var cnVis = {
  min: 0,
  max: 100,
  palette: ['blue', 'green', 'yellow', 'red']
};

// Center map on dataset
Map.centerObject(dataset);

// Add the first band for visualization (e.g., CN_fair_ii_drained)
Map.addLayer(cnResult.select('CN_fair_ii_drained'), cnVis, 'Curve Number (Fair, ARC II, Drained)');
