# P4DB - The Case for In-Network OLTP

This is the source code for our (Matthias Jasny, Lasse Thostrup, Tobias Ziegler and Carsten Binnig) published paper at SIGMOD’22: P4DB - The Case for In-Network OLTP. Paper can be found here: [Paper Link](https://www.informatik.tu-darmstadt.de/media/datamanagement/pdf_publications/P4DB_preprint.pdf)

## Abstract

In this paper we present a new approach for distributed DBMSs called P4DB, that uses a programmable switch to accelerate OLTP workloads. The main idea of P4DB is that it implements a transaction processing engine on top of a P4-programmable switch. The switch can thus act as an accelerator in the network, especially when it is used to store and process hot (contended) tuples on the switch. In our experiments, we show that P4DB hence provides significant benefits compared to traditional DBMS architectures and can achieve a speedup of up to 8x.

## Citation

```
@inproceedings{mjasny22,
    author    = {Matthias Jasny and Lasse Thostrup and Tobias Ziegler and Carsten Binnig},
    title     = {P4DB - The Case for In-Network OLTP},
    booktitle = {SIGMOD},
    year      = {2022}
}

```

## Directory structure

- `./src` contains P4DB code for database nodes:
    - one instance which spawns a server + N workers
    - requires DPDK to access NIC

- `./switch_src` contains P4DB code for the switch:
    - control plane in C++
    - P4 code-generator
    - P4DB Firmwares for YCSB, SmallBank, TPC-C (+ Microbenchmarks)

- `./figures` contains R scripts to generate the paper figures
    - `data/` contains CSV files with measurements
    - `out/` contains rendered PDFs

- `./experiments` contains scripts for evaluation
    - Use [distexprunner](https://github.com/mjasny/distexprunner/)



### Build & Run

This is the database node server:

```
meson build
ninja -C build
./build/p4db --help
```
