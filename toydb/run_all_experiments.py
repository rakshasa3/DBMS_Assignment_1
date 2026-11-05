#!/usr/bin/env python3

import csv
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

PF_DIR = ROOT / "pflayer"
DATA_DIR = (ROOT / "../data").resolve()

OUTPUT_DIR = ROOT / "buffer_experiments"
PF_DATA_DIR = OUTPUT_DIR / "pf_data"
LOG_DIR = OUTPUT_DIR / "logs"

BUFFER_SIZES = [4, 8, 12, 16]

RESULT_FILE = OUTPUT_DIR / "buffer_results.csv"


def run_command(command, cwd, logfile):
    logfile.parent.mkdir(parents=True, exist_ok=True)

    with open(logfile, "w", encoding="utf-8") as log:
        process = subprocess.run(
            [str(x) for x in command],
            cwd=cwd,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True
        )

    if process.returncode != 0:
        raise RuntimeError(
            f"Command failed. Check {logfile}"
        )


def find_datasets():
    if not DATA_DIR.exists():
        raise FileNotFoundError(
            f"Dataset directory not found: {DATA_DIR}"
        )

    return sorted(
        p for p in DATA_DIR.rglob("*")
        if p.is_file()
        and p.suffix.lower() in (".txt", ".csv")
    )


def safe_name(path):
    return re.sub(
        r"[^A-Za-z0-9_.-]+",
        "_",
        str(path.relative_to(DATA_DIR))
    )


def get_page_count(logfile):
    output = logfile.read_text(
        encoding="utf-8",
        errors="replace"
    )

    match = re.search(
        r"Pages created\s*:\s*(\d+)",
        output,
        re.IGNORECASE
    )

    if not match:
        raise RuntimeError(
            "Page count not found in converter output."
        )

    return int(match.group(1))


def parse_results(logfile):
    lines = logfile.read_text(
        encoding="utf-8",
        errors="replace"
    ).splitlines()

    index = None

    for i, line in enumerate(lines):
        if line.startswith(
            "BufferSize,Policy,WritePercentage,"
        ):
            index = i
            break

    if index is None:
        raise RuntimeError(
            "Benchmark CSV header not found."
        )

    rows = list(csv.DictReader(lines[index:]))

    if not rows:
        raise RuntimeError(
            "No benchmark results were generated."
        )

    return rows


def main():
    OUTPUT_DIR.mkdir(exist_ok=True)
    PF_DATA_DIR.mkdir(exist_ok=True)
    LOG_DIR.mkdir(exist_ok=True)

    datasets = find_datasets()

    if not datasets:
        print("No datasets found.")
        sys.exit(1)

    converter = ROOT / "text_to_pf"
    converter_source = ROOT / "text_to_pf.c"

    benchmark = PF_DIR / "benchmark_pf"

    print(f"Found {len(datasets)} datasets.")

    # Compile the separate converter using existing PF objects.
    print("\nCompiling external text_to_pf converter...")

    objects = [
        PF_DIR / "pf.o",
        PF_DIR / "buf.o",
        PF_DIR / "hash.o"
    ]

    missing = [str(obj) for obj in objects if not obj.exists()]

    if missing:
        run_command(
            ["make"],
            PF_DIR,
            LOG_DIR / "compile_pf.log"
        )

    run_command(
        [
            "gcc",
            "-std=gnu89",
            "-w",
            "-I",
            ROOT,
            "-o",
            converter,
            converter_source,
            *objects
        ],
        ROOT,
        LOG_DIR / "compile_converter.log"
    )

    if not benchmark.exists():
        run_command(
            ["make", "benchmark_pf"],
            PF_DIR,
            LOG_DIR / "compile_benchmark.log"
        )

    results = []

    for dataset in datasets:
        name = safe_name(dataset)

        pf_file = PF_DATA_DIR / f"{name}.data"

        print(f"\nPreparing {dataset.name}")

        loader_log = LOG_DIR / f"{name}_loader.log"

        try:
            run_command(
                [
                    converter,
                    dataset.resolve(),
                    pf_file.resolve()
                ],
                ROOT,
                loader_log
            )

            pages = get_page_count(loader_log)

            print(f"Pages: {pages}")

        except Exception as error:
            print(f"Conversion failed: {error}")

            results.append({
                "Dataset": str(dataset.relative_to(DATA_DIR)),
                "Status": "CONVERSION_FAILED",
                "Error": str(error)
            })

            continue

        for buffer_size in BUFFER_SIZES:
            log_file = LOG_DIR / (
                f"{name}_buffer_{buffer_size}.log"
            )

            print(f"Running buffer size {buffer_size}")

            try:
                run_command(
                    [
                        benchmark,
                        buffer_size,
                        pages,
                        pf_file.resolve()
                    ],
                    PF_DIR,
                    log_file
                )

                rows = parse_results(log_file)

                for row in rows:
                    row["Dataset"] = str(
                        dataset.relative_to(DATA_DIR)
                    )
                    row["Status"] = "OK"
                    row["Error"] = ""

                    results.append(row)

            except Exception as error:
                print(f"Benchmark failed: {error}")

                results.append({
                    "Dataset": str(dataset.relative_to(DATA_DIR)),
                    "BufferSize": buffer_size,
                    "Status": "FAILED",
                    "Error": str(error)
                })

    columns = [
        "Dataset",
        "BufferSize",
        "Policy",
        "WritePercentage",
        "Passes",
        "LogicalReads",
        "LogicalWrites",
        "PhysicalReads",
        "PhysicalWrites",
        "CacheHits",
        "CacheMisses",
        "Status",
        "Error"
    ]

    with open(
        RESULT_FILE,
        "w",
        newline="",
        encoding="utf-8"
    ) as file:
        writer = csv.DictWriter(
            file,
            fieldnames=columns,
            extrasaction="ignore"
        )

        writer.writeheader()
        writer.writerows(results)

    print("\n===================================")
    print("BUFFER EXPERIMENTS COMPLETE")
    print(f"Results: {RESULT_FILE}")
    print(f"Logs: {LOG_DIR}")
    print("===================================")


if __name__ == "__main__":
    main()
