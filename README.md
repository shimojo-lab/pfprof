# oxton

A PERUSE-based MPI tracer

## Requirements

- cmake 3.1+
- g++
- Open MPI 2.0.1+ built with PERUSE

## How to build

```
$ cmake  -DBUILD_SHARED_LIBS=ON .
$ make
```

## How to use

```
mpirun -x LD_PRELOAD=<path/to/liboxton.so> <path/to/app>
```
