/**
 * @file list.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Linux kernel style intrusive doubly-linked list implementation.
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 * This file provides an intrusive doubly-linked list implementation similar to
 * the Linux kernel's list_head. The list nodes are embedded directly into the
 * data structures, allowing for efficient memory usage and flexible data organization.
 *
 * Features include:
 * - O(1) insertion, deletion, and movement operations
 * - Both header-only and implementation functions
 * - Safe iteration macros that support deletion during traversal
 * - Entry extraction macros to get the containing structure from a list node
 * - List splicing and cutting operations
 * - Forward and reverse traversal support
 */

#pragma once

#include "defines/types.h"

/**
 * @struct list_head
 * @brief Doubly-linked list node structure.
 *
 * This structure is designed to be embedded directly into user-defined structures.
 * A single list_head can be a member of multiple lists simultaneously.
 */
typedef struct list_head {
    struct list_head* next; /**< Pointer to the next node in the list. */
    struct list_head* prev; /**< Pointer to the previous node in the list. */
} list_head;

/* ===== Initialization Macros ===== */

/**
 * @brief Static initializer for a list head.
 *
 * Use this to statically initialize a list head at compile time.
 *
 * @param name The variable name for the list head.
 */
#define LIST_HEAD(name) \
    list_head name = { &(name), &(name) }

/**
 * @brief Dynamically initialize a list head.
 *
 * Use this to initialize a list head at runtime.
 *
 * @param ptr Pointer to the list_head to initialize.
 */
static inline void INIT_LIST_HEAD(list_head* ptr) {
    ptr->next = ptr;
    ptr->prev = ptr;
}

/* ===== Internal Helper Functions (in list.c) ===== */

/**
 * @brief Internal function to add a list node between two known nodes.
 *
 * This function is for internal use by the list implementation.
 *
 * @param new_node The new node to insert.
 * @param prev The node that will precede the new node.
 * @param next The node that will follow the new node.
 */
void __list_add(list_head* new_node, list_head* prev, list_head* next);

/**
 * @brief Internal function to remove a list node between two known nodes.
 *
 * This function is for internal use by the list implementation.
 *
 * @param prev The node preceding the node to remove.
 * @param next The node following the node to remove.
 */
void __list_del(list_head* prev, list_head* next);

/* ===== Basic List Operations ===== */

/**
 * @brief Insert a new node at the head of the list.
 *
 * The new node will be inserted immediately after the head.
 *
 * @param new_node The node to insert.
 * @param head The list head.
 */
static inline void list_add(list_head* new_node, list_head* head) {
    __list_add(new_node, head, head->next);
}

/**
 * @brief Insert a new node at the tail of the list.
 *
 * The new node will be inserted immediately before the head (at the end).
 *
 * @param new_node The node to insert.
 * @param head The list head.
 */
static inline void list_add_tail(list_head* new_node, list_head* head) {
    __list_add(new_node, head->prev, head);
}

/**
 * @brief Delete a node from the list.
 *
 * The node is removed from the list but its pointers are not cleared.
 * Use list_del_init() if you need to reinitialize the node.
 *
 * @param entry The node to delete.
 */
static inline void list_del(list_head* entry) {
    __list_del(entry->prev, entry->next);
}

/**
 * @brief Delete a node and reinitialize it.
 *
 * The node is removed from the list and its pointers are set to point to itself,
 * making it a valid empty list.
 *
 * @param entry The node to delete and reinitialize.
 */
void list_del_init(list_head* entry);

/**
 * @brief Replace an old node with a new node.
 *
 * The old node is removed from the list and replaced with the new node.
 * The old node's pointers are not modified.
 *
 * @param old_node The node to replace.
 * @param new_node The replacement node.
 */
static inline void list_replace(list_head* old_node, list_head* new_node) {
    new_node->next = old_node->next;
    new_node->next->prev = new_node;
    new_node->prev = old_node->prev;
    new_node->prev->next = new_node;
}

/**
 * @brief Replace a node and reinitialize the old node.
 *
 * The old node is removed from the list and replaced with the new node.
 * The old node's pointers are reinitialized to point to itself.
 *
 * @param old_node The node to replace.
 * @param new_node The replacement node.
 */
void list_replace_init(list_head* old_node, list_head* new_node);

/**
 * @brief Check if a list is empty.
 *
 * @param head The list head to check.
 * @return true if the list is empty, false otherwise.
 */
static inline bool list_is_empty(const list_head* head) {
    return head->next == head;
}

/**
 * @brief Check if a node is the last entry in the list.
 *
 * @param entry The node to check.
 * @param head The list head.
 * @return true if the node is the last entry, false otherwise.
 */
static inline bool list_is_last(const list_head* entry, const list_head* head) {
    return entry->next == head;
}

/* ===== List Splicing Operations ===== */

/**
 * @brief Join two lists by inserting the first list at the head of the second.
 *
 * The list at @p list is inserted at the beginning of the list at @p head.
 * The @p list head becomes empty after this operation.
 *
 * @param list The list to insert (will be emptied).
 * @param head The destination list head.
 */
void list_splice(const list_head* list, list_head* head);

/**
 * @brief Join two lists by appending the first list to the end of the second.
 *
 * The list at @p list is appended to the end of the list at @p head.
 * The @p list head becomes empty after this operation.
 *
 * @param list The list to append (will be emptied).
 * @param head The destination list head.
 */
void list_splice_tail(const list_head* list, list_head* head);

/**
 * @brief Join two lists and reinitialize the source list head.
 *
 * Similar to list_splice(), but also reinitializes the source list head.
 *
 * @param list The list to insert (will be emptied and reinitialized).
 * @param head The destination list head.
 */
void list_splice_init(list_head* list, list_head* head);

/**
 * @brief Cut a list into two parts.
 *
 * Moves the initial part of @p head (up to but not including @p entry)
 * to @p list. @p entry becomes the new head of the remaining list.
 *
 * @param list The destination list head for the cut portion.
 * @param head The source list head.
 * @param entry The first entry that will remain in the source list.
 */
void list_cut_position(list_head* list, list_head* head, list_head* entry);

/* ===== Entry Access Macros ===== */

/**
 * @brief Get the structure containing a list_head.
 *
 * This macro uses container_of to calculate the address of the containing
 * structure given the address of a list_head member.
 *
 * @param ptr Pointer to the list_head member.
 * @param type The type of the containing structure.
 * @param member The name of the list_head member within the structure.
 * @return Pointer to the containing structure.
 */
#define list_entry(ptr, type, member) \
    ((type*)((char*)(ptr) - (unsigned long)(&((type*)0)->member)))

/**
 * @brief Get the first entry in a list.
 *
 * @param ptr The list head.
 * @param type The type of the containing structure.
 * @param member The name of the list_head member within the structure.
 * @return Pointer to the first entry, or NULL if the list is empty.
 */
#define list_first_entry(ptr, type, member) \
    (list_is_empty(ptr) ? NULL : list_entry((ptr)->next, type, member))

/**
 * @brief Get the last entry in a list.
 *
 * @param ptr The list head.
 * @param type The type of the containing structure.
 * @param member The name of the list_head member within the structure.
 * @return Pointer to the last entry, or NULL if the list is empty.
 */
#define list_last_entry(ptr, type, member) \
    (list_is_empty(ptr) ? NULL : list_entry((ptr)->prev, type, member))

/**
 * @brief Get the next entry in a list.
 *
 * @param pos Pointer to the current entry.
 * @param member The name of the list_head member within the structure.
 * @return Pointer to the next entry.
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, __typeof__(*(pos)), member)

/**
 * @brief Get the previous entry in a list.
 *
 * @param pos Pointer to the current entry.
 * @param member The name of the list_head member within the structure.
 * @return Pointer to the previous entry.
 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, __typeof__(*(pos)), member)

/**
 * @brief Safely get the next entry in a list.
 *
 * Unlike list_next_entry, this macro checks if the next entry is the
 * list head before dereferencing.
 *
 * @param pos Pointer to the current entry.
 * @param head The list head.
 * @param member The name of the list_head member within the structure.
 * @return Pointer to the next entry, or NULL if at the end of the list.
 */
#define list_next_entry_safe(pos, head, member) \
    ((pos)->member.next == (head) ? NULL : list_next_entry(pos, member))

/**
 * @brief Safely get the previous entry in a list.
 *
 * Unlike list_prev_entry, this macro checks if the previous entry is the
 * list head before dereferencing.
 *
 * @param pos Pointer to the current entry.
 * @param head The list head.
 * @param member The name of the list_head member within the structure.
 * @return Pointer to the previous entry, or NULL if at the start of the list.
 */
#define list_prev_entry_safe(pos, head, member) \
    ((pos)->member.prev == (head) ? NULL : list_prev_entry(pos, member))

/* ===== List Traversal Macros ===== */

/**
 * @brief Iterate over a list.
 *
 * @param pos The list_head pointer to use as a loop counter.
 * @param head The list head to iterate over.
 */
#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/**
 * @brief Iterate over a list in reverse.
 *
 * @param pos The list_head pointer to use as a loop counter.
 * @param head The list head to iterate over.
 */
#define list_for_each_prev(pos, head) \
    for ((pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)

/**
 * @brief Iterate over a list, safe against deletion.
 *
 * This variant stores the next node before each iteration, allowing
 * safe deletion of the current node during the loop.
 *
 * @param pos The list_head pointer to use as a loop counter.
 * @param n Another list_head pointer to store the next node.
 * @param head The list head to iterate over.
 */
#define list_for_each_safe(pos, n, head)                       \
    for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); \
         (pos) = (n), (n) = (pos)->next)

/**
 * @brief Iterate over a list in reverse, safe against deletion.
 *
 * @param pos The list_head pointer to use as a loop counter.
 * @param n Another list_head pointer to store the previous node.
 * @param head The list head to iterate over.
 */
#define list_for_each_prev_safe(pos, n, head)                  \
    for ((pos) = (head)->prev, (n) = (pos)->prev; (pos) != (head); \
         (pos) = (n), (n) = (pos)->prev)

/**
 * @brief Iterate over a list of given type.
 *
 * @param pos The type pointer to use as a loop counter.
 * @param head The list head to iterate over.
 * @param member The name of the list_head member within the structure.
 */
#define list_for_each_entry(pos, head, member)                 \
    for ((pos) = list_first_entry(head, __typeof__(*(pos)), member); \
         (pos) != NULL; (pos) = list_next_entry_safe(pos, head, member))

/**
 * @brief Iterate over a list of given type in reverse.
 *
 * @param pos The type pointer to use as a loop counter.
 * @param head The list head to iterate over.
 * @param member The name of the list_head member within the structure.
 */
#define list_for_each_entry_reverse(pos, head, member)         \
    for ((pos) = list_last_entry(head, __typeof__(*(pos)), member); \
         (pos) != NULL; (pos) = list_prev_entry_safe(pos, head, member))

/**
 * @brief Iterate over a list of given type, safe against deletion.
 *
 * @param pos The type pointer to use as a loop counter.
 * @param n Another type pointer to store the next entry.
 * @param head The list head to iterate over.
 * @param member The name of the list_head member within the structure.
 */
#define list_for_each_entry_safe(pos, n, head, member)         \
    for ((pos) = list_first_entry(head, __typeof__(*(pos)), member); \
         (pos) && ((n) = list_next_entry_safe(pos, head, member), 1); \
         (pos) = (n))

/**
 * @brief Iterate over a list of given type in reverse, safe against deletion.
 *
 * @param pos The type pointer to use as a loop counter.
 * @param n Another type pointer to store the previous entry.
 * @param head The list head to iterate over.
 * @param member The name of the list_head member within the structure.
 */
#define list_for_each_entry_reverse_safe(pos, n, head, member) \
    for ((pos) = list_last_entry(head, __typeof__(*(pos)), member);  \
         (pos) && ((n) = list_prev_entry_safe(pos, head, member), 1); \
         (pos) = (n))

/* ===== List Query Functions ===== */

/**
 * @brief Check if a list has exactly one entry.
 *
 * @param head The list head to check.
 * @return true if the list has exactly one entry, false otherwise.
 */
static inline bool list_is_singular(const list_head* head) {
    return !list_is_empty(head) && (head->next == head->prev);
}

/**
 * @brief Check if a list node is on a list.
 *
 * This function checks whether the given node has been added to a list.
 * A node that has been initialized with INIT_LIST_HEAD() but not added
 * to any list will return false.
 *
 * @param node The node to check.
 * @return true if the node is on a list, false otherwise.
 */
bool list_on_list(const list_head* node);

/**
 * @brief Count the number of entries in a list.
 *
 * @param head The list head.
 * @return The number of entries in the list.
 */
size_t list_count(const list_head* head);
