#include <iostream>
#include <chrono>
#include <mpi.h>
#include <assert.h>
#include <string.h>
#include <vector>

const int NUM_ITERS = 100;
const int WAIT = 1; // time to wait between sends in microseconds

const int ITEM_COUNT = 1;

//const int ITEM_COUNT = (1024 * 1024 * 64);
//const int ITEM_COUNT = 1; // 4 B
//const int ITEM_COUNT = 12; // 48 B
//const int ITEM_COUNT = 125; // 500 B
//const int ITEM_COUNT = 1250; // 5 kB
//const int ITEM_COUNT = 12500; // 50 kB
//const int ITEM_COUNT = 125000; // 500 kB
//const int ITEM_COUNT = 1250000; // 5 MB
//const int ITEM_COUNT = 12500000; // 50 MB
//const int ITEM_COUNT = 125000000; // 500 MB

void latency_test(int size, int rank);
void cycle_receiver_test(int size, int rank);
void delayed_message_stream_test(int size, int rank);
void throughput_test(int size, int rank);
void throughput_vect_test(int size, int rank);
void allgather_test(int size, int rank);
void broadcast_test(int size, int rank);

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

	//latency_test(size, rank); // ping pong latency between sender & receiver
	cycle_receiver_test(size, rank); // cycle through different receivers	
	//delayed_message_stream_test(size,rank); // send, wait, send, wait, ...
	//throughput_vect_test(size, rank); // sweep over multiple message sizes
	
	// throughput_test(size, rank);
	// allgather_test(size, rank);
	// broadcast_test(size, rank);

	MPI_Finalize();

	return 0;
}

void latency_test(int size, int rank) {
	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {
		int value = 42;

		for (size_t i = 0; i < NUM_ITERS; i++) {
			auto begin = steady_clock::now();
			MPI_Send(&value, 1, MPI_INT, /* dst */ 1, /* tag */ 0, MPI_COMM_WORLD);
			MPI_Recv(&value, 1, MPI_INT, /* source */ 1,
					 MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			auto end = steady_clock::now();

			//cout << "Iter " << i << ": " << duration_cast<microseconds>(end - begin).count() << " us" << endl;
			cout << duration_cast<microseconds>(end - begin).count() << endl;
		}

	} else {
		assert(rank == 1);

		for (size_t i = 0; i < NUM_ITERS; i++) {
			int value = -1;

			MPI_Recv(&value, 1, MPI_INT, /* source */ 0,
				MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			MPI_Send(&value, 1, MPI_INT, /* dst */ 0,
			    /* tag */ 0, MPI_COMM_WORLD);
		}
	}
}

void cycle_receiver_test(int size, int rank) {

	int numints = 262144; // set the message size
	// for reference, 262144 ints = 1 MB messages

	// construct the send/receive buffer:
	int * buf = new int[numints];
	for (int i = 0; i < numints; i++) {
		buf[i] = i;
	}

	int64_t start, stop;
	vector<int> times(NUM_ITERS);

	int Nreceivers = 2;
	vector<int> receivers(Nreceivers);
	for (int i = 0; i < Nreceivers; i++)
		receivers[i] = i + 1; // receiving rank
	
	MPI_Barrier(MPI_COMM_WORLD);

	for (size_t i = 0; i < NUM_ITERS; i++) {

		int current_receiver = receivers[i % Nreceivers];

		if (rank == 0) {

			start = get_us();

			int items_acked = 0;

			MPI_Send(buf, numints, MPI_INT, /* dst */ current_receiver, /* tag */ 0, MPI_COMM_WORLD);
			MPI_Recv(&items_acked, 1, MPI_INT, /* source */ current_receiver,
					 MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			
			stop = get_us();
			times[i] = stop - start; // record the time it took

			assert(items_acked >= 0);

			// poll over the wait period:
			start = get_us();
			stop = get_us();
			while ((stop - start) < WAIT){
				stop = get_us();
			}
		} else if (rank == current_receiver) {
		
			int items_received = 0;
			MPI_Status status;

			MPI_Recv(buf, numints, MPI_INT, /* source */ 0,
				MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			MPI_Get_count(&status, MPI_INT, &items_received);

			MPI_Send(&items_received, 1, MPI_INT, /* dst */ 0,
			    /* tag */ 0, MPI_COMM_WORLD);
		}
	}

	if (rank == 0) {
		// print the timing output to console:
		//cout << "Runs in microseconds:" << endl;
		for (int i = 0; i < NUM_ITERS; i++)
			cout << times[i] << endl;
	}

}


void delayed_message_stream_test(int size, int rank) {
	
	int numints = 262144; // set the message size
	// for reference, 262144 ints = 1 MB messages

	// construct the send/receive buffer:
	int * buf = new int[numints];
	for (int i = 0; i < numints; i++) {
		buf[i] = i;
	}

	int64_t start, stop;
	vector<int> times(NUM_ITERS);
	
	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {

		for (size_t i = 0; i < NUM_ITERS; i++) {
	
			start = get_us();

			int items_acked = 0;

			MPI_Send(buf, numints, MPI_INT, /* dst */ 1, /* tag */ 0, MPI_COMM_WORLD);
			MPI_Recv(&items_acked, 1, MPI_INT, /* source */ 1,
					 MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			
			stop = get_us();
			times[i] = stop - start; // record the time it took

			assert(items_acked >= 0);

			// poll over the wait period:
			start = get_us();
			stop = get_us();
			while ((stop - start) < WAIT){
				stop = get_us();
			}
		}

		// print the timing output to console:
		//cout << "Runs in microseconds:" << endl;
		for (int i = 0; i < NUM_ITERS; i++)
			cout << times[i] << endl;

	} else {
		assert(rank == 1);

		for (size_t i = 0; i < NUM_ITERS; i++) {
			int items_received = 0;
			MPI_Status status;

			MPI_Recv(buf, numints, MPI_INT, /* source */ 0,
				MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			MPI_Get_count(&status, MPI_INT, &items_received);

			MPI_Send(&items_received, 1, MPI_INT, /* dst */ 0,
			    /* tag */ 0, MPI_COMM_WORLD);
		}
	}

}

void throughput_test(int size, int rank) {
	int * buf = new int[ITEM_COUNT];

	/* initialize the message */
	for (size_t i = 0; i < ITEM_COUNT; i++) {
		buf[i] = i;
	}

	int64_t start, stop;
	vector<int> times(NUM_ITERS);
	
	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {

		//auto begin = steady_clock::now();
		for (size_t i = 0; i < NUM_ITERS; i++) {
	
			start = get_us();

			int items_acked = 0;

			MPI_Send(buf, ITEM_COUNT, MPI_INT, /* dst */ 1, /* tag */ 0, MPI_COMM_WORLD);
			MPI_Recv(&items_acked, 1, MPI_INT, /* source */ 1,
					 MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			
			stop = get_us();
			times[i] = stop - start; // record the time it took

			assert(items_acked >= 0);
			if (items_acked != ITEM_COUNT) {
				cout << "Only acked " << items_acked << " instead of " << ITEM_COUNT << endl;
			}
		}
		//auto end = steady_clock::now();
		//cout << "Sent " << ITEM_COUNT << " ints " << NUM_ITERS << " times" << endl;
		//cout << "Runtime: " << duration_cast<microseconds>(end - begin).count() << " us" << endl;

		// print the timing output to console:
		cout << "Runs in microseconds:" << endl;
		for (int i = 0; i < NUM_ITERS; i++)
			cout << times[i] << endl;

	} else {
		assert(rank == 1);

		for (size_t i = 0; i < NUM_ITERS; i++) {
			int items_received = 0;
			MPI_Status status;

			MPI_Recv(buf, ITEM_COUNT, MPI_INT, /* source */ 0,
				MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			MPI_Get_count(&status, MPI_INT, &items_received);

			MPI_Send(&items_received, 1, MPI_INT, /* dst */ 0,
			    /* tag */ 0, MPI_COMM_WORLD);
		}
	}
}

void throughput_vect_test(int size, int rank) {

	int seed = 1;
	int maxpow = 24; // 28
	vector<int> ITEM_COUNT_VECT(maxpow);
	ITEM_COUNT_VECT[0] = seed;
	for (int i = 1; i < maxpow; i++)
		ITEM_COUNT_VECT[i] = ITEM_COUNT_VECT[i-1] * 2;

	int64_t start, stop;
	vector<vector<int>> times(NUM_ITERS);
	for (int i = 0; i < NUM_ITERS; i++) {
		times[i].resize(ITEM_COUNT_VECT.size());
		for (int j = 0; j < (int)ITEM_COUNT_VECT.size(); j++)
			times[i][j] = 0; // write to entire matrix
	}

	for (int ii = 0; ii < (int)ITEM_COUNT_VECT.size(); ii++) {
	
		int * buf = new int[ITEM_COUNT_VECT[ii]]; // allocate the send / receive buffers
		// initialize the message
		for (int i = 0; i < (int)ITEM_COUNT_VECT[ii]; i++) {
			buf[i] = i;
		}
	
		MPI_Barrier(MPI_COMM_WORLD);

		if (rank == 0) {
			for (size_t i = 0; i < NUM_ITERS; i++) {
	
				start = get_us();
				int items_acked = 0;
				MPI_Send(buf, ITEM_COUNT_VECT[ii], MPI_INT, /* dst */ 1, /* tag */ 0, MPI_COMM_WORLD);
				MPI_Recv(&items_acked, 1, MPI_INT, /* source */ 1,
					 MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				stop = get_us();
				times[i][ii] = stop - start; // record the time it took

				//assert(items_acked >= 0);
				//if (items_acked != ITEM_COUNT_VECT[ii]) {
				//	cout << "Only acked " << items_acked << " instead of " << ITEM_COUNT << endl;
				//}
			}
		} else {
			assert(rank == 1);
			for (size_t i = 0; i < NUM_ITERS; i++) {
				
				int items_received = 0;
				MPI_Status status;
				MPI_Recv(buf, ITEM_COUNT_VECT[ii], MPI_INT, /* source */ 0,
					MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				MPI_Get_count(&status, MPI_INT, &items_received);
				MPI_Send(&items_received, 1, MPI_INT, /* dst */ 0,
					/* tag */ 0, MPI_COMM_WORLD);
			}
		}
	}

	if (rank == 0) {
		// print the timing output to console:
		//cout << "Runs in microseconds [iter, send_size]:" << endl;
		for (int i = 0; i < NUM_ITERS; i++) {
			for (int j = 0; j < (int)ITEM_COUNT_VECT.size(); j++)
				cout << times[i][j] << " ";
			cout << endl;
		}
	}
}

void allgather_test(int size, int rank)
{
	int sendbuf[1];
	sendbuf[0] = rank * 1000;

	int * recvbuf = new int[size];

	for (int i = 0; i < size; i++) {
		recvbuf[i] = 0;
	}

	MPI_Barrier(MPI_COMM_WORLD);

	auto before = high_resolution_clock::now();
	MPI_Allgather(&sendbuf, 1, MPI_INT, recvbuf, 1, MPI_INT, MPI_COMM_WORLD);
	auto after = high_resolution_clock::now();

	if (rank == 0) {
		cout << "total time: " << duration_cast<microseconds>(after - before).count() << endl;

		cout << "Received data: [";
		for (int i = 0; i < size; i++) {
			cout << recvbuf[i] << ", ";
		}
		cout << endl;
	}
}

void broadcast_test(int size, int rank)
{
	/*
	 *  In this experiment, the leader sends out a broadcast, and
	 *  every other node, when it receives that message, replies back
	 *  with a unicast message that has its local time.
	 */

	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {
		int64_t * remoteTimes = new int64_t[size];
		memset(remoteTimes, 0, sizeof(int64_t) * size);

		int64_t before = get_us();
		MPI_Bcast(&before, 1, MPI_INT64_T, 0, MPI_COMM_WORLD);

		for (int i = 0; i < size - 1; i++) {
			int64_t remoteTime;  // the time at the remote machine
			MPI_Status status;

			MPI_Recv(&remoteTime, 1, MPI_INT64_T, MPI_ANY_SOURCE,
					 MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			int remoteNode = status.MPI_SOURCE;
			assert(remoteNode < size);
			remoteTimes[remoteNode] = remoteTime;
		}

		int64_t after = get_us(); 

		cout << "Experiment: broadcast_test, N=" << size << endl;
		cout << "Before: " << before << endl;
		
		for (int i = 0; i < size; i++) {
			cout << "remote " << i << ": " << remoteTimes[i] << endl;
		}

		cout << "After: " << after << endl;

		delete [] remoteTimes;

	} else {

		int64_t masterTime = 0;
		MPI_Bcast(&masterTime, 1, MPI_INT64_T, 0, MPI_COMM_WORLD);
		
		int64_t nowtime = get_us();
		MPI_Send(&nowtime, 1, MPI_INT64_T, 0, 0, MPI_COMM_WORLD);
	}
}
