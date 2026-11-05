#!/usr/bin/env python3

from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent

RESULT_DIR = ROOT / "buffer_experiments"
INPUT_FILE = RESULT_DIR / "buffer_results.csv"
PLOT_DIR = RESULT_DIR / "plots"

METRICS = [
    "PhysicalReads",
    "PhysicalWrites",
    "LogicalReads",
    "LogicalWrites",
    "CacheHits",
    "CacheMisses"
]


def main():
    if not INPUT_FILE.exists():
        raise FileNotFoundError(
            "Run run_all_experiments.py first."
        )

    PLOT_DIR.mkdir(exist_ok=True)

    df = pd.read_csv(INPUT_FILE)

    df = df[df["Status"] == "OK"].copy()

    if df.empty:
        raise ValueError("No successful results to plot.")

    for column in [
        "BufferSize",
        "WritePercentage",
        *METRICS
    ]:
        df[column] = pd.to_numeric(
            df[column],
            errors="coerce"
        )

    for dataset in df["Dataset"].unique():

        subset = df[df["Dataset"] == dataset]

        for metric in METRICS:

            for buffer_size in sorted(
                subset["BufferSize"].dropna().unique()
            ):

                current = subset[
                    subset["BufferSize"] == buffer_size
                ]

                pivot = current.pivot_table(
                    index="WritePercentage",
                    columns="Policy",
                    values=metric,
                    aggfunc="mean"
                )

                if pivot.empty:
                    continue

                ax = pivot.plot(
                    kind="line",
                    marker="o",
                    figsize=(9, 5)
                )

                ax.set_title(
                    f"{metric} - {dataset} "
                    f"(Buffer {buffer_size})"
                )

                ax.set_xlabel("Write percentage")
                ax.set_ylabel(metric)
                ax.grid(True)

                plt.tight_layout()

                filename = (
                    f"{Path(dataset).stem}_"
                    f"{metric}_{buffer_size}.png"
                )

                plt.savefig(
                    PLOT_DIR / filename,
                    dpi=150
                )

                plt.close()

        # Cache hit ratio
        subset = subset.copy()

        total = (
            subset["CacheHits"] +
            subset["CacheMisses"]
        )

        subset["CacheHitRatio"] = (
            subset["CacheHits"] /
            total.where(total != 0)
        ) * 100

        for buffer_size in sorted(
            subset["BufferSize"].dropna().unique()
        ):

            current = subset[
                subset["BufferSize"] == buffer_size
            ]

            pivot = current.pivot_table(
                index="WritePercentage",
                columns="Policy",
                values="CacheHitRatio",
                aggfunc="mean"
            )

            if pivot.empty:
                continue

            ax = pivot.plot(
                kind="line",
                marker="o",
                figsize=(9, 5)
            )

            ax.set_title(
                f"Cache hit ratio - {dataset} "
                f"(Buffer {buffer_size})"
            )

            ax.set_xlabel("Write percentage")
            ax.set_ylabel("Cache hit ratio (%)")
            ax.grid(True)

            plt.tight_layout()

            plt.savefig(
                PLOT_DIR /
                f"{Path(dataset).stem}_CacheHitRatio_{buffer_size}.png",
                dpi=150
            )

            plt.close()

    print(f"All plots saved in: {PLOT_DIR}")


if __name__ == "__main__":
    main()
