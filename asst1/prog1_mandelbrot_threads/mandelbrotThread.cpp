#include <stdio.h>
#include <thread>

#include "CycleTimer.h"

typedef struct {
    float x0, x1;
    float y0, y1;
    unsigned int width;
    unsigned int height;
    int maxIterations;
    int* output;
    int threadId;
    int numThreads;
} WorkerArgs;


extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]);


//
// workerThreadStart --
//
// Thread entrypoint.
void workerThreadStartBasic(WorkerArgs * const args) {
    // TODO FOR CS149 STUDENTS: Implement the body of the worker
    // thread here. Each thread should make a call to mandelbrotSerial()
    // to compute a part of the output image.  For example, in a
    // program that uses two threads, thread 0 could compute the top
    // half of the image and thread 1 could compute the bottom half.

    // printf("Hello world from thread %d\n", args->threadId);
    
    // Basic Implementation
    int row_num_per_thread = args->height / args->numThreads;
    int start_row = args->threadId * row_num_per_thread;
    int end_row = start_row + row_num_per_thread;
    
    if (args->threadId == args->numThreads - 1)
        end_row = args->height;

    double thread_start_time = CycleTimer::currentSeconds();
    mandelbrotSerial(
        args->x0, args->y0, args->x1, args->y1,
        args->width, args->height,
        start_row, end_row - start_row,
        args->maxIterations,
        args->output
    );
    double thread_end_time = CycleTimer::currentSeconds();
    printf(
        "Thread [%d]: [%.3f] ms\n", 
        args->threadId, (thread_end_time - thread_start_time) * 1000
    );
}

void workerThreadStart(WorkerArgs * const args) {
    // TODO FOR CS149 STUDENTS: Implement the body of the worker
    // thread here. Each thread should make a call to mandelbrotSerial()
    // to compute a part of the output image.  For example, in a
    // program that uses two threads, thread 0 could compute the top
    // half of the image and thread 1 could compute the bottom half.

    // printf("Hello world from thread %d\n", args->threadId);
    
    // Optimized Implementation
    constexpr unsigned int CHUNK_SIZE = 32;
    double thread_start_time = CycleTimer::currentSeconds();
    for (unsigned int cur_row = args->threadId * CHUNK_SIZE;
        cur_row < args->height; 
        cur_row += args->numThreads * CHUNK_SIZE) {
            int row = std::min(CHUNK_SIZE, args->height - cur_row);
            mandelbrotSerial(
                args->x0, args->y0, args->x1, args->y1,
                args->width, args->height,
                cur_row, row,
                args->maxIterations,
                args->output
            );
        }
    double thread_end_time = CycleTimer::currentSeconds();
    printf(
        "Thread [%d]: [%.3f] ms\n", 
        args->threadId, (thread_end_time - thread_start_time) * 1000
    );
}

//
// MandelbrotThread --
//
// Multi-threaded implementation of mandelbrot set image generation.
// Threads of execution are created by spawning std::threads.
void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[])
{
    static constexpr int MAX_THREADS = 32;

    if (numThreads > MAX_THREADS)
    {
        fprintf(stderr, "Error: Max allowed threads is %d\n", MAX_THREADS);
        exit(1);
    }

    // Creates thread objects that do not yet represent a thread.
    std::thread workers[MAX_THREADS];
    WorkerArgs args[MAX_THREADS];

    for (int i=0; i<numThreads; i++) {
      
        // TODO FOR CS149 STUDENTS: You may or may not wish to modify
        // the per-thread arguments here.  The code below copies the
        // Same arguments for each thread
        args[i].x0 = x0;
        args[i].y0 = y0;
        args[i].x1 = x1;
        args[i].y1 = y1;
        args[i].width = width;
        args[i].height = height;
        args[i].maxIterations = maxIterations;
        args[i].numThreads = numThreads;
        args[i].output = output;
        // Only TID different
        args[i].threadId = i; 
    }

    // Spawn the worker threads.  Note that only numThreads-1 std::threads
    // are created and the main application thread is used as a worker
    // as well.
    for (int i=1; i<numThreads; i++) {
        workers[i] = std::thread(workerThreadStart, &args[i]);
    }
    
    workerThreadStart(&args[0]);

    // join worker threads
    for (int i=1; i<numThreads; i++) {
        workers[i].join();
    }
}

