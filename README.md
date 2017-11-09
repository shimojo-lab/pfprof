# pfprof

A PERUSE-based MPI profiler

## Requirements

- cmake 3.1+
- g++
- Open MPI 2.0.1+ built with PERUSE

## How to build

```
$ cmake .
$ make
```

## How to use

Linux:

```
mpirun -x LD_PRELOAD=<path/to/libpfprof.so> <path/to/app>
```

macOS:

```
mpirun -x DYLD_INSERT_LIBRARIES=<path/to/libpfprof.dylib> -x DYLD_FORCE_FLAT_NAMESPACE=YES <path/to/app>
```
