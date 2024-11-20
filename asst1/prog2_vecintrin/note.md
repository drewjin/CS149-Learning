# SIMD向量编程 CS149学习笔记

SIMD：Single Instruction Multi Data. 单指令流多数据。

CS149中，老师给出了一个单一指令译码器，多ALU的处理器，便是实现SIMD的方式。而在课后作业中，他们也实现了一个伪SIMD模拟器。

# 关键思想

1. 单指令多数据执行，实现小块数据并行，提升高效性。

2. 利用向量化掩码计算，实现并行逻辑控制。

# 题目

## 前情提要

函数声明（可以跳过）

```cpp
extern Logger CS149Logger;

template <typename T>
struct __cs149_vec {
    T value[VECTOR_WIDTH];
};

// Declare a mask with __cs149_mask
struct __cs149_mask : __cs149_vec<bool> {};

// Declare a floating point vector register with __cs149_vec_float
#define __cs149_vec_float __cs149_vec<float>

// Declare an integer vector register with __cs149_vec_int
#define __cs149_vec_int   __cs149_vec<int>

//***********************
//* Function Definition *
//***********************

// Return a mask initialized to 1 in the first N lanes and 0 in the others
__cs149_mask _cs149_init_ones(int first = VECTOR_WIDTH);

// Return the inverse of maska
__cs149_mask _cs149_mask_not(__cs149_mask &maska);

// Return (maska | maskb)
__cs149_mask _cs149_mask_or(__cs149_mask &maska, __cs149_mask &maskb);

// Return (maska & maskb)
__cs149_mask _cs149_mask_and(__cs149_mask &maska, __cs149_mask &maskb);

// Count the number of 1s in maska
int _cs149_cntbits(__cs149_mask &maska);

// Set register to value if vector lane is active
//  otherwise keep the old value
void _cs149_vset_float(__cs149_vec_float &vecResult, float value, __cs149_mask &mask);
void _cs149_vset_int(__cs149_vec_int &vecResult, int value, __cs149_mask &mask);
// For user's convenience, returns a vector register with all lanes initialized to value
__cs149_vec_float _cs149_vset_float(float value);
__cs149_vec_int _cs149_vset_int(int value);

// Copy values from vector register src to vector register dest if vector lane active
// otherwise keep the old value
void _cs149_vmove_float(__cs149_vec_float &dest, __cs149_vec_float &src, __cs149_mask &mask);
void _cs149_vmove_int(__cs149_vec_int &dest, __cs149_vec_int &src, __cs149_mask &mask);

// Load values from array src to vector register dest if vector lane active
//  otherwise keep the old value
void _cs149_vload_float(__cs149_vec_float &dest, float* src, __cs149_mask &mask);
void _cs149_vload_int(__cs149_vec_int &dest, int* src, __cs149_mask &mask);

// Store values from vector register src to array dest if vector lane active
//  otherwise keep the old value
void _cs149_vstore_float(float* dest, __cs149_vec_float &src, __cs149_mask &mask);
void _cs149_vstore_int(int* dest, __cs149_vec_int &src, __cs149_mask &mask);

// Return calculation of (veca + vecb) if vector lane active
//  otherwise keep the old value
void _cs149_vadd_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vadd_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// Return calculation of (veca - vecb) if vector lane active
//  otherwise keep the old value
void _cs149_vsub_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vsub_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// Return calculation of (veca * vecb) if vector lane active
//  otherwise keep the old value
void _cs149_vmult_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vmult_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// Return calculation of (veca / vecb) if vector lane active
//  otherwise keep the old value
void _cs149_vdiv_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vdiv_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);


// Return calculation of absolute value abs(veca) if vector lane active
//  otherwise keep the old value
void _cs149_vabs_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_mask &mask);
void _cs149_vabs_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_mask &mask);

// Return a mask of (veca > vecb) if vector lane active
//  otherwise keep the old value
void _cs149_vgt_float(__cs149_mask &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vgt_int(__cs149_mask &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// Return a mask of (veca < vecb) if vector lane active
//  otherwise keep the old value
void _cs149_vlt_float(__cs149_mask &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vlt_int(__cs149_mask &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// Return a mask of (veca == vecb) if vector lane active
//  otherwise keep the old value
void _cs149_veq_float(__cs149_mask &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_veq_int(__cs149_mask &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// Adds up adjacent pairs of elements, so
//  [0 1 2 3] -> [0+1 0+1 2+3 2+3]
void _cs149_hadd_float(__cs149_vec_float &vecResult, __cs149_vec_float &vec);

// Performs an even-odd interleaving where all even-indexed elements move to front half
//  of the array and odd-indexed to the back half, so
//  [0 1 2 3 4 5 6 7] -> [0 2 4 6 1 3 5 7]
void _cs149_interleave_float(__cs149_vec_float &vecResult, __cs149_vec_float &vec);

// Add a customized log to help debugging
void addUserLog(const char * logStr);
```

## 绝对值求解并行化

### 串行代码

```cpp
// computes the absolute value of all elements in the input array
// values, stores result in output
void absSerial(float *values, float *output, int N) {
    for (int i = 0; i < N; i++) {
        float x = values[i];
        if (x < 0) 
            output[i] = -x;
        else
            output[i] = x;
    }
}
```

绝对值的求解是一个简单的过程，仅仅需要判断数值是否小于0，若小于0则反转，若大于等于0则不变。这个逻辑在向量编程中也容易实现。

下面是CS149直接给出的示例代码。

### 并行代码

```cpp
// implementation of absSerial() above, but it is vectorized using CS149 intrinsics
void absVector(float *values, float *output, int N) {
    __cs149_vec_float x;
    __cs149_vec_float result;
    __cs149_vec_float zero = _cs149_vset_float(0.f);
    __cs149_mask maskAll, maskIsNegative, maskIsNotNegative;

    //  Note: Take a careful look at this loop indexing.  This example
    //  code is not guaranteed to work when (N % VECTOR_WIDTH) != 0.
    //  Why is that the case?
    for (int i = 0; i < N; i += VECTOR_WIDTH) {
        // All ones 
        maskAll = _cs149_init_ones(); // [true, true, true, true]

        // All zeros 判断各个元素是否为负
        maskIsNegative = _cs149_init_ones(0); // [false, false, false, false]

        // Load vector of values from contiguous memory addresses
        // 此处是 values + i，指向第 i 位的指针，需要注意 i 的变化步长是向量宽度
        _cs149_vload_float(x, values + i, maskAll); // x = values[i];

        // Set mask according to predicate
        // 判断 *所有* 元素是否小于 0，分别放到 maskIsNegative 里
        _cs149_vlt_float(maskIsNegative, x, zero, maskAll); // if (x < 0) {

        // Execute instruction using mask ("if" clause)
        // *判断小于0* 的元素，被0减去，实现向量化的 if
        _cs149_vsub_float(result, zero, x, maskIsNegative); //   output[i] = -x;

        // Inverse maskIsNegative to generate "else" mask
        // 反转小于 0 向量，用于实现 else 逻辑，产出大于等于 0 向量
        maskIsNotNegative = _cs149_mask_not(maskIsNegative); // } else {

        // Execute instruction ("else" clause)
        // 针对 *大于等于 0* 的元素，直接赋值，实现向量化的 else
        _cs149_vload_float(result, values + i, maskIsNotNegative); //   output[i] = x; }

        // Write results back to memory
        // 输出到 output 对应位置
        _cs149_vstore_float(output + i, result, maskAll);
    }
}
```

上述代码的详细注释中，可以充分明晰向量化操作的精髓。即：显式构造逻辑，且为了保证每个向量内的所有元素都被完整处理，且其不像过去串行代码，一个if-else，满足一个逻辑就不会进入另外一个逻辑中，而是一个向量内有多个很可能符合不同条件的元素。因此，需要在一段连续的逻辑里完成多个可能的条件判断以保证处理的完备性。