#!/bin/bash

# run internal to a single node:
#mpirun -mca pml ob1 -mca btl ^openib -np 2 hellocomet

# run over TCP:
#mpirun -mca pml ob1 -mca btl ^openib -H 10.1.100.40,10.1.100.42 hellocomet

# run over IB (RoCE):
#mpirun -mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 -H 10.1.100.40,10.1.100.42 hellocomet
mpirun -mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 -H 10.1.100.40,10.1.100.42,10.1.100.44 hellocomet
