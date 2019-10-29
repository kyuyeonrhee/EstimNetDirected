#!/bin/bash

#SBATCH --job-name="SimualateERGM"
#SBATCH --ntasks=1
#SBATCH --time=0-0:30:00

echo -n "started at: "; date

ROOT=..

module load R

time ${ROOT}/src/SimulateERGM  sim_config_example.txt

time Rscript ${ROOT}/scripts/plotSimulationDiagnostics.R stats_sim_n2000_sample.txt

echo -n "ended at: "; date

