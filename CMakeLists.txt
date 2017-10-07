cmake_minimum_required(VERSION 3.8)
project(MPISound)

set(CMAKE_CXX_STANDARD 17)

find_package(MPI REQUIRED)

include_directories(${MPI_INCLUDE_PATH})

add_executable(MPISound sources/mpi_to_sound.cpp)
add_executable(send_recv example_send_recv.cpp)
add_library(mpisound SHARED sources/mpi_sound.cpp)

target_link_libraries(send_recv mpisound ${MPI_LIBRARIES} )
