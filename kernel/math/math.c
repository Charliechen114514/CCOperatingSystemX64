#include "math.h"

int clamp(int val, int min_val, int max_val) {
    val = val < min_val ? min_val : val;
    val = val > max_val ? max_val : val;
    return val;
}