# CSL3050 Database Systems Assignment

*B24CS1122 - Krishna Mishra*
*B24CS1124 - Sai Akshar Tadimeti*

## 1. Project Overview

This project extends the ToyDB database system to implement and evaluate three database components:

1. **Paged File (PF) Layer** – Implements page buffering, LRU and MRU page replacement, dirty-page tracking, configurable buffer pool size, and I/O statistics.
2. **Record Management (RM) Layer** – Implements variable-length record storage using a slotted-page structure, including insertion, deletion, retrieval, compaction, and sequential scanning.
3. **Access Method (AM) Layer** – Implements B+ tree indexing and compares incremental index construction, post-population index construction, and bottom-up bulk loading.

The project also includes benchmark programs and Python scripts for generating experimental results and plots.

---

## 2. Directory Structure

The project is organized as follows:

```text
DBMS_Assignment_1/
│
├── data/
│   ├── student.txt
│   ├── courses.txt
│   └── other supplied datasets
├── docs/
│   ├── am.ps
│   ├── pf.ps
│   └── project_desc.pdf
│
└── toydb/
    │
    ├── pflayer/
    │   ├── pf.c
    │   ├── pf.h
    │   ├── buf.c
    │   ├── hash.c
    │   ├── benchmark_pf.c
    │   └── Makefile
    │
    ├── rmlayer/
    │   ├── rm.c
    │   ├── rm.h
    │   ├── space_utilization.c
    │   └── Makefile
    │
    ├── amlayer/
    │   ├── am.c
    │   ├── am.h
    │   ├── amscan.c
    │   ├── aminsert.c
    │   ├── ambulkload.c
    │   ├── student_store.c
    │   ├── student_store.h
    │   ├── benchmark_student_bulkload.c
    │   ├── benchmark_student_queries.c
    │   └── makefile
    │
    ├── testam/
    │
    ├── buffer_experiments/
    │
    ├── text_to_pf.c
    ├── run_all_experiments.py
    ├── plot_experiment_results.py
    └── README.md
```

---

## 3. Requirements and Dependencies

The following tools are required:

- GCC compiler
- GNU Make
- Python 3
- Python packages:
  - pandas
  - matplotlib

Install the Python dependencies using:

```bash
python3 -m pip install pandas matplotlib
```

On Ubuntu or Debian, GCC and Make can be installed using:

```bash
sudo apt update
sudo apt install build-essential python3 python3-pip
```

---

## 4. PF Layer – Page Buffering

### Description

The PF layer manages fixed-size pages and provides buffering to reduce physical disk I/O.

The implementation includes:

- LRU page replacement.
- MRU page replacement.
- Replacement policy selection when opening a file.
- Runtime-configurable buffer pool size.
- Dirty-page tracking.
- Explicit dirty marking through `PF_MarkDirty()`.
- Logical read and write statistics.
- Physical read and write statistics.
- Cache hit and cache miss statistics.

The replacement policy can be selected using:

```c
PF_OpenFileWithPolicy(filename, PF_LRU);
```

or:

```c
PF_OpenFileWithPolicy(filename, PF_MRU);
```

The buffer pool size can be configured using:

```c
PF_SetBufferSize(buffer_size);
```

This should be called before buffer frames are allocated.

### Benchmark Description

`benchmark_pf.c` evaluates the effect of different read/write workload mixtures on the buffer manager.

The benchmark tests:

| Write Percentage | Read Percentage |
|---:|---:|
| 0% | 100% |
| 25% | 75% |
| 50% | 50% |
| 75% | 25% |
| 100% | 0% |

Both LRU and MRU are evaluated.

The benchmark collects:

- Logical reads
- Logical writes
- Physical reads
- Physical writes
- Cache hits
- Cache misses

It also supports different buffer pool sizes.

### Compile PF Layer

From the `toydb/` directory:

```bash
cd pflayer
make clean
make benchmark_pf
cd ..
```

To build the PF library object separately:

```bash
make -C pflayer pflayer.o
```

### Run PF Experiments

First, ensure that the dataset directory exists:

```text
assignment/
├── data/
└── toydb/
```

Run:

```bash
python3 run_all_experiments.py
```

The experiment script:

1. Finds text and CSV datasets in the `data/` directory.
2. Converts them into PF-format files.
3. Runs the PF benchmark with buffer pool sizes of 4, 8, 12, and 16 pages.
4. Evaluates both LRU and MRU.
5. Tests the different read/write workload mixtures.
6. Saves the results in CSV format.

The generated results are stored in:

```text
buffer_experiments/
├── buffer_results.csv
├── pf_data/
└── logs/
```

### Generate PF Graphs

After the experiments have completed, run:

```bash
python3 plot_experiment_results.py
```

The graphs are saved in:

```text
buffer_experiments/plots/
```

The X-axis represents the write percentage of the workload, while the Y-axis represents the selected performance statistic.

---

## 5. RM Layer – Slotted-Page Record Management

### Description

The RM layer implements variable-length record storage using a slotted-page structure.

Each page contains:

- A page header.
- A slot directory.
- Variable-length record data.
- Free-space information.

The page header maintains:

- Number of slots.
- Offset of the free-space region.
- Available free bytes.

Each slot stores the offset and length of its corresponding record.

The implementation supports:

| Operation | Function |
|---|---|
| Create record file | `RM_CreateFile()` |
| Destroy record file | `RM_DestroyFile()` |
| Open record file | `RM_OpenFile()` |
| Close record file | `RM_CloseFile()` |
| Insert record | `RM_InsertRecord()` |
| Delete record | `RM_DeleteRecord()` |
| Retrieve record | `RM_GetRecord()` |
| Compact page | `RM_CompactPage()` |
| Start sequential scan | `RM_OpenScan()` |
| Retrieve next record | `RM_GetNextRecord()` |
| Close sequential scan | `RM_CloseScan()` |

Records are identified using a Record ID (RID), consisting of a page number and a slot number.

Deleted slots are marked as tombstones and can be reused. Page compaction helps eliminate fragmentation within the page.

### Compile RM Layer

From the `toydb/` directory:

```bash
cd rmlayer
make clean
make
cd ..
```

This builds:

- `rm.o`
- `rmlayer.o`
- `space_utilization`

### Run the Space Utilization Experiment

Run from the `rmlayer/` directory:

```bash
cd rmlayer
./space_utilization
cd ..
```

The program loads `courses.txt`, inserts the records into a slotted-page file, and compares the allocated space against static fixed-length record management.

The static management experiment considers maximum record lengths of:

- 128 bytes
- 256 bytes
- 512 bytes

The program prints a comparison table containing allocated storage and space utilization.

**Note:** The current space-utilization driver uses `courses.txt`. The assignment also requires the slotted-page system to manage Student data. A separate Student RM test should be added to demonstrate Student record insertion, deletion, and sequential scanning through the RM interface.

---

## 6. AM Layer – B+ Tree Indexing

### Description

The AM layer implements B+ tree indexing on top of the PF layer.

The Student dataset is indexed using the `roll_no` field.

Three construction techniques are evaluated:

1. Incremental insertion.
2. Index construction after the Student data file has been populated.
3. Bottom-up bulk loading.

The bulk-loading implementation sorts entries and constructs leaf and internal index levels from the bottom up.

The bulk loader supports configurable fill factors. The benchmark evaluates 100% and 80% fill factors.

### Important Source Files

| File | Description |
|---|---|
| `am.c` | Main AM interface |
| `aminsert.c` | B+ tree insertion |
| `amscan.c` | Index scan operations |
| `amsearch.c` | Index search operations |
| `ambulkload.c` | Bottom-up bulk-loading implementation |
| `student_store.c` | Student record storage utilities |
| `build_student_index_incremental.c` | Incremental index construction |
| `build_student_index.c` | Index construction after populating the Student file |
| `benchmark_student_bulkload.c` | Index construction performance comparison |
| `benchmark_student_queries.c` | Query performance comparison |

### Compile AM Layer

From the `toydb/` directory:

```bash
cd amlayer
make clean
make
cd ..
```

This builds the AM library and the associated benchmark and index-building executables.

### 6.1 Incremental Index Construction

This approach starts with an empty Student data file and an empty B+ tree index.

Each Student record is read, stored, and immediately inserted into the index.

Run:

```bash
cd amlayer
./build_student_index_incremental ../../data/student.txt
cd ..
```

The program reports the number of indexed records, elapsed CPU time, and PF I/O statistics.

### 6.2 Index Construction After Populating the Student File

This approach first creates the Student data file and stores all records.

After the data file has been populated, an empty B+ tree index is created. The program scans the Student file and inserts each roll number into the index.

Run:

```bash
cd amlayer
./build_student_index ../../data/student.txt
cd ..
```

The program reports the construction time and PF I/O statistics.

### 6.3 Bulk-Loading Benchmark

The bulk-loading benchmark compares:

- Incremental insertion.
- Bulk loading with 100% fill factor.
- Bulk loading with 80% fill factor.

Run:

```bash
cd amlayer
./benchmark_student_bulkload
cd ..
```

The benchmark creates the following index files:

```text
idx_student_inc.1
idx_student_bulk100.2
idx_student_bulk80.3
```

The program reports:

- Number of index pages.
- Logical reads.
- Logical writes.
- Physical reads.
- Physical writes.
- Cache hits.
- Cache misses.

The benchmark uses the Student roll number as a fixed-width character key.

### 6.4 Query Performance Benchmark

The query benchmark compares equality searches on the Student roll number using four index files:

| Index File | Construction Method |
|---|---|
| `idx_student_inc.1` | Incremental insertion |
| `idx_student_bulk100.2` | Bulk loading, 100% fill |
| `idx_student_bulk80.3` | Bulk loading, 80% fill |
| `student.0` | Post-population index construction |

First, generate the three benchmark indexes:

```bash
cd amlayer
./benchmark_student_bulkload
```

Then generate the post-population index:

```bash
./build_student_index ../../data/student.txt
```

Finally, run:

```bash
./benchmark_student_queries ../../data/student.txt
cd ..
```

The benchmark measures:

- Query execution time.
- Total logical page accesses.
- Average logical page accesses per query.
- Cache hits and misses.
- Successful and failed searches.

Logical page accesses represent PF logical read requests. They are not the same as unique pages accessed or physical disk reads.

**Benchmark note:** The current query benchmark loop is configured to run 100 queries. For the final experiment, increase this to a larger workload or iterate over all loaded Student keys, and use the same query set for every index. The `num_keys` argument is already available in the query function.

---

## 7. Recommended Execution Order

For a clean run, use the following sequence.

### Step 1: Build PF

```bash
cd pflayer
make clean
make benchmark_pf
cd ..
```

### Step 2: Build RM

```bash
cd rmlayer
make clean
make
cd ..
```

### Step 3: Build AM

```bash
cd amlayer
make clean
make
cd ..
```

### Step 4: Run PF Experiments

```bash
python3 run_all_experiments.py
python3 plot_experiment_results.py
```

### Step 5: Run RM Space Utilization

```bash
cd rmlayer
./space_utilization
cd ..
```

### Step 6: Run AM Construction Benchmarks

```bash
cd amlayer
./benchmark_student_bulkload
./build_student_index ../../data/student.txt
```

### Step 7: Run AM Query Benchmark

```bash
./benchmark_student_queries ../../data/student.txt
cd ..
```

---

## 8. Experimental Results

The project produces the following types of results:

### PF Layer

- Buffer pool size comparison.
- LRU versus MRU comparison.
- Logical and physical I/O comparison.
- Read/write workload mixture comparison.
- Cache hit and miss comparison.
- Performance graphs.

### RM Layer

- Slotted-page storage utilization.
- Static fixed-length storage utilization.
- Comparison across different maximum record lengths.

### AM Layer

- Incremental index construction statistics.
- Post-population index construction statistics.
- Bulk-loading statistics at different fill factors.
- Query execution time.
- Logical page accesses per query.
- Cache performance.

---

## 9. Notes and Limitations

1. The supplied `data/` directory must be extracted beside `toydb/`.
2. Several programs use relative dataset paths, so run them from the directories specified in this README.
3. The PF buffer size must be configured before buffer frames are allocated.
4. Run the AM construction programs before running the query benchmark, since the query benchmark expects the index files to exist.
5. The RM space-utilization experiment currently uses `courses.txt`.
6. The AM query benchmark currently runs 100 queries. Increase the query workload before collecting final comparison results.
7. The bulk-loading benchmark uses synthetic page and slot references for its benchmark entries. Its construction statistics compare index-building costs, but those references should not be treated as verified RIDs into the actual Student data file.
8. Logical page accesses count logical PF requests. Physical reads and writes represent disk-level operations and should be reported separately.

---

## 10. Conclusion

The project provides implementations and experiments for the PF, RM, and AM layers of ToyDB.

The PF layer evaluates buffer replacement and I/O behavior, the RM layer demonstrates slotted-page record storage, and the AM layer evaluates multiple B+ tree construction strategies and their query performance.

The benchmark programs and plotting scripts provide experimental evidence for comparing the different implementation choices.
