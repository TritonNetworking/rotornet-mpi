# rotornet-mpi
RotorNet MPI Implementation

## To setup comet for running with the 2018 Intel toolchain

module purge
module load gnutools
export MODULEPATH=/share/apps/compute/modulefiles:$MODULEPATH
module load intel/2018.1.163
module load intelmpi/2018.1.163

export CXX=mpicxx
make
