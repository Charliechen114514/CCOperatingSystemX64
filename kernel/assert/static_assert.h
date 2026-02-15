/**
 * @file ccos_static_assert.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief   Sometimes, Static Assert Test can prevent some shits
 *          cases happens, so, use it if requested!
 * @version 0.1
 * @date 2026-02-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

// ============================================================================
// Safety: Static Assertions for Type Sizes
// ============================================================================
#define STATIC_ASSERT(expr, msg) typedef char static_assertion_##msg[(expr) ? 1 : -1]

