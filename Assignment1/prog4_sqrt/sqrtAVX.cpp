#include <immintrin.h>

extern void sqrtSerial(int N, float startGuess, float* values, float* output);

__m256 fabsVector(__m256 values) {
    __m256 zero = _mm256_set1_ps(0.f);
    __m256 maskLtZero = _mm256_cmp_ps(values, zero, _CMP_LT_OQ);
    __m256 minusValues = _mm256_sub_ps(zero, values);
    values = _mm256_blendv_ps(values, minusValues, maskLtZero);
    return values;
}

void sqrtVector(int N, float initialGuess, float values[], float output[]) {
    const int VECTOR_WIDTH = 8;
    int remainder = N % VECTOR_WIDTH;
    __m256i maskAll = _mm256_set1_epi32(-1);
    __m256 kThreshold = _mm256_set1_ps(0.00001f);
    for (int i = 0; i < N - remainder; i += VECTOR_WIDTH) {
        __m256 x = _mm256_maskload_ps(values + i, maskAll); 
        __m256 guess = _mm256_set1_ps(initialGuess); 
        __m256 error = fabsVector(_mm256_sub_ps(_mm256_mul_ps(_mm256_mul_ps(guess, guess), x), _mm256_set1_ps(1.f)));
        __m256 maskGtK = _mm256_cmp_ps(error, kThreshold, _CMP_GT_OQ);
        int tempMask = _mm256_movemask_ps(maskGtK);
        int GtZeroNums = __builtin_popcount(tempMask);
        while (GtZeroNums) {
            // 3.f * guess
            __m256 guess3 = _mm256_mul_ps(_mm256_set1_ps(3.f), guess);
            // x * guess * guess * guess
            __m256 xguess = _mm256_mul_ps(_mm256_mul_ps(_mm256_mul_ps(x, guess), guess), guess);
            guess = _mm256_blendv_ps(guess, _mm256_mul_ps(_mm256_sub_ps(guess3, xguess), _mm256_set1_ps(0.5f)), maskGtK); // 根据mask选择不变的值
            error = fabsVector(_mm256_blendv_ps(error, _mm256_sub_ps(_mm256_mul_ps(_mm256_mul_ps(guess, guess), x), _mm256_set1_ps(1.f)), maskGtK));

            maskGtK = _mm256_cmp_ps(error, kThreshold, _CMP_GT_OQ);
            tempMask = _mm256_movemask_ps(maskGtK);
            GtZeroNums = __builtin_popcount(tempMask);
        }
        _mm256_storeu_ps(output + i, _mm256_mul_ps(x, guess));
    }
    sqrtSerial(remainder, initialGuess, values + N - remainder, output + N - remainder);
}