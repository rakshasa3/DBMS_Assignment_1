import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("buffer_results.csv")

metrics = [
    "PhysicalReads",
    "PhysicalWrites",
    "CacheHits",
    "CacheMisses"
]

for metric in metrics:

    plt.figure(figsize=(8, 5))

    for policy in ["LRU", "MRU"]:

        data = df[df["Policy"] == policy]

        plt.plot(
            data["WritePercentage"],
            data[metric],
            marker="o",
            label=policy
        )

    plt.xlabel("Write Query Percentage")
    plt.ylabel(metric)

    plt.title(metric + " Comparison: LRU vs MRU")

    plt.legend()
    plt.grid(True)

    plt.savefig(metric + "_comparison.png")

    plt.show()
