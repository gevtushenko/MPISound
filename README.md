# MPISound

This project is something an audio profiler of Message Passing Interface. For MPI operation representation MPISound 
use stereo system (left headphone is master rank and right headphone is second process). Therefore only two processes are supported
for this time. 

## Usage
You can specify library when your project compiled or you can use LD_PRELOAD instead (also you need to specify path to MPI library in
mpi_sound.cpp)

```bash
mpic++ -shared -fPIC ../sources/mpi_sound.cpp -o sound.so
LD_PRELOAD='./sound.so' mpirun -np 2 ./send_recv
```

Then some log files will be created (rank_0_output.t). If you run 

```bash
./MPISound
```

it will use the files to create WAV sound file.