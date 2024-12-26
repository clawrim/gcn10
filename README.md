# GCN10: Global Curve Number Dataset

Welcome to the **Global Curve Number Dataset Repository**! This repository contains tools and resources to generate a high-resolution, open-source global curve number dataset designed to enhance runoff estimation and hydrological modeling across diverse regions.

## Overview

The GCN10 dataset provides 18 global raster maps tailored to distinct land cover types, soil properties, and hydrological conditions. It is optimized for integration into hydrological models and supports research, policy-making, and water resource management.

### Key Features
1. High-resolution global curve number dataset.
2. Open-source and GIS-agnostic, compatible with common hydrological models.
3. Supports diverse climatic regions, land uses, and soil types.
4. Includes tools for generating curve number rasters blockwise and calculating runoff.

---

## Repository Contents
1. **Scripts**:
   - `serial-cn-processing`: Generates curve number rasters blockwise (serial).
   - `parallel-cn-processing`: Parallelized generation of curve number rasters.
   - `scs-cn-method`: Calculates runoff using the SCS Curve Number method.
2. **Documentation**: All the instructions are provided either in the script or in a separate local REAMDE file.
3. **Sample Data**: Example rasters for testing and validation.

---

## Getting Started

### Clone the Repository
```bash
git clone https://github.com/mabdazzam/GCN10
cd GCN10
```
