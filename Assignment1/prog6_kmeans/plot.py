import os

import numpy as np
import matplotlib.pyplot as plt
from sklearn.decomposition import PCA
from os.path import join

root = '/home/drew/Desktop/Parallel Computing-CS 149/CS149-Learning/Assignment1/prog6_kmeans'

START_LOGFILE = join(root, "start.log")
END_LOGFILE = join(root, "end.log")
START_PLOTFILE = join(root, "start.png")
END_PLOTFILE = join(root, "end.png")

for title, logfile, plotfile in [("Start", START_LOGFILE, START_PLOTFILE), ("End", END_LOGFILE, END_PLOTFILE)]:

    assert os.path.exists(logfile), "Log file doesn't exist, try running the program."

    with open(logfile) as f:
        # Read header
        M, N, K = f.readline().split(',')
        M, N, K = int(M), int(N), int(K)

        # Read data
        data = []
        cluster_assignments = []
        cluster_centroids = []

        for line in f.readlines():
            prefix = line.split(':')[0].split(' ')[0]
            if prefix == "Example":
                cluster_assignments.append(line.split(':')[0].split(' ')[-1])
                datapoint = line.split(':')[1].strip().split(' ')
                data.append(np.asarray(datapoint, dtype=float))
                line = f.readline()
            elif prefix == "Centroid":
                centroid = line.split(':')[1].strip().split(' ')
                cluster_centroids.append(np.asarray(centroid, dtype=float))

    # Main data containers
    data = np.stack(data)
    cluster_assignments = np.asarray(cluster_assignments, dtype=int)
    cluster_centroids = np.stack(cluster_centroids)

    # Dimensionality reduction so we can plot the clusters
    pca = PCA(n_components=2)
    pca.fit(data)

    data_2d = pca.transform(data)
    cluster_centroids_2d = pca.transform(cluster_centroids)

    plt.subplots(figsize=(10,8))
    plt.scatter(data_2d[:,0], data_2d[:,1], c=cluster_assignments)
    plt.scatter(cluster_centroids_2d[:,0], cluster_centroids_2d[:,1], c='r', marker='*', edgecolors='black', s=1000)
    plt.title(f"K-Means: {title}")

    plt.savefig(plotfile)