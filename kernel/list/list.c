/**
 * @file list.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Implementation of Linux kernel style intrusive doubly-linked list.
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 * This file contains the implementation of functions that cannot be
 * expressed as inline functions or macros in list.h.
 */

#include "list.h"
#include "base/help_macros.h"

/* ===== Internal Helper Functions ===== */

void __list_add(list_head* new_node, list_head* prev, list_head* next) {
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

void __list_del(list_head* prev, list_head* next) {
    next->prev = prev;
    prev->next = next;
}

/* ===== Basic List Operations ===== */

void list_del_init(list_head* entry) {
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

void list_replace_init(list_head* old_node, list_head* new_node) {
    list_replace(old_node, new_node);
    INIT_LIST_HEAD(old_node);
}

/* ===== List Splicing Operations ===== */

void list_splice(const list_head* list, list_head* head) {
    if (!list_is_empty(list)) {
        // Save pointers before modifying
        list_head* first = (list_head*)list->next;
        list_head* last = (list_head*)list->prev;
        list_head* head_next = head->next;

        // Connect the spliced list to the head
        first->prev = head;
        head->next = first;

        // Connect the end of spliced list to the rest of head's list
        last->next = head_next;
        head_next->prev = last;
    }
}

void list_splice_tail(const list_head* list, list_head* head) {
    if (!list_is_empty(list)) {
        // Save pointers before modifying
        list_head* first = (list_head*)list->next;
        list_head* last = (list_head*)list->prev;
        list_head* head_prev = head->prev;

        // Connect the spliced list to the end of head
        head->prev = last;
        last->next = head;

        // Connect the beginning of spliced list to head
        head_prev->next = first;
        first->prev = head_prev;
    }
}

void list_splice_init(list_head* list, list_head* head) {
    if (!list_is_empty(list)) {
        // Save the first and last nodes of the list to be spliced
        list_head* first = list->next;
        list_head* last = list->prev;
        // Save the head's next pointer (it will be modified)
        list_head* head_next = head->next;

        // Connect the spliced list to the head
        first->prev = head;
        head->next = first;

        // Connect the end of spliced list to the rest of head's list
        last->next = head_next;
        head_next->prev = last;

        // Reinitialize the source list head
        INIT_LIST_HEAD(list);
    }
}

void list_cut_position(list_head* list, list_head* head, list_head* entry) {
    if (list_is_empty(head)) {
        INIT_LIST_HEAD(list);
        return;
    }

    if (list_is_singular(head) && (head->next != entry && head != entry)) {
        INIT_LIST_HEAD(list);
        return;
    }

    if (entry == head) {
        // Cut the entire list - move all nodes to list
        list->next = head->next;
        list->prev = head->prev;
        list->next->prev = list;
        list->prev->next = list;
        INIT_LIST_HEAD(head);
    } else {
        // Cut from head to entry (exclusive)
        // The first element of list is head->next
        // The last element of list is entry->prev
        list->next = head->next;
        list->prev = entry->prev;

        // Connect the cut portion to list
        list->next->prev = list;
        list->prev->next = list;

        // Update head to start from entry
        head->next = entry;
        entry->prev = head;
    }
}

/* ===== List Query Functions ===== */

bool list_on_list(const list_head* node) {
    // An initialized but unadded list node points to itself
    // A node on a list has different next and prev pointers
    return (node->next != node) && (node->prev != node);
}

size_t list_count(const list_head* head) {
    size_t count = 0;
    list_head* pos;

    list_for_each(pos, head) {
        count++;
    }

    return count;
}
