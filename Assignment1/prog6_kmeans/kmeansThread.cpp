#include <algorithm>
#include <vector>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <mutex>   

#include "CycleTimer.h"

using namespace std;

typedef struct {
    // Control work assignments
    int start, end;

    // Shared by all functions
    double *data;
    double *clusterCentroids;
    int *clusterAssignments;
    double *currCost;
    int M, N, K;

    // Thread-specific
    int threadId;
    int numThreads;
} WorkerArgs;

constexpr int MAX_THREADS = 24;


/**
 * Checks if the algorithm has converged.
 * 
 * @param prevCost Pointer to the K dimensional array containing cluster costs 
 *    from the previous iteration.
 * @param currCost Pointer to the K dimensional array containing cluster costs 
 *    from the current iteration.
 * @param epsilon Predefined hyperparameter which is used to determine when
 *    the algorithm has converged.
 * @param K The number of clusters.
 * 
 * NOTE: DO NOT MODIFY THIS FUNCTION!!!
 */
static bool stoppingConditionMet(double *prevCost, double *currCost,
                                 double epsilon, int K) {
    for (int k = 0; k < K; k++) {
        if (abs(prevCost[k] - currCost[k]) > epsilon)
        return false;
    }
    return true;
}

/**
 * Computes L2 distance between two points of dimension nDim.
 * 
 * @param x Pointer to the beginning of the array representing the first
 *     data point.
 * @param y Poitner to the beginning of the array representing the second
 *     data point.
 * @param nDim The dimensionality (number of elements) in each data point
 *     (must be the same for x and y).
 */
double dist(double *x, double *y, int nDim) {
    double accum = 0.0;
    for (int i = 0; i < nDim; i++) {
        accum += pow((x[i] - y[i]), 2);
    }
    return sqrt(accum);
}

/**
 * Assigns each data point to its "closest" cluster centroid.
 */
void computeAssignments(WorkerArgs *const args) {
    double *minDist = new double[args->M];
    
    // Initialize arrays
    for (int m =0; m < args->M; m++) {
        minDist[m] = 1e30;
        args->clusterAssignments[m] = -1;
    }

    // Assign datapoints to closest centroids
    for (int k = args->start; k < args->end; k++) {
        for (int m = 0; m < args->M; m++) {
            double d = dist(
                &args->data[m * args->N],
                &args->clusterCentroids[k * args->N], 
                args->N
            );
            if (d < minDist[m]) {
                minDist[m] = d;
                args->clusterAssignments[m] = k;
            }
        }
    }

    free(minDist);
}

void computeAssignmentsThreadKernel_w_mutex(
    WorkerArgs *const args, double *minDist, std::mutex &mtx) {
    int k = args->threadId;
    // Assign datapoints to closest centroids
    for (int m = 0; m < args->M; m++) {
        double d = dist(
            &args->data[m * args->N],
            &args->clusterCentroids[k * args->N], 
            args->N
        );
        mtx.lock();
        if (d < minDist[m]) {
            minDist[m] = d;
            args->clusterAssignments[m] = k;
        }
        mtx.unlock();
    }
}

void computeAssignmentsThreads_1(WorkerArgs &args) {
/*
    Serial函数主要执行了两层循环，k遍历所有的聚类，m遍历所有的点，都每个点找到距离最近的聚类中心。
    由于聚类的数量并不多，可以去掉最外层循环，改用多线程的方式，每个线程负责计算所有点到某个聚类
    中心的距离。
    写下多线程计算所有点到聚类中心距离的算法,由于K个线程都要访问minDist，访问时加锁：
*/
    std::thread threads[MAX_THREADS];
    WorkerArgs threadArgs[MAX_THREADS];
    double *minDist=new double[args.M];
    int step=args.M/args.numThreads;
    std::mutex mtx;

    // Initialize arrays
    for (int m = 0; m < args.M; m++) {
        minDist[m] = 1e30;
        args.clusterAssignments[m] = -1;
    }
    // Initialize threads
    for (int i = 0; i < args.K; i++) {
        threadArgs[i] = args;
        threadArgs[i].threadId = i;
    }
    for (int i = 0; i < args.K; i++) 
        threads[i] = std::thread(
            computeAssignmentsThreadKernel_w_mutex, 
            &threadArgs[i], minDist, ref(mtx)
        );
    for (int i = 0; i < args.K; i++)
        threads[i].join();

    free(minDist);
}

void computeAssignmentsThreadKernel_wo_mutex(
    WorkerArgs *const args, double *minDist) {
    int k = args->threadId;
    for (int m = 0; m < args->M; m++) {
        minDist[k*m]= dist(
            &args->data[m * args->N],
            &args->clusterCentroids[k * args->N], 
            args->N
        );
    }
}

void computeAssignmentsThreads_2(WorkerArgs &args){
/*
    采用空间换时间的思想，创建一个double myDist[K * M]，存每个点到每个聚类中心的距离, 
    每个线程同样对应一个单独的聚类中心。即，对于线程k，其计算的点m到聚类中心k的距离存在
    myDist[k * m]处，
    这样不同的线程对应了不同的内存空间，所以不需要加锁。所有线程都计算完毕后，再比较出每
    个点最近的聚类中心。
*/
    std::thread workThread[MAX_THREADS];
    WorkerArgs threadArg[MAX_THREADS];
    double *myDist=new double[args.K*args.M];

    for(int i = 0; i < args.K; i++){
        threadArg[i]=args;
        threadArg[i].threadId=i;
    }
    for(int i = 0; i < args.K; i++)
        workThread[i]=std::thread(
            computeAssignmentsThreadKernel_wo_mutex,
            &threadArg[i], myDist
        );
    
    for(int i = 0; i < args.K; i++)
        workThread[i].join();
    
    for(int i = 0; i < args.M; ++i){
        double mymin=myDist[i];
        int k=0;
        for(int j = 1; j < args.K; ++j){
            if(myDist[j*i]<mymin){
                mymin=myDist[j*i];
                k=j;
            }
        }
        args.clusterAssignments[i] = k;
    }
    free(myDist);
    return ;
}

/**
 * Given the cluster assignments, computes the new centroid locations for
 * each cluster.
 */
void computeCentroids(WorkerArgs *const args) {
    int *counts = new int[args->K];

    // Zero things out
    for (int k = 0; k < args->K; k++) {
        counts[k] = 0;
        for (int n = 0; n < args->N; n++) {
            args->clusterCentroids[k * args->N + n] = 0.0;
        }
    }


  // Sum up contributions from assigned examples
    for (int m = 0; m < args->M; m++) {
        int k = args->clusterAssignments[m];
        for (int n = 0; n < args->N; n++) {
            args->clusterCentroids[k * args->N + n] += 
            args->data[m * args->N + n];
        }
        counts[k]++;
    }

  // Compute means
    for (int k = 0; k < args->K; k++) {
        counts[k] = max(counts[k], 1); // prevent divide by 0
        for (int n = 0; n < args->N; n++) {
            args->clusterCentroids[k * args->N + n] /= counts[k];
        }
    }

    free(counts);
}

/**
 * Computes the per-cluster cost. Used to check if the algorithm has converged.
 */
void computeCost(WorkerArgs *const args) {
    double *accum = new double[args->K];

    // Zero things out
    for (int k = 0; k < args->K; k++) {
        accum[k] = 0.0;
    }

    // Sum cost for all data points assigned to centroid
    for (int m = 0; m < args->M; m++) {
        int k = args->clusterAssignments[m];
        accum[k] += dist(&args->data[m * args->N],
                        &args->clusterCentroids[k * args->N], args->N);
    }

    // Update costs
    for (int k = args->start; k < args->end; k++) {
        args->currCost[k] = accum[k];
    }

    free(accum);
}

/**
 * Computes the K-Means algorithm, using std::thread to parallelize the work.
 *
 * @param data Pointer to an array of length M*N representing the M different N 
 *     dimensional data points clustered. The data is layed out in a "data point
 *     major" format, so that data[i*N] is the start of the i'th data point in 
 *     the array. The N values of the i'th datapoint are the N values in the 
 *     range data[i*N] to data[(i+1) * N].
 * @param clusterCentroids Pointer to an array of length K*N representing the K 
 *     different N dimensional cluster centroids. The data is laid out in
 *     the same way as explained above for data.
 * @param clusterAssignments Pointer to an array of length M representing the
 *     cluster assignments of each data point, where clusterAssignments[i] = j
 *     indicates that data point i is closest to cluster centroid j.
 * @param M The number of data points to cluster.
 * @param N The dimensionality of the data points.
 * @param K The number of cluster centroids.
 * @param epsilon The algorithm is said to have converged when
 *     |currCost[i] - prevCost[i]| < epsilon for all i where i = 0, 1, ..., K-1
 */
void kMeansThread(
    double *data, double *clusterCentroids, int *clusterAssignments,
    int M, int N, int K, double epsilon, int type) {
    // Used to track convergence
    double *prevCost = new double[K];
    double *currCost = new double[K];

    // The WorkerArgs array is used to pass inputs to and return output from
    // functions.
    WorkerArgs args;
    args.data = data;
    args.clusterCentroids = clusterCentroids;
    args.clusterAssignments = clusterAssignments;
    args.currCost = currCost;
    args.M = M;
    args.N = N;
    args.K = K;
    args.numThreads=4;

    // Initialize arrays to track cost
    for (int k = 0; k < K; k++) {
        prevCost[k] = 1e30;
        currCost[k] = 0.0;
    }

    /* Main K-Means Algorithm Loop */
    int iter = 0;
    double assigning_time = 0.0;
    double centroid_time = 0.0;
    double cost_time = 0.0;

    if (type == 0)
        printf("<<<Serial>>>\n");
    else if (type == 1)
        printf("<<<Threads_1>>>\n");
    else
        printf("<<<Threads_2>>>\n");

    while (!stoppingConditionMet(prevCost, currCost, epsilon, K)) {
        // Update cost arrays (for checking convergence criteria)
        for (int k = 0; k < K; k++) {
            prevCost[k] = currCost[k];
        }

        // Setup args struct
        args.start = 0;
        args.end = K;

        // Time counting for each task
        double assign_time_start = CycleTimer::currentSeconds();
        if (type == 0)
            computeAssignments(&args);
        else if (type == 1)
            computeAssignmentsThreads_1(args);
        else
            computeAssignmentsThreads_2(args);
        
        double assign_time_end = CycleTimer::currentSeconds();

        double centroid_time_start = CycleTimer::currentSeconds();
        computeCentroids(&args);
        double centroid_time_end = CycleTimer::currentSeconds();

        double cost_time_start = CycleTimer::currentSeconds();
        computeCost(&args);
        double cost_time_end = CycleTimer::currentSeconds();

        assigning_time += (assign_time_end - assign_time_start);
        centroid_time += (centroid_time_end - centroid_time_start);
        cost_time += (cost_time_end - cost_time_start);

        printf("Iteration %d: Assign time = %.3f ms, Centroid time = %.3f ms, Cost time = %.3f ms\n",
            iter, 1000.f * (assign_time_end - assign_time_start),
            1000.f * (centroid_time_end - centroid_time_start),
            1000.f * (cost_time_end - cost_time_start));

        iter++;
    }

    printf("[TOTAL] Assign time = %.3f ms, Centroid time = %.3f ms, Cost time = %.3f ms\n",
            1000.f * assigning_time,
            1000.f * centroid_time,
            1000.f * cost_time);

    free(currCost);
    free(prevCost);
}
