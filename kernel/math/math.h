/**
 * @file math.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Welp, for one who dont like math, things got shit!
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
#ifndef min
#    define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#    define max(a, b) ((a) < (b) ? (b) : (a))
#endif

#ifndef abs
#    define abs(x) ((x) > 0 ? (x) : -(x))
#endif

/**
 * @brief Clamp the value into boundaries
 *
 * @param val
 * @param min_val
 * @param max_val
 * @return int
 */
int clamp(int val, int min_val, int max_val);
