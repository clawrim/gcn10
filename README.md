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
   - `generate_curve_number.py`: Generates curve number rasters blockwise (serial).
   - `parallel_cn.py`: Parallelized generation of curve number rasters.
   - `scs_runoff`: Calculates runoff using the SCS Curve Number method.
2. **Documentation**: Instructions for usage and integration.
3. **Sample Data**: Example rasters for testing and validation.
4. **Validation Results**: Performance tests across hydrological scenarios.

---

## Getting Started

### Clone the Repository
```bash
git clone https://github.com/mabdazzam/GCN10
cd GCN10
```
