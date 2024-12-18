# 并行Scan算法：CS149

## 1. Prefix Sum



## 2. Parallel Prefix Sum (Scan)

```cpp
__global__ void 
__upsweep_phase(int N, int* result, int two_dplus1, int two_d);

__global__ void 
__downsweep_phase(int N, int* result, int two_dplus1, int two_d);

void exclusive_scan(int* input, int N, int* result) {
    // CS149 TODO:
    //
    // Implement your exclusive scan implementation here. Keep in
    // mind that although the arguments to this function are device
    // allocated arrays, this is a function that is running in a thread
    // on the CPU.  Your implementation will need to make multiple calls
    // to CUDA kernel functions (that you must write) to implement the
    // scan.
    int64_t rounded_length = nextPow2(N);
    for (int64_t two_d = 1; two_d < rounded_length; two_d *= 2) {
        int64_t two_dplus1 = two_d << 1;
        int64_t computation_nums = rounded_length / two_dplus1;
        const int64_t blocks = (computation_nums + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
        __upsweep_phase<<<blocks, THREADS_PER_BLOCK>>>(N, result, two_dplus1, two_d);
        cudaDeviceSynchronize();
    }
    for (int64_t two_d = N / 2; two_d >= 1; two_d /= 2) {
        int64_t two_dplus1 = two_d << 1;
        int64_t computation_nums = rounded_length / two_dplus1;
        const int64_t blocks = (computation_nums + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
        __downsweep_phase<<<blocks, THREADS_PER_BLOCK>>>(N, result, two_dplus1, two_d);
        cudaDeviceSynchronize();
    }
}

__global__ void 
__upsweep_phase(int N, int* result, int two_dplus1, int two_d) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < N) {
        int i = index * two_dplus1;
        result[i + two_dplus1 - 1] += result[i + two_d - 1];
    }
}

__global__ void 
__downsweep_phase(int N, int* result, int two_dplus1, int two_d) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < N) {
        int i = index * two_dplus1;
        int t = result[i + two_d - 1];
        if (i == 0)
            result[i + two_dplus1 - 1] = 0;
        result[i + two_d - 1] = result[i + two_dplus1 - 1];
        result[i + two_dplus1 - 1] += t;
    }
}
```