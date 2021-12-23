# pmem_bench

A set of PMEM benchmarks.

## Dependencies

PMDK
```
spack install pmdk
spack load pmdk
```

## Building

```
cd /path/to/pmem_bench
mkdir build
cd build
cmake ../
make -j8
```

## Test Environment Setup

To format a PMEM device in devdax mode:
```
ndctl create-namespace -m devdax
ndctl enable-namespace namespace0.0
ndctl list -N -n namespace0.0 #Check if activated
```
You can now call mmap on "/dev/dax0.0"

## Single-Producer-Single-Consumer

* One thread appends data to a RAM ring buffer
* One thread consumes the RAM ring buffer and persists to PMEM asynchronously after a configurable
  percentage of RAM is reached
* Can the production rate match consumption rate?

```
cd /path/to/pmem_bench
cd build
sudo ./test_spsc [pmem_path] [pmem_size_gb] [ram_size_gb] [cpu1] [cpu2] [max_iter] [thresh_min] [thresh_max] [type_size_bytes]
```