import json
import sys
import tarfile
from collections import defaultdict

import matplotlib.pyplot as plt

import numpy as np


def plot_matrix(message_matrix, path, title):
    fig, ax = plt.subplots()

    img = ax.imshow(message_matrix)
    cb = fig.colorbar(img)
    cb.set_label(title)
    ax.set_xlabel("Sender Rank")
    ax.set_ylabel("Receiver Rank")
    ax.grid(b=False)

    fig.savefig(path)


def plot_traffic_matrix(traffic_matrix, path):
    plot_matrix(traffic_matrix, path, "Sent Bytes")


def plot_message_matrix(message_matrix, path):
    plot_matrix(message_matrix, path, "Sent Messages")


def plot_message_size_histogram(message_sizes, path):
    fig, ax = plt.subplots()

    values = list(message_sizes.keys())
    weights = list(message_sizes.values())

    ax.hist(values, bins=50, weights=weights)
    ax.set_xlabel("Message Size [B]")
    ax.set_ylabel("Messages Sent")

    fig.savefig(path)


class Phase:
    def __init__(self, n_procs):
        self.traffic_matrix = np.zeros((n_procs, n_procs))
        self.message_matrix = np.zeros((n_procs, n_procs))
        self.message_sizes = defaultdict(lambda: 0)


def main():
    trace_archive = sys.argv[1]
    phases = []

    with tarfile.open(trace_archive, "r:*") as tar:
        for member in tar.getmembers():
            if not member.isfile() or not member.name.endswith(".json"):
                continue

            with tar.extractfile(member) as f:
                trace = json.loads(f.read().decode("utf-8"))
                n_procs = trace["n_procs"]
                rank = trace["rank"]

                if not phases:
                    phases = [Phase(n_procs) for _ in range(trace["n_phases"])]

                for i, phase in enumerate(trace["phases"]):
                    phases[i].traffic_matrix[rank] = phase["tx_bytes"]
                    phases[i].message_matrix[rank] = phase["tx_messages"]

                    for item in phase["tx_message_sizes"]:
                        phases[i].message_sizes[item["message_size"]] \
                            += item["frequency"]

    for i, phase in enumerate(phases):
        plot_traffic_matrix(phase.traffic_matrix,
                            "traffic_matrix-{0}.pdf".format(i))
        plot_message_matrix(phase.message_matrix,
                            "message_matrix-{0}.pdf".format(i))
        plot_message_size_histogram(phase.message_sizes,
                                    "message_size_histogram-{0}.pdf".format(i))


if __name__ == "__main__":
    main()
