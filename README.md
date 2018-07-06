# rotornet-mpi

RotorNet MPI Implementation

## Instructions:

Go to either /microbenchmarks or /rlb_vX

There are a few different scripts for compiling and running on either Comet or Sysnet servers:

Compiling:
$ ./compile_<...>.sh

Running on Comet:
$ sbatch go_<...>.sb

Running on Sysnet:
$ mpirun <flags> <executable>

OR

$ ./go_sysnet.sh
