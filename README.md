# oxton

A PERUSE-based MPI tracer

## Requirements

- g++
- [OTF2](http://www.vi-hps.org/projects/score-p) 1.4+
- MPI implementation that supports PERUSE

## How to build

```
$ cmake  -DBUILD_SHARED_LIBS=ON .
$ make
```

## How to use

```
mpirun -x LD_PRELOAD=<path/to/liboxton.so> <path/to/app>
```

Traces will be stored under `trace/` in OTF2 format.
