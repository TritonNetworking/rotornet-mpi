#!/bin/bash

# run over TCP:
#mpirun -mca pml ob1 -mca btl ^openib -H 10.1.100.40,10.1.100.44 rotor_test

# run over IB (RoCE):
mpirun -mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 -H 10.1.100.36,10.1.100.38,10.1.100.40,10.1.100.42,10.1.100.44 rotor_test
