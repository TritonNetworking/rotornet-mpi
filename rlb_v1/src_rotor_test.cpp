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

//const int ITEM_COUNT = (1024 * 1024 * 64); // # ints = 268435456 bytes
const int ITEM_COUNT = 1;

const int64_t run_us = 20099999; // total runtime, microseconds
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

	int size, rank;

	assert(steady_clock::is_steady);

	MPI_Init(&argc, &argv);

	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	rotor_test(size, rank);

	MPI_Finalize();

	return 0;
}

void rotor_kernel(int size, int rank, int slot, int Nmatch, int * sendbuf, int * recvbuf, vector<vector<int>> sendto, vector<vector<int>> recvfrom) {

	int64_t slot_start = get_us(); // slot start time (us)

	MPI_Request r_handle; // receive handle
	MPI_Request s_handle; // send handle
			
	// start a non-blocking receive:
	MPI_Irecv(recvbuf, ITEM_COUNT, MPI_INT,
		/* src */ recvfrom[rank - 1][slot % Nmatch], MPI_ANY_TAG,
		MPI_COMM_WORLD, &r_handle);

	// start a non-blocking send:
	MPI_Isend(sendbuf, ITEM_COUNT, MPI_INT,
		/* dst */ sendto[rank - 1][slot % Nmatch], /* tag */ 0,
		MPI_COMM_WORLD, &s_handle);

	// debug (slow print statment):
	//cout << "Rank " << rank << " started sending to rank " <<
	//	sendto[rank - 1][slot] << endl;

	// poll for receive complete
	int recv_done = 0;
	MPI_Status status;
	int items_received = 0;
	while (recv_done == 0) {
		MPI_Test(&r_handle, &recv_done, &status);
		if (recv_done == 1) {
			// get how many ints were received:
			MPI_Get_count(&status, MPI_INT, &items_received);
			// block on ACK send:
			
			//cout << "rank " << rank << ": received " << items_received << " ints, sending ACK to rank 0" << endl;
			
			// cout << slot << " " << rank << " " << get_us() - slot_start << endl;

			MPI_Send(&items_received, 1, MPI_INT,
				0, 0, MPI_COMM_WORLD);
		}
	}

	// debug (slow print statements):
	// poll for receive complete and sent complete
	/*int send_done = 0;
	while ((recv_done == 0) || (send_done == 0)) {
		if (recv_done == 0) {
			MPI_Test(&r_handle, &recv_done,
				&status);
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
	}*/
}

void rotor_test(int size, int rank) {
	
	int Nslots = run_us / slot_us; // total number of slots

	if (rank == 0) { // "sync" node
		
		cout << "slot_us = " << slot_us << endl;

		// wait for comm nodes to init variables and buffers:
		MPI_Barrier(MPI_COMM_WORLD);
		
		int64_t start_wu = get_us();
		// wait for warm up ACKs with blocking receive:
		int i1;
		for (int i = 0; i < size - 1; i++)
			MPI_Recv(&i1, 1, MPI_INT, i + 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		// warmup finished
		MPI_Barrier(MPI_COMM_WORLD);
		cout << endl << "Warmup finished in " << get_us() - start_wu << " us." << endl << endl;
	
		// init variables
		int prev_slot = 0;
		int this_slot = 0;
		int slot = -1; // slot indexed from 0
		vector<vector<int>> items_acked(size - 1); // [comm_node, slot]
		vector<vector<int>> times_acked(size - 1); // [comm_node, slot]
		for (int i = 0; i < size - 1; i++) {
			items_acked[i].resize(Nslots);
			times_acked[i].resize(Nslots);
		}
		vector<MPI_Request> r_handles(size - 1); // vector of receive handles
		int Numacked;
		vector<int> recv_done(size - 1);
		
		// start RotorLB sync clock:
		int64_t start = get_us(); // global sync start time (us)
		int64_t current = get_us();
		while (current - start < run_us) {
			prev_slot = (current - start) / slot_us;
			current = get_us();
			this_slot = (current - start) / slot_us;
			if (this_slot > prev_slot) {
				MPI_Barrier(MPI_COMM_WORLD); // trigger slot start
				slot++;
				// debug:
				//cout << "SYNC NODE: slot = " << slot <<
				//" started at " << current - start << " us." << endl;

				// wait for ACKs from each comm node.
				// first, open non-blocking recvs
				for (int i = 0; i < (size - 1); i++) {
					MPI_Irecv(&items_acked[i][slot], 1, MPI_INT,
						i + 1, MPI_ANY_TAG, MPI_COMM_WORLD, &r_handles[i]);
				}

				// next, poll for ACK times
				Numacked = 0;
				for (int i = 0; i < size - 1; i++)
					recv_done[i] = 0;
				
				//cout << "SYNC NODE: started polling " << get_us() - current << " us into slot..." << endl;
	
				while (Numacked < size - 1) {
					for (int i = 0; i < (size - 1); i++) {
						if (recv_done[i] == 0) {
							MPI_Test(&r_handles[i], &recv_done[i], MPI_STATUS_IGNORE);
							if (recv_done[i] == 1) {
								times_acked[i][slot] = get_us() - current;
								Numacked++;
								//cout << "   ACK " << Numacked << ", received from rank " << i+1 << " at " << times_acked[i][slot] << " us into the slot." << endl;
							}
						}
					}
				}
			}
		}

		// print the timing output to console:
		cout << "Integers ACKed [rank, slot]:" << endl;
		for (int i = 0; i < (size - 1); i++) {
			cout << "rank " << i + 1 << ": ";
			for (int j = 0; j < Nslots; j++)
				cout << items_acked[i][j] << " ";
			cout << endl;
		}
		cout << endl << "Times ACKed (relative to slot start) [rank, slot]:" << endl;
		for (int i = 0; i < (size - 1); i++) {
			cout << "rank " << i + 1 << ": ";
			for (int j = 0; j < Nslots; j++)
				cout << times_acked[i][j] << " ";
			cout << endl;
		}
		cout << endl;

	} else { // communicating nodes
		
		// define the matchings: [rank, slot]
		int Nmatch = size - 2; // number of matchings
		
		vector<vector<int>> sendto(size - 1);
		vector<vector<int>> recvfrom(size - 1);
		for (int i = 0; i < size - 1; i++) {
			sendto[i].resize(Nmatch);
			recvfrom[i].resize(Nmatch);
		}

		// built-in shift-based connection generation:
		vector<int> base_vect(size - 1);
		for (int i = 0; i < size - 1; i++)
			base_vect[i] = i + 1;
		rotate(base_vect.begin(), base_vect.begin() + Nmatch, base_vect.end());
		
		for (int i = 0; i < size - 1; i++) {
			rotate(base_vect.begin(), base_vect.begin() + 1, base_vect.end());
			for (int j = 0; j < Nmatch; j++)
				sendto[i][j] = base_vect[j + 1];
		}
		for (int i = 0; i < size - 1; i++)
			for (int j = 0; j < Nmatch; j++)
				recvfrom[i][j] = sendto[i][Nmatch - 1 - j];
			

		// read connecitons from files:
		/*ifstream input1("sendto_" + to_string(size - 1) + ".txt");
		if (input1.is_open()){
			for (int i = 0; i < size - 1; i++) {
    				string line;
    				getline(input1, line);
    				stringstream stream(line);
				for (int j = 0; j < Nmatch; j++)
					stream >> sendto[i][j];
			}
		}		
		ifstream input2("recvfrom_" + to_string(size - 1) + ".txt");
		if (input2.is_open()){
			for (int i = 0; i < size - 1; i++) {
    				string line;
    				getline(input2, line);
    				stringstream stream(line);
				for (int j = 0; j < Nmatch; j++)
					stream >> recvfrom[i][j];
			}
		}*/
			
		// hardcoded connections:
		/*vector<vector<int>> sendto{ { 2, 3 },
                               { 3, 1 },
                               { 1, 2 } };
		vector<vector<int>> recvfrom{ { 3, 2 },
                               { 1, 3 },
                               { 2, 1 } };
		//
		vector<vector<int>> sendto{
				{ 2, 3, 4 },
				{ 3, 4, 1 },
				{ 4, 1, 2 },
				{ 1, 2, 3 } };
		vector<vector<int>> recvfrom{
				{ 4, 3, 2 },
				{ 1, 4, 3 },
				{ 2, 1, 4 },
				{ 3, 2, 1 } };*/
		
		// debug connections:
		/*if (rank == 1) {
			cout << "size of `sendto` = " << sendto.size() << " x " << sendto[0].size() << endl;
			cout << "sendto = " << endl;
			for (int i = 0; i < size-1; i++) {
				for (int j = 0; j < Nmatch; j++)
					cout << sendto[i][j] << " ";
				cout << endl;
			}
		}*/


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
		rotor_kernel(size, rank, 0, Nmatch, sendbuf, recvbuf, sendto, recvfrom);
		
		// Start 1-hop RotorLB:
		MPI_Barrier(MPI_COMM_WORLD); // comm nodes ready to start RLB
		for (int slot = 0; slot < Nslots; slot++) {
			// waiting on the sync node to trigger:
			MPI_Barrier(MPI_COMM_WORLD);
			rotor_kernel(size, rank, slot, Nmatch, sendbuf, recvbuf, sendto, recvfrom);			
		}
	}
}

