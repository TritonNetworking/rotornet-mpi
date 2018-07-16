#include <iostream>
#include <chrono>
#include <mpi.h>
#include <assert.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <strstream>
#include <fstream>
#include <algorithm>

// this is to test MPI_COMM_SPLIT controller (for multiple staggered rotors)

const int64_t run_us = 300299; // total runtime, microseconds
const int64_t slot_us = 300; // slot time, microseconds

void rotor_test(int size, int rank);

using namespace std;
using namespace chrono;

inline int64_t get_us()
{
	steady_clock::duration dur{steady_clock::now().time_since_epoch()};
	return duration_cast<microseconds>(dur).count();
}

int main(int argc, char* argv[])
{

	int size, rank;

	assert(steady_clock::is_steady);

	MPI_Init(&argc, &argv);

	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	rotor_test(size, rank);

	MPI_Finalize();

	return 0;
}

void rotor_test(int size, int rank) {
	
	int Nslots = run_us / slot_us; // total number of slots
	int offset_us = slot_us / (size - 1); // time offset of each slave node, microseconds

	if (rank == 0) { // Master "sync" node
	
		vector<int> currentslot(size - 1); // keep track of the current slot
		vector<int> recvbuffer(size - 1);
		for (int i = 0; i < (size - 1); i++) {
			currentslot[i] = 0;
			recvbuffer[i] = 0;
		}
		
		// matrix to store receive times
		vector<vector<int>> times(size - 1); // [comm_node, slot]
		for (int i = 0; i < size - 1; i++) {
			times[i].resize(Nslots);
			for (int j = 0; j < Nslots - 1; j++) {
				times[i][j] = 0;
			}
		}
		vector<MPI_Request> r_handles(size - 1); // vector of receive handles
		
		int recv_done; // dummy variable

		// sync with slave nodes:
		MPI_Barrier(MPI_COMM_WORLD);


		// Implementation with non-blocking receives:
		/*
		// open non-blocking receives from each slave node	
		for (int i = 0; i < (size - 1); i++) {
			MPI_Irecv(&recvbuffer[i], 1, MPI_INT,
				i + 1, MPI_ANY_TAG, MPI_COMM_WORLD, &r_handles[i]);
		}

		int64_t start_time = get_us(); // get start time at master node
		int done = 0;
		while (done == 0) {		
			// poll for messages from each slave node:
			for (int i = 0; i < (size - 1); i++) {
				if (currentslot[i] < Nslots) { // we're still expecting to receive something from this node
					MPI_Test(&r_handles[i], &recv_done, MPI_STATUS_IGNORE); // check if we did
					if (recv_done == 1) {
						times[i][currentslot[i]] = get_us() - start_time; // record time received
						currentslot[i]++; // update the current slot
						if (currentslot[i] < Nslots) // open another non-blocking receive from this node
							MPI_Irecv(&recvbuffer[i], 1, MPI_INT,
								i + 1, MPI_ANY_TAG, MPI_COMM_WORLD, &r_handles[i]);
					}
				}
			}
			// check if we're done (we've received Nslots times from every node)
			int done_temp=1;
			for (int i = 0; i < (size - 1); i++) {
				if (currentslot[i] < Nslots) // there's still more to go
					done_temp = 0;
			}
			done = done_temp;
		}
		*/

		// Implementation with MPI_Iprobe & blocking receives:

		int64_t start_time = get_us(); // get start time at master node
		int done = 0;
		while (done == 0) {
			// poll for messages from each slave node:
			for (int i = 0; i < (size - 1); i++) {
				MPI_Iprobe(i + 1, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_done, MPI_STATUS_IGNORE); // check if a message is waiting
				if (recv_done == 1) {
					times[i][currentslot[i]] = get_us() - start_time; // record time received
					currentslot[i]++; // update the current slot
					MPI_Recv(&recvbuffer[i], 1, MPI_INT,
						i + 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				}
			}
			// check if we're done (we've received Nslots times from every node)
			int done_temp=1;
			for (int i = 0; i < (size - 1); i++) {
				if (currentslot[i] < Nslots) // there's still more to go
					done_temp = 0;
			}
			done = done_temp;
		}


		// print the timing output to console:
		//cout << "Integers ACKed [rank, slot]:" << endl;
		cout << "Times ping received from slave nodes [rank, slot]:" << endl;
		for (int i = 0; i < (size - 1); i++) {
			//cout << "rank " << i + 1 << ": ";
			for (int j = 0; j < Nslots; j++)
				cout << times[i][j] << " ";
			cout << endl;
		}
		cout << endl;

	} else { // slave nodes
		
		// init variables
		int prev_slot = 0;
		int this_slot = 0;
		int slot = -1; // slot indexed from 0
		int sendbuf = 1;		

		// wait for master node
		MPI_Barrier(MPI_COMM_WORLD);
		
		// start RotorLB sync clock:
		int64_t start = get_us(); // start time (us)
		start = start - (rank - 1)*offset_us; // stagger the start times
		int64_t current = get_us();
		
		while (slot < Nslots - 1) {
			prev_slot = (current - start) / slot_us;
			current = get_us();
			this_slot = (current - start) / slot_us;
			if (this_slot > prev_slot) {
				slot++;
				int64_t s = get_us();
				MPI_Send(&sendbuf, 1, MPI_INT, 0, 0, MPI_COMM_WORLD); 
				s = get_us() - s;
				
				// debug:
				//if (rank == 1)
				//	cout << "rank " << rank << ", slot " << slot << ": sending took " << s << " us." << endl;
			}
		}
	}
}

