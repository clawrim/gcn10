# Build `gcn10` as a Conda package

Minimal steps to **build** and **install** the `gcn10` conda package from `conda/recipe/`.

## Prerequisites
- Conda (Miniconda/Anaconda) is installed and initialized for your shell.
- Install build tools once:
  ```sh
  conda install -y -c conda-forge --override-channels conda-build cmake ninja pkg-config
  ```
  *(On Windows, `pkg-config` isn’t required.)*


## Linux / macOS

### 1) Build (choose **one** MPI variant per run)
OpenMPI:
```sh
conda build conda/recipe -c conda-forge --override-channels --variants "{mpi: openmpi}"
```
MPICH:
```sh
conda build conda/recipe -c conda-forge --override-channels --variants "{mpi: mpich}"
```
or simply
```sh
conda build src/recipe
```

### 2) Install from the local build into a fresh env
```sh
conda create -y -n gcn10test -c "file://$HOME/usr/local/opt/miniconda3/conda-bld" -c conda-forge gcn10
conda activate gcn10test
```

### 3) Check and Run
Confirm installation;

```sh
gcn10 --help
gcn10 --version
```
Run a trial run;
```sh
cd src/test
mpirun -np 4 gcn10 -c config.txt -l blocks.txt
```

## Windows (cmd.exe)

### 1) Build (MS-MPI variant)
```bat
conda build conda\recipe -c conda-forge --override-channels --variants "{mpi: msmpi}"
```

### 2) Install from the local build into a fresh env
```bat
conda create -y -n gcn10test ^
  -c file://%USERPROFILE%\usr\local\opt\miniconda3\conda-bld ^
  -c conda-forge ^
  gcn10
conda activate gcn10test
```

### 3) Check and Run
Confirm installation;
```bat
gcn10 --help
gcn10 --version
```
Run a trial run (assuming you built from the repo source);
```bat
cd src\test
mpiexec -n 4 gcn10 -c config.txt -l blocks.txt
```

## Notes
- Build **one** MPI provider per package; repeat for other variants as needed.
- Local build artifacts are under your Conda `conda-bld` folder (e.g., `.../linux-64/`, `.../win-64/`).
