#!/bin/bash

module purge
export CXX=mpicxx

# compile with Intel 2018 toolchain:
#module load gnutools
#export MODULEPATH=/share/apps/compute/modulefiles:$MODULEPATH
#module load intel/2018.1.163
#module load intelmpi/2018.1.163

# compile with OpenMPI toolchain:
module load gnu/4.9.2
module load openmpi_ib/1.8.4

make
