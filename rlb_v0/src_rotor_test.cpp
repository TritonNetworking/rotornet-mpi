#include <iostream>
#include <chrono>
#include <mpi.h>
#include <assert.h>
#include <string.h>
#include <vector>

//const int ITEM_COUNT = (1024 * 1024 * 64); // # ints = 268435456 bytes
const int ITEM_COUNT = 1;

const int64_t run_us = 299999; // total runtime, microseconds
const int64_t slot_us = 100000; // slot time, microseconds

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
	cout << "Running ..." << endl;

	int size, rank;

	assert(steady_clock::is_steady);

	MPI_Init(&argc, &argv);

	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	rotor_test(size, rank);

	MPI_Finalize();

	return 0;
}

void rotor_kernel(int size, int rank, int slot, int * sendbuf, int * recvbuf, vector<vector<int>> sendto, vector<vector<int>> recvfrom) {

	int64_t slot_start = get_us(); // slot start time (us)

	MPI_Request r_handle; // receive handle
	MPI_Request s_handle; // send handle
			
	// start a non-blocking receive:
	MPI_Irecv(recvbuf, ITEM_COUNT, MPI_INT,
		/* src */ recvfrom[rank - 1][slot], MPI_ANY_TAG,
		MPI_COMM_WORLD, &r_handle);

	// start a non-blocking send:
	MPI_Isend(sendbuf, ITEM_COUNT, MPI_INT,
		/* dst */ sendto[rank - 1][slot], /* tag */ 0,
		MPI_COMM_WORLD, &s_handle);

	// debug:
	cout << "Rank " << rank << " started sending to rank " <<
		sendto[rank - 1][slot] << " " << get_us() - slot_start <<
		" us into the slot..." << endl;

	// poll for send and receive complete
	int recv_done = 0;
	int send_done = 0;

	while ((recv_done == 0) || (send_done == 0)) {
		if (recv_done == 0) {
			MPI_Test(&r_handle, &recv_done,
				MPI_STATUS_IGNORE);
			if (recv_done == 1) {
				cout << "Rank " << rank <<
				" done receiving " <<
				get_us() - slot_start <<
				" us into the slot." << endl;
			}
		}
		if (send_done == 0) {
			MPI_Test(&s_handle, &send_done,
				MPI_STATUS_IGNORE);
			if (send_done == 1) {
				cout << "Rank " << rank <<
				" done sending " <<
				get_us() - slot_start <<
				" us into the slot." << endl;
			}
		}
	}
	
	// if we wanted to do an explicit ACK mechanism
	// this would need additional work...
						
	// send ACK:
	/*	
	int items_received = 0;
	MPI_Get_count(&status, MPI_INT, &items_received);
	MPI_Isend(&items_received, 1, MPI_INT,
		sendto[rank - 1][1], 0, MPI_COMM_WORLD);
	*/

	// receive ACK:
	/*
	int items_acked = 0;
	MPI_Irecv(&items_acked, 1, MPI_INT,
		recvfrom[rank - 1][1], MPI_ANY_TAG,
		MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	assert(items_acked >= 0);
	if (items_acked != ITEM_COUNT)
		cout << "Only acked " << items_acked << endl;
	*/
}

void rotor_test(int size, int rank) {
	
	int Nslots = run_us / slot_us; // total number of slots

	if (rank == 0) { // "sync" node
		
		// wait for comm nodes to init buffers:
		MPI_Barrier(MPI_COMM_WORLD);
		cout << "WARMUP SLOT:" << endl; // debug

		// wait for warmup to finish:
		MPI_Barrier(MPI_COMM_WORLD);
	
		// start RotorLB sync clock:
		int64_t start = get_us(); // global sync start time (us)
		int64_t current = get_us();
		int prev_slot = 0;
		int this_slot = 0;
		int slot = 0; // debug
		while (current - start < run_us) {
			prev_slot = (current - start) / slot_us;
			current = get_us();
			this_slot = (current - start) / slot_us;
			if (this_slot > prev_slot) {
				MPI_Barrier(MPI_COMM_WORLD); // trigger
				// debug:
				slot++;
				cout << "SYNC NODE: slot = " << slot <<
				" started at " << current - start << " us." << endl;
			}
		}

	} else { // communicating nodes
		
		// define the connection patterns: [rank, slot]
		vector<vector<int>> sendto{ { 2, 3 },
                               { 3, 1 },
                               { 1, 2 } };
		vector<vector<int>> recvfrom{ { 3, 2 },
                               { 1, 3 },
                               { 2, 1 } };
		
		// init buffers:
		int * sendbuf = new int[ITEM_COUNT];
		int * recvbuf = new int[ITEM_COUNT];
		// fill buffers:
		for (size_t i = 0; i < ITEM_COUNT; i++) {
			sendbuf[i] = i;
			recvbuf[i] = i;
		}

		// !!! Warm up - have each node do a send and receive !!!
		MPI_Barrier(MPI_COMM_WORLD); // comm nodes ready to warm up	
		rotor_kernel(size, rank, 0, sendbuf, recvbuf, sendto, recvfrom);
		
		// Start 1-hop RotorLB:
		MPI_Barrier(MPI_COMM_WORLD); // comm nodes ready to start RLB
		for (int i = 0; i < Nslots; i++) {
			MPI_Barrier(MPI_COMM_WORLD);
			// waiting on the sync node to trigger...		
			rotor_kernel(size, rank, i, sendbuf, recvbuf, sendto, recvfrom);
			
		}
	}
}

