#include <iostream>
#include <thread>
#include <chrono>
#include <mpi.h>

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    int rank;

    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int size = 1024;
    int number[size];

    int max_step = 3;

    for(int step = 0; step < max_step; ++step) {
        if(rank == 0) {
            MPI_Send(&number, size, MPI_INT, 1, 0, MPI_COMM_WORLD);
            std::this_thread::sleep_for(0.1s);
        }
        else if(rank == 1) {
            std::this_thread::sleep_for(1s);
            MPI_Recv(&number, size, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    MPI_Finalize();
    return 0;
}