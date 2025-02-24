I have 3 working programs to calculate SCS CN runoff.
    OpenMP Parallelized to process one block at a time.
    MPI Parallelized for running more than one blocks in parallel but individual block would be serial
    Hybrid; combo of both

May be I will compare the performance of all three, for now, omp version seems better.

Validation steps;
1. Get 15 - 18 watersheds (9 representing 9 gcn10 conditions, rest must be urban).
2. Clip the GCN10 and GCN250 to the watershed extent.
3. Read the GLDAS rasters in GRASS with gcn10 and gcn250 as the g.region.
4. export daily rasters from GRASS.
    This would take some processing, maybe sum all three hourly rasters for a day, convert units from kmm-1s-1 to mm and then export.
5. Run the scs solver for each watershed.
