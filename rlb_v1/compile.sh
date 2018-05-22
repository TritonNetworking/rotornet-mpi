#!/bin/bash

module purge
module load gnu/4.9.2
module load openmpi_ib/1.8.4

make
