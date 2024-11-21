#include <stdio.h>
#include <algorithm>
#include <getopt.h>
#include <math.h>
#include "CS149intrin.h"
#include "logger.h"
using namespace std;

#define EXP_MAX 10

Logger CS149Logger;

void usage(const char *progname);
void initValue(float *values, int *exponents, float *output, float *gold, unsigned int N);
void absSerial(float *values, float *output, int N);
void absVector(float *values, float *output, int N);
void clampedExpSerial(float *values, int *exponents, float *output, int N);
void clampedExpVector(float *values, int *exponents, float *output, int N);
float arraySumSerial(float *values, int N);
float arraySumVector(float *values, int N);
bool verifyResult(float *values, int *exponents, float *output, float *gold, int N);

int main(int argc, char *argv[]) {
    int N = 16;
    bool printLog = false;

    // parse commandline options ////////////////////////////////////////////
    int opt;
    static struct option long_options[] = {
        {"size", 1, 0, 's'},
        {"log", 0, 0, 'l'},
        {"help", 0, 0, '?'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "s:l?", long_options, NULL)) != EOF) {
        switch (opt) {
        case 's':
            N = atoi(optarg);
            if (N <= 0) {
                printf("Error: Workload size is set to %d (<0).\n", N);
                return -1;
            }
            break;
        case 'l':
            printLog = true;
            break;
        case '?':
        default:
            usage(argv[0]);
            return 1;
        }
    }

    float *values = new float[N + VECTOR_WIDTH];
    int *exponents = new int[N + VECTOR_WIDTH];
    float *output = new float[N + VECTOR_WIDTH];
    float *gold = new float[N + VECTOR_WIDTH];
    initValue(values, exponents, output, gold, N);

    // Absolute
    absSerial(values, gold, N);
    absVector(values, output, N);

    printf("\e[1;31mCLAMPED ABSOLUTE\e[0m (provided) \n");
    bool clampedCorrect = verifyResult(values, exponents, output, gold, N);
    if (printLog)
        CS149Logger.printLog();
    CS149Logger.printStats();

    // Exponent
    clampedExpSerial(values, exponents, gold, N);
    clampedExpVector(values, exponents, output, N);

    printf("\e[1;31mCLAMPED EXPONENT\e[0m (required) \n");
    clampedCorrect = verifyResult(values, exponents, output, gold, N);
    if (printLog)
        CS149Logger.printLog();
    CS149Logger.printStats();

    printf("************************ Result Verification *************************\n");
    if (!clampedCorrect){
        printf("@@@ Failed!!!\n");
    } else {
        printf("Passed!!!\n");
    }

    // Sum
    printf("\n\e[1;31mARRAY SUM\e[0m (bonus) \n");
    if (N % VECTOR_WIDTH == 0) {
        float sumGold = arraySumSerial(values, N);
        float sumOutput = arraySumVector(values, N);
        float epsilon = 0.1;
        bool sumCorrect = abs(sumGold - sumOutput) < epsilon * 2;
        if (!sumCorrect) {
            printf("Expected %f, got %f\n.", sumGold, sumOutput);
            printf("@@@ Failed!!!\n");
        } else {
            printf("Passed!!!\n");
        }
    } else {
        printf("Must have N %% VECTOR_WIDTH == 0 for this problem (VECTOR_WIDTH is %d)\n", VECTOR_WIDTH);
    }

    delete[] values;
    delete[] exponents;
    delete[] output;
    delete[] gold;

    return 0;
}

void usage(const char *progname) {
    printf("Usage: %s [options]\n", progname);
    printf("Program Options:\n");
    printf("  -s  --size <N>     Use workload size N (Default = 16)\n");
    printf("  -l  --log          Print vector unit execution log\n");
    printf("  -?  --help         This message\n");
}

void initValue(float *values, int *exponents, float *output, float *gold, unsigned int N) {
    for (unsigned int i = 0; i < N + VECTOR_WIDTH; i++) {
        // random input values
        values[i] = -1.f + 4.f * static_cast<float>(rand()) / RAND_MAX;
        exponents[i] = rand() % EXP_MAX;
        output[i] = 0.f;
        gold[i] = 0.f;
    }
}

bool verifyResult(float *values, int *exponents, float *output, float *gold, int N)
{
    int incorrect = -1;
    float epsilon = 0.00001;
    for (int i = 0; i < N + VECTOR_WIDTH; i++)
    {
        if (abs(output[i] - gold[i]) > epsilon)
        {
            incorrect = i;
            break;
        }
    }

    if (incorrect != -1)
    {
        if (incorrect >= N)
            printf("You have written to out of bound value!\n");
        printf("Wrong calculation at value[%d]!\n", incorrect);
        printf("value  = ");
        for (int i = 0; i < N; i++)
        {
            printf("% f ", values[i]);
        }
        printf("\n");

        printf("exp    = ");
        for (int i = 0; i < N; i++)
        {
            printf("% 9d ", exponents[i]);
        }
        printf("\n");

        printf("output = ");
        for (int i = 0; i < N; i++)
        {
            printf("% f ", output[i]);
        }
        printf("\n");

        printf("gold   = ");
        for (int i = 0; i < N; i++)
        {
            printf("% f ", gold[i]);
        }
        printf("\n");
        return false;
    }
    printf("Results matched with answer!\n");
    return true;
}

// computes the absolute value of all elements in the input array
// values, stores result in output
void absSerial(float *values, float *output, int N)
{
    for (int i = 0; i < N; i++)
    {
        float x = values[i];
        if (x < 0)
        {
            output[i] = -x;
        }
        else
        {
            output[i] = x;
        }
    }
}

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

        // All zeros
        maskIsNegative = _cs149_init_ones(0); // [false, false, false, false]

        // Load vector of values from contiguous memory addresses
        _cs149_vload_float(x, values + i, maskAll); // x = values[i];

        // Set mask according to predicate
        _cs149_vlt_float(maskIsNegative, x, zero, maskAll); // if (x < 0) {

        // Execute instruction using mask ("if" clause)
        _cs149_vsub_float(result, zero, x, maskIsNegative); //   output[i] = -x;

        // Inverse maskIsNegative to generate "else" mask
        maskIsNotNegative = _cs149_mask_not(maskIsNegative); // } else {

        // Execute instruction ("else" clause)
        _cs149_vload_float(result, values + i, maskIsNotNegative); //   output[i] = x; }

        // Write results back to memory
        _cs149_vstore_float(output + i, result, maskAll);
    }
}

// accepts an array of values and an array of exponents
//
// For each element, compute values[i]^exponents[i] and clamp value to
// 9.999.  Store result in output.
void clampedExpSerial(float *values, int *exponents, float *output, int N) {
    for (int i = 0; i < N; i++) {
        float x = values[i];
        int y = exponents[i];
        if (y == 0) {
            output[i] = 1.f;
        } else {
            float result = x;
            int count = y - 1;
            while (count > 0) {
                result *= x;
                count--;
            }
            if (result > 9.999999f) {
                result = 9.999999f;
            }
            output[i] = result;
        }
    }
}

void clampedExpVector(float *values, int *exponents, float *output, int N) {
    // CS149 STUDENTS TODO: Implement your vectorized version of
    // clampedExpSerial() here.
    //
    // Your solution should work for any value of
    // N and VECTOR_WIDTH, not just when VECTOR_WIDTH divides N
    __cs149_vec_float result;
    __cs149_vec_float x; // value
    __cs149_vec_float y; // exponents
    __cs149_vec_float zero = _cs149_vset_float(0.f);
    __cs149_vec_float max = _cs149_vset_float(9.999999f);
    __cs149_mask maskIsZero = _cs149_init_ones(0); // [false, false, false, false]
    __cs149_mask maskAll = _cs149_init_ones(VECTOR_WIDTH); // [true, true, true, true]
    float f_exponents[N];
    for (int i = 0; i < N; i++) {
        f_exponents[i] = static_cast<float>(*(exponents + i));
    }
    int remainder = N % VECTOR_WIDTH;
    for (int i = 0; i < N - remainder; i += VECTOR_WIDTH) {
        _cs149_vload_float(x, values + i, maskAll);
        _cs149_vload_float(y, f_exponents + i, maskAll);
        _cs149_veq_float(maskIsZero, y, zero, maskAll); // if (y == 0)
        _cs149_vset_float(result, 1.f, maskIsZero);     // result[i] = 1.f
        __cs149_mask maskIsNotZero = _cs149_mask_not(maskIsZero);                    // else
        _cs149_vmove_float(result, x, maskIsNotZero);      // float result = x;
        __cs149_vec_float count, one = _cs149_vset_float(1.f);
        _cs149_vsub_float(count, y, one, maskIsNotZero);   // int count = y - 1;
        __cs149_mask maskIsGtZero = _cs149_init_ones(0);   // while(count > 0)
        _cs149_vgt_float(maskIsGtZero, count, zero, maskAll);
        while (_cs149_cntbits(maskIsGtZero) != 0) {       // 只要还有count大于0就继续循环
            _cs149_vmult_float(result, result, x, maskIsGtZero); // result = result * x;
            _cs149_vsub_float(count, count, one, maskIsGtZero);  // count = count - 1;
            _cs149_vgt_float(maskIsGtZero, count, zero, maskAll);// 计算有哪些count > 0
        }
        __cs149_mask maskIsGtMax = _cs149_init_ones(0);
        _cs149_vgt_float(maskIsGtMax, result, max, maskAll); // if (result > 9.999999f)
        _cs149_vmove_float(result, max, maskIsGtMax);        // result = 9.99999f
        _cs149_vstore_float(output + i, result, maskAll);    // output[i] = result
    }
    clampedExpSerial(values + (N - remainder), exponents + (N - remainder), output + (N - remainder), remainder);
}

// returns the sum of all elements in values
float arraySumSerial(float *values, int N) {
    float sum = 0;
    for (int i = 0; i < N; i++) {
        sum += values[i];
    }
    return sum;
}

void printVector(float *values, int N) {
    printf("[");
    for (int i = 0; i < N; i++) 
        printf("%f ", values[i]);
    printf("]\n");
}

// returns the sum of all elements in values
// You can assume N is a multiple of VECTOR_WIDTH
// You can assume VECTOR_WIDTH is a power of 2
float arraySumVector(float* values, int N) {
    //
    // CS149 STUDENTS TODO: Implement your vectorized version of arraySumSerial here
    //
    __cs149_mask maskAll = _cs149_init_ones(VECTOR_WIDTH);
    float temp[VECTOR_WIDTH];
    float sum = 0;
    for (int i=0; i<N; i+=VECTOR_WIDTH) {
        printf("[step]\n");
        __cs149_vec_float x;
        _cs149_vload_float(x, values + i, maskAll);
        for (int i = 0; i < log2(VECTOR_WIDTH); i++) {
            printf("[round]\n");
            _cs149_hadd_float(x, x);
            printf("x hadd[%d]:", i);
            printVector(x.value, VECTOR_WIDTH);
            _cs149_interleave_float(x, x);
            printf("x interleave[%d]:", i);
            printVector(x.value, VECTOR_WIDTH);
        }
        _cs149_vstore_float(temp, x, maskAll);
        sum += temp[0];
        printf("sum: %f\n", sum);
    }
    return sum;
}

/* 归约算法
[step]
[round]
x hadd[0]:[4.493148 4.493148 2.987481 2.987481 1.020687 1.020687 3.268057 3.268057 ]
x interleave[0]:[4.493148 2.987481 1.020687 3.268057 2.987481 3.268057 3.268057 3.268057 ]
[round]
x hadd[1]:[7.480628 7.480628 4.288744 4.288744 6.255538 6.255538 6.536114 6.536114 ]
x interleave[1]:[7.480628 4.288744 6.255538 6.536114 4.288744 6.536114 6.536114 6.536114 ]
[round]
x hadd[2]:[11.769373 11.769373 12.791651 12.791651 10.824858 10.824858 13.072227 13.072227 ]
x interleave[2]:[11.769373 12.791651 10.824858 13.072227 12.791651 13.072227 13.072227 13.072227 ]
sum: 11.769373
[step]
[round]
x hadd[0]:[1.109257 1.109257 -1.385871 -1.385871 -0.854122 -0.854122 4.047428 4.047428 ]
x interleave[0]:[1.109257 -1.385871 -0.854122 4.047428 -1.385871 4.047428 4.047428 4.047428 ]
[round]
x hadd[1]:[-0.276614 -0.276614 3.193306 3.193306 2.661556 2.661556 8.094855 8.094855 ]
x interleave[1]:[-0.276614 3.193306 2.661556 8.094855 3.193306 8.094855 8.094855 8.094855 ]
[round]
x hadd[2]:[2.916692 2.916692 10.756412 10.756412 11.288161 11.288161 16.189711 16.189711 ]
x interleave[2]:[2.916692 10.756412 11.288161 16.189711 10.756412 16.189711 16.189711 16.189711 ]
sum: 14.686065
*/