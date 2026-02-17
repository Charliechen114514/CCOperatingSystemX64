/**
 * @file test_list.c
 * @brief Comprehensive unit tests for list module (Linux kernel style intrusive doubly-linked list)
 */

// Standard types for hosted environment
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Include kernel types and list header
#include "defines/types.h"
#include "list/list.h"

// Define global test counters
int g_test_passed = 0;
int g_test_failed = 0;

#include "host_support.h"

// ============================================================================
// Test Data Structures
// ============================================================================

/**
 * @struct test_node
 * @brief Sample structure to test intrusive list functionality.
 */
typedef struct test_node {
    int value;
    list_head list;  // Embedded list node
} test_node;

/**
 * @struct multi_list_node
 * @brief Structure with two list_head members to test multi-list membership.
 */
typedef struct multi_list_struct {
    int value;
    list_head list1;
    list_head list2;
} multi_list_node;

// ============================================================================
// Helper Functions and Global Node Pool
// ============================================================================

#define MAX_NODES 256
static test_node g_node_pool[MAX_NODES];
static int g_node_idx = 0;

/**
 * @brief Reset the node pool for a fresh test.
 */
static void reset_nodes(void) {
    g_node_idx = 0;
}

/**
 * @brief Create a new test node with a given value.
 */
static test_node* create_node(int value) {
    if (g_node_idx < MAX_NODES) {
        g_node_pool[g_node_idx].value = value;
        INIT_LIST_HEAD(&g_node_pool[g_node_idx].list);
        return &g_node_pool[g_node_idx++];
    }
    return NULL;
}

/**
 * @brief Initialize a list head for testing.
 */
static void init_list_head(list_head* head) {
    INIT_LIST_HEAD(head);
}

// ============================================================================
// 1. Initialization Tests
// ============================================================================

static void test_init(void) {
    TEST_INFO("Testing list initialization");

    // Test static initialization macro
    LIST_HEAD(static_head);
    TEST_ASSERT_EQ(static_head.next, &static_head, "LIST_HEAD: next points to self");
    TEST_ASSERT_EQ(static_head.prev, &static_head, "LIST_HEAD: prev points to self");

    // Test dynamic initialization
    list_head dynamic_head;
    INIT_LIST_HEAD(&dynamic_head);
    TEST_ASSERT_EQ(dynamic_head.next, &dynamic_head, "INIT_LIST_HEAD: next points to self");
    TEST_ASSERT_EQ(dynamic_head.prev, &dynamic_head, "INIT_LIST_HEAD: prev points to self");

    // Test is_empty on newly initialized list
    TEST_ASSERT_TRUE(list_is_empty(&static_head), "list_is_empty: newly initialized list is empty");
    TEST_ASSERT_TRUE(list_is_empty(&dynamic_head), "list_is_empty: dynamic list is empty");

    TEST_PASS("list initialization");
}

// ============================================================================
// 2. Basic Insertion Tests
// ============================================================================

static void test_insertion(void) {
    TEST_INFO("Testing basic insertion operations");

    reset_nodes();

    list_head head;
    test_node* node1, *node2, *node3;

    init_list_head(&head);
    node1 = create_node(1);
    node2 = create_node(2);
    node3 = create_node(3);

    // Test list_add (insert at head)
    list_add(&node1->list, &head);
    TEST_ASSERT_FALSE(list_is_empty(&head), "list_add: list is not empty after add");
    TEST_ASSERT_EQ(head.next, &node1->list, "list_add: head.next points to inserted node");
    TEST_ASSERT_EQ(head.prev, &node1->list, "list_add: head.prev points to inserted node (single element)");
    TEST_ASSERT_EQ(node1->list.next, &head, "list_add: node.next points back to head");
    TEST_ASSERT_EQ(node1->list.prev, &head, "list_add: node.prev points back to head");

    // Test list_add with multiple nodes
    list_add(&node2->list, &head);
    TEST_ASSERT_EQ(head.next, &node2->list, "list_add: new node at head");
    TEST_ASSERT_EQ(node2->list.next, &node1->list, "list_add: new node points to previous first");

    list_add(&node3->list, &head);
    TEST_ASSERT_EQ(head.next, &node3->list, "list_add: third node becomes first");

    // Verify order is: head -> node3 -> node2 -> node1 -> head
    TEST_ASSERT_EQ(node3->list.next, &node2->list, "list_add: order verified (3->2)");
    TEST_ASSERT_EQ(node2->list.next, &node1->list, "list_add: order verified (2->1)");

    TEST_PASS("basic insertion");
}

// ============================================================================
// 3. Tail Insertion Tests
// ============================================================================

static void test_tail_insertion(void) {
    TEST_INFO("Testing tail insertion operations");

    reset_nodes();

    list_head head;
    test_node* node1, *node2, *node3;

    init_list_head(&head);
    node1 = create_node(10);
    node2 = create_node(20);
    node3 = create_node(30);

    // Test list_add_tail (insert at tail)
    list_add_tail(&node1->list, &head);
    TEST_ASSERT_EQ(head.next, &node1->list, "list_add_tail: single node - head.next");
    TEST_ASSERT_EQ(head.prev, &node1->list, "list_add_tail: single node - head.prev");

    list_add_tail(&node2->list, &head);
    TEST_ASSERT_EQ(head.prev, &node2->list, "list_add_tail: new node at tail");
    TEST_ASSERT_EQ(node1->list.next, &node2->list, "list_add_tail: first node points to second");

    list_add_tail(&node3->list, &head);

    // Verify order is: head -> node1 -> node2 -> node3 -> head
    TEST_ASSERT_EQ(head.next, &node1->list, "list_add_tail: order verified (head->1)");
    TEST_ASSERT_EQ(node1->list.next, &node2->list, "list_add_tail: order verified (1->2)");
    TEST_ASSERT_EQ(node2->list.next, &node3->list, "list_add_tail: order verified (2->3)");
    TEST_ASSERT_EQ(node3->list.next, &head, "list_add_tail: order verified (3->head)");

    TEST_PASS("tail insertion");
}

// ============================================================================
// 4. Deletion Tests
// ============================================================================

static void test_deletion(void) {
    TEST_INFO("Testing deletion operations");

    reset_nodes();

    list_head head;
    test_node* node1, *node2, *node3;

    init_list_head(&head);
    node1 = create_node(1);
    node2 = create_node(2);
    node3 = create_node(3);

    // Build list: head -> 1 -> 2 -> 3 -> head
    list_add_tail(&node1->list, &head);
    list_add_tail(&node2->list, &head);
    list_add_tail(&node3->list, &head);

    // Test list_del (remove middle node)
    list_del(&node2->list);
    TEST_ASSERT_EQ(head.next, &node1->list, "list_del: head still points to node1");
    TEST_ASSERT_EQ(node1->list.next, &node3->list, "list_del: node1 now points to node3");
    TEST_ASSERT_EQ(node3->list.prev, &node1->list, "list_del: node3 prev points to node1");

    // Test list_del (remove first node)
    list_del(&node1->list);
    TEST_ASSERT_EQ(head.next, &node3->list, "list_del: node3 is now first");
    TEST_ASSERT_EQ(head.prev, &node3->list, "list_del: node3 is also last (only element)");

    // Test list_del (remove last node)
    list_del(&node3->list);
    TEST_ASSERT_TRUE(list_is_empty(&head), "list_del: list empty after removing all nodes");

    TEST_PASS("deletion");
}

// ============================================================================
// 5. Delete Init Tests
// ============================================================================

static void test_del_init(void) {
    TEST_INFO("Testing delete and reinitialize");

    reset_nodes();

    list_head head;
    test_node* node;

    init_list_head(&head);
    node = create_node(42);

    list_add(&node->list, &head);

    // Test list_del_init
    list_del_init(&node->list);
    TEST_ASSERT_TRUE(list_is_empty(&head), "list_del_init: list is empty");
    TEST_ASSERT_EQ(node->list.next, &node->list, "list_del_init: node.next points to self");
    TEST_ASSERT_EQ(node->list.prev, &node->list, "list_del_init: node.prev points to self");

    // Verify the node can be added to another list
    list_head head2;
    init_list_head(&head2);
    list_add(&node->list, &head2);
    TEST_ASSERT_FALSE(list_is_empty(&head2), "list_del_init: reinitialized node can be reused");

    TEST_PASS("delete and reinitialize");
}

// ============================================================================
// 6. Replace Tests
// ============================================================================

static void test_replace(void) {
    TEST_INFO("Testing replace operations");

    reset_nodes();

    list_head head;
    test_node* node1, *node2, *node3, *new_node;

    init_list_head(&head);
    node1 = create_node(1);
    node2 = create_node(2);
    node3 = create_node(3);
    new_node = create_node(99);

    list_add_tail(&node1->list, &head);
    list_add_tail(&node2->list, &head);
    list_add_tail(&node3->list, &head);

    // Test list_replace
    list_replace(&node2->list, &new_node->list);

    TEST_ASSERT_EQ(node1->list.next, &new_node->list, "list_replace: node1 points to new_node");
    TEST_ASSERT_EQ(new_node->list.next, &node3->list, "list_replace: new_node points to node3");
    TEST_ASSERT_EQ(node3->list.prev, &new_node->list, "list_replace: node3 prev points to new_node");
    TEST_ASSERT_EQ(new_node->list.prev, &node1->list, "list_replace: new_node prev points to node1");

    TEST_PASS("replace");
}

// ============================================================================
// 7. Replace Init Tests
// ============================================================================

static void test_replace_init(void) {
    TEST_INFO("Testing replace and reinitialize old node");

    reset_nodes();

    list_head head;
    test_node* node1, *old_node, *new_node;

    init_list_head(&head);
    node1 = create_node(1);
    old_node = create_node(2);
    new_node = create_node(99);

    list_add_tail(&node1->list, &head);
    list_add_tail(&old_node->list, &head);

    // Test list_replace_init
    list_replace_init(&old_node->list, &new_node->list);

    // Verify new node is in the list
    TEST_ASSERT_EQ(node1->list.next, &new_node->list, "list_replace_init: new_node in list");

    // Verify old node is reinitialized
    TEST_ASSERT_EQ(old_node->list.next, &old_node->list, "list_replace_init: old node points to self");
    TEST_ASSERT_EQ(old_node->list.prev, &old_node->list, "list_replace_init: old node prev points to self");

    TEST_PASS("replace and reinitialize");
}

// ============================================================================
// 8. List State Query Tests
// ============================================================================

static void test_list_queries(void) {
    TEST_INFO("Testing list state queries");

    reset_nodes();

    // Test list_is_last
    list_head head;
    init_list_head(&head);
    test_node* n1 = create_node(1);
    test_node* n2 = create_node(2);
    test_node* n3 = create_node(3);

    list_add_tail(&n1->list, &head);
    list_add_tail(&n2->list, &head);
    list_add_tail(&n3->list, &head);

    TEST_ASSERT_TRUE(list_is_last(&n3->list, &head), "list_is_last: node3 is last");
    TEST_ASSERT_FALSE(list_is_last(&n2->list, &head), "list_is_last: node2 is not last");
    TEST_ASSERT_FALSE(list_is_last(&n1->list, &head), "list_is_last: node1 is not last");

    // Test list_is_singular
    list_head single_head;
    init_list_head(&single_head);
    TEST_ASSERT_FALSE(list_is_singular(&single_head), "list_is_singular: empty list is not singular");

    test_node* single = create_node(100);
    list_add(&single->list, &single_head);
    TEST_ASSERT_TRUE(list_is_singular(&single_head), "list_is_singular: single element list is singular");

    test_node* second = create_node(200);
    list_add_tail(&second->list, &single_head);
    TEST_ASSERT_FALSE(list_is_singular(&single_head), "list_is_singular: two element list is not singular");

    // Test list_on_list
    test_node* detached = create_node(300);
    TEST_ASSERT_FALSE(list_on_list(&detached->list), "list_on_list: detached node returns false");

    list_add(&detached->list, &head);
    TEST_ASSERT_TRUE(list_on_list(&detached->list), "list_on_list: attached node returns true");

    // Test list_count
    list_head count_head;
    init_list_head(&count_head);
    TEST_ASSERT_EQ(list_count(&count_head), (size_t)0, "list_count: empty list has 0 elements");

    test_node* c1 = create_node(10);
    test_node* c2 = create_node(20);
    test_node* c3 = create_node(30);

    list_add_tail(&c1->list, &count_head);
    TEST_ASSERT_EQ(list_count(&count_head), (size_t)1, "list_count: 1 element");

    list_add_tail(&c2->list, &count_head);
    TEST_ASSERT_EQ(list_count(&count_head), (size_t)2, "list_count: 2 elements");

    list_add_tail(&c3->list, &count_head);
    TEST_ASSERT_EQ(list_count(&count_head), (size_t)3, "list_count: 3 elements");

    TEST_PASS("list state queries");
}

// ============================================================================
// 9. Entry Access Tests
// ============================================================================

static void test_entry_access(void) {
    TEST_INFO("Testing entry access macros");

    reset_nodes();

    list_head head;
    test_node* node1, *node2, *node3;
    test_node* retrieved;

    init_list_head(&head);
    node1 = create_node(10);
    node2 = create_node(20);
    node3 = create_node(30);

    list_add_tail(&node1->list, &head);
    list_add_tail(&node2->list, &head);
    list_add_tail(&node3->list, &head);

    // Test list_entry
    retrieved = list_entry(&node2->list, test_node, list);
    TEST_ASSERT_EQ(retrieved, node2, "list_entry: correct structure retrieved");
    TEST_ASSERT_EQ(retrieved->value, 20, "list_entry: value is correct");

    // Test list_first_entry
    retrieved = list_first_entry(&head, test_node, list);
    TEST_ASSERT_EQ(retrieved, node1, "list_first_entry: returns first node");
    TEST_ASSERT_EQ(retrieved->value, 10, "list_first_entry: value is correct");

    // Test list_last_entry
    retrieved = list_last_entry(&head, test_node, list);
    TEST_ASSERT_EQ(retrieved, node3, "list_last_entry: returns last node");
    TEST_ASSERT_EQ(retrieved->value, 30, "list_last_entry: value is correct");

    // Test list_next_entry
    retrieved = list_first_entry(&head, test_node, list);
    retrieved = list_next_entry(retrieved, list);
    TEST_ASSERT_EQ(retrieved, node2, "list_next_entry: returns second node");

    // Test list_prev_entry
    retrieved = list_last_entry(&head, test_node, list);
    retrieved = list_prev_entry(retrieved, list);
    TEST_ASSERT_EQ(retrieved, node2, "list_prev_entry: returns middle node");

    // Test list_first_entry on empty list
    list_head empty_head;
    init_list_head(&empty_head);
    retrieved = list_first_entry(&empty_head, test_node, list);
    TEST_ASSERT_NULL(retrieved, "list_first_entry: returns NULL for empty list");

    // Test list_last_entry on empty list
    retrieved = list_last_entry(&empty_head, test_node, list);
    TEST_ASSERT_NULL(retrieved, "list_last_entry: returns NULL for empty list");

    TEST_PASS("entry access");
}

// ============================================================================
// 10. List Traversal Tests
// ============================================================================

static void test_traversal(void) {
    TEST_INFO("Testing list traversal");

    reset_nodes();

    list_head head;
    test_node* node1, *node2, *node3, *node4;
    list_head* pos;
    int sum = 0;
    int count = 0;

    init_list_head(&head);
    node1 = create_node(10);
    node2 = create_node(20);
    node3 = create_node(30);
    node4 = create_node(40);

    list_add_tail(&node1->list, &head);
    list_add_tail(&node2->list, &head);
    list_add_tail(&node3->list, &head);
    list_add_tail(&node4->list, &head);

    // Test list_for_each
    sum = 0;
    list_for_each(pos, &head) {
        test_node* node = list_entry(pos, test_node, list);
        sum += node->value;
    }
    TEST_ASSERT_EQ(sum, 100, "list_for_each: sum of all values");

    // Test list_for_each_prev (reverse)
    sum = 0;
    list_for_each_prev(pos, &head) {
        test_node* node = list_entry(pos, test_node, list);
        sum += node->value;
    }
    TEST_ASSERT_EQ(sum, 100, "list_for_each_prev: sum of all values (reverse)");

    // Test list_for_each_entry
    sum = 0;
    count = 0;
    test_node* entry;
    list_for_each_entry(entry, &head, list) {
        sum += entry->value;
        count++;
    }
    TEST_ASSERT_EQ(sum, 100, "list_for_each_entry: sum of all values");
    TEST_ASSERT_EQ(count, 4, "list_for_each_entry: correct count");

    // Test list_for_each_entry_reverse
    sum = 0;
    count = 0;
    list_for_each_entry_reverse(entry, &head, list) {
        sum += entry->value;
        count++;
    }
    TEST_ASSERT_EQ(sum, 100, "list_for_each_entry_reverse: sum of all values");
    TEST_ASSERT_EQ(count, 4, "list_for_each_entry_reverse: correct count");

    TEST_PASS("list traversal");
}

// ============================================================================
// 11. Safe Traversal Tests (with deletion)
// ============================================================================

static void test_safe_traversal(void) {
    TEST_INFO("Testing safe traversal with deletion");

    reset_nodes();

    list_head head;
    test_node nodes[5];
    list_head* pos, *n;
    int count = 0;

    init_list_head(&head);

    // Create 5 nodes with even and odd values
    for (int i = 0; i < 5; i++) {
        nodes[i].value = i * 10;
        INIT_LIST_HEAD(&nodes[i].list);
        list_add_tail(&nodes[i].list, &head);
    }

    // Test list_for_each_safe - delete all nodes
    list_for_each_safe(pos, n, &head) {
        list_del(pos);
        count++;
    }
    TEST_ASSERT_EQ(count, 5, "list_for_each_safe: deleted 5 nodes");
    TEST_ASSERT_TRUE(list_is_empty(&head), "list_for_each_safe: list is empty");

    // Refill and test list_for_each_entry_safe - delete even values
    for (int i = 0; i < 5; i++) {
        nodes[i].value = i;
        INIT_LIST_HEAD(&nodes[i].list);
        list_add_tail(&nodes[i].list, &head);
    }

    test_node* entry;
    test_node* tmp;
    int sum = 0;

    list_for_each_entry_safe(entry, tmp, &head, list) {
        if (entry->value % 2 == 0) {
            list_del(&entry->list);
        } else {
            sum += entry->value;
        }
    }

    TEST_ASSERT_EQ(sum, 9, "list_for_each_entry_safe: sum of remaining (1+3+5=9)");
    TEST_ASSERT_EQ(list_count(&head), (size_t)3, "list_for_each_entry_safe: 3 nodes remaining");

    TEST_PASS("safe traversal with deletion");
}

// ============================================================================
// 12. Splice Tests
// ============================================================================

static void test_splice(void) {
    TEST_INFO("Testing list splice operations");

    reset_nodes();

    list_head list1, list2;
    test_node* n1, *n2, *n3;
    test_node* m1, *m2;

    init_list_head(&list1);
    init_list_head(&list2);

    n1 = create_node(1);
    n2 = create_node(2);
    n3 = create_node(3);
    m1 = create_node(10);
    m2 = create_node(20);

    // list1: 1 -> 2 -> 3
    list_add_tail(&n1->list, &list1);
    list_add_tail(&n2->list, &list1);
    list_add_tail(&n3->list, &list1);

    // list2: 10 -> 20
    list_add_tail(&m1->list, &list2);
    list_add_tail(&m2->list, &list2);

    // Test list_splice (insert list2 at head of list1)
    list_splice(&list2, &list1);

    // list1 should be: 10 -> 20 -> 1 -> 2 -> 3
    test_node* entry;
    int values[5];
    int idx = 0;

    list_for_each_entry(entry, &list1, list) {
        values[idx++] = entry->value;
    }

    TEST_ASSERT_EQ(values[0], 10, "list_splice: first element is 10");
    TEST_ASSERT_EQ(values[1], 20, "list_splice: second element is 20");
    TEST_ASSERT_EQ(values[2], 1, "list_splice: third element is 1");
    TEST_ASSERT_EQ(list_count(&list1), (size_t)5, "list_splice: combined count is 5");

    TEST_PASS("list splice");
}

// ============================================================================
// 13. Splice Init Tests
// ============================================================================

static void test_splice_init(void) {
    TEST_INFO("Testing list_splice_init");

    reset_nodes();

    list_head list1, list2;
    test_node* n1, *m1;

    init_list_head(&list1);
    init_list_head(&list2);

    n1 = create_node(1);
    m1 = create_node(10);

    list_add_tail(&n1->list, &list1);
    list_add_tail(&m1->list, &list2);

    list_splice_init(&list2, &list1);

    TEST_ASSERT_EQ(list_count(&list1), (size_t)2, "list_splice_init: list1 has 2 elements");
    TEST_ASSERT_TRUE(list_is_empty(&list2), "list_splice_init: list2 is empty and reinitialized");
    TEST_ASSERT_FALSE(list_on_list(&m1->list), "list_splice_init: list2 head is no longer on a list");

    TEST_PASS("list_splice_init");
}

// ============================================================================
// 14. Cut Position Tests
// ============================================================================

static void test_cut_position(void) {
    TEST_INFO("Testing list_cut_position");

    reset_nodes();

    list_head list1, list2;
    test_node* nodes[5];

    init_list_head(&list1);
    init_list_head(&list2);

    // Create list: 0 -> 10 -> 20 -> 30 -> 40
    for (int i = 0; i < 5; i++) {
        nodes[i] = create_node(i * 10);
        list_add_tail(&nodes[i]->list, &list1);
    }

    // Cut at node2 (index 2, value 20)
    // list1 should have: 0 -> 10
    // list2 should have: 20 -> 30 -> 40
    list_cut_position(&list2, &list1, &nodes[2]->list);

    TEST_ASSERT_EQ(list_count(&list1), (size_t)2, "list_cut_position: list1 has 2 elements");
    TEST_ASSERT_EQ(list_count(&list2), (size_t)3, "list_cut_position: list2 has 3 elements");

    test_node* entry;
    int sum = 0;

    list_for_each_entry(entry, &list1, list) {
        sum += entry->value;
    }
    TEST_ASSERT_EQ(sum, 10, "list_cut_position: list1 sum is 10 (0+10)");

    sum = 0;
    list_for_each_entry(entry, &list2, list) {
        sum += entry->value;
    }
    TEST_ASSERT_EQ(sum, 90, "list_cut_position: list2 sum is 90 (20+30+40)");

    // Test cutting entire list
    reset_nodes();
    init_list_head(&list1);
    init_list_head(&list2);

    for (int i = 0; i < 3; i++) {
        nodes[i] = create_node(i * 10);
        list_add_tail(&nodes[i]->list, &list1);
    }

    list_cut_position(&list2, &list1, &list1);

    TEST_ASSERT_TRUE(list_is_empty(&list2), "list_cut_position: cutting at head leaves list2 empty");
    TEST_ASSERT_EQ(list_count(&list1), (size_t)3, "list_cut_position: list1 unchanged");

    TEST_PASS("list_cut_position");
}

// ============================================================================
// 15. Safe Entry Access Tests
// ============================================================================

static void test_safe_entry_access(void) {
    TEST_INFO("Testing safe entry access macros");

    reset_nodes();

    list_head head;
    test_node* node1, *node2, *node3;
    test_node* retrieved;

    init_list_head(&head);
    node1 = create_node(10);
    node2 = create_node(20);
    node3 = create_node(30);

    list_add_tail(&node1->list, &head);
    list_add_tail(&node2->list, &head);
    list_add_tail(&node3->list, &head);

    // Test list_next_entry_safe
    retrieved = node2;
    retrieved = list_next_entry_safe(retrieved, &head, list);
    TEST_ASSERT_EQ(retrieved, node3, "list_next_entry_safe: returns node3");

    retrieved = node3;
    retrieved = list_next_entry_safe(retrieved, &head, list);
    TEST_ASSERT_NULL(retrieved, "list_next_entry_safe: returns NULL at end of list");

    // Test list_prev_entry_safe
    retrieved = node2;
    retrieved = list_prev_entry_safe(retrieved, &head, list);
    TEST_ASSERT_EQ(retrieved, node1, "list_prev_entry_safe: returns node1");

    retrieved = node1;
    retrieved = list_prev_entry_safe(retrieved, &head, list);
    TEST_ASSERT_NULL(retrieved, "list_prev_entry_safe: returns NULL at start of list");

    TEST_PASS("safe entry access");
}

// ============================================================================
// 16. Edge Cases Tests
// ============================================================================

static void test_edge_cases(void) {
    TEST_INFO("Testing edge cases");

    reset_nodes();

    list_head head;
    test_node* node;

    init_list_head(&head);

    // Test operations on empty list
    TEST_ASSERT_TRUE(list_is_empty(&head), "edge: empty list is empty");
    TEST_ASSERT_FALSE(list_is_singular(&head), "edge: empty list is not singular");
    TEST_ASSERT_EQ(list_count(&head), (size_t)0, "edge: empty list count is 0");

    // Test single element list
    node = create_node(42);
    list_add(&node->list, &head);

    TEST_ASSERT_TRUE(list_is_singular(&head), "edge: single element list is singular");
    TEST_ASSERT_TRUE(list_is_last(&node->list, &head), "edge: single element is also last");
    TEST_ASSERT_EQ(list_count(&head), (size_t)1, "edge: single element count");

    // Test adding and removing same element multiple times
    list_del(&node->list);
    TEST_ASSERT_TRUE(list_is_empty(&head), "edge: list empty after removal");

    list_add(&node->list, &head);
    list_del(&node->list);
    TEST_ASSERT_TRUE(list_is_empty(&head), "edge: list empty after second removal");

    // Test node in multiple lists using separate list_head members
    list_head list1, list2;
    multi_list_node mln;

    mln.value = 200;
    INIT_LIST_HEAD(&mln.list1);
    INIT_LIST_HEAD(&mln.list2);

    init_list_head(&list1);
    init_list_head(&list2);

    list_add_tail(&mln.list1, &list1);
    list_add_tail(&mln.list2, &list2);

    TEST_ASSERT_EQ(list_count(&list1), (size_t)1, "edge: node in list1 via list1 member");
    TEST_ASSERT_EQ(list_count(&list2), (size_t)1, "edge: node in list2 via list2 member");

    TEST_PASS("edge cases");
}

// ============================================================================
// 17. Large List Tests
// ============================================================================

static void test_large_list(void) {
    TEST_INFO("Testing large list operations");

    reset_nodes();

    list_head head;
    test_node* nodes[100];

    init_list_head(&head);

    // Create list with 100 elements
    for (int i = 0; i < 100; i++) {
        nodes[i] = create_node(i);
        list_add_tail(&nodes[i]->list, &head);
    }

    TEST_ASSERT_EQ(list_count(&head), (size_t)100, "large: 100 elements added");

    // Verify sum of 0-99
    int sum = 0;
    test_node* entry;
    list_for_each_entry(entry, &head, list) {
        sum += entry->value;
    }
    TEST_ASSERT_EQ(sum, 4950, "large: sum is 4950 (0+1+...+99)");

    // Delete every other element
    test_node* tmp;
    list_for_each_entry_safe(entry, tmp, &head, list) {
        if (entry->value % 2 == 0) {
            list_del(&entry->list);
        }
    }

    TEST_ASSERT_EQ(list_count(&head), (size_t)50, "large: 50 elements after deletion");

    // Verify only odd numbers remain
    sum = 0;
    list_for_each_entry(entry, &head, list) {
        TEST_ASSERT_TRUE(entry->value % 2 == 1, "large: only odd values remain");
        sum += entry->value;
    }

    // Sum of odd numbers from 1-99: 1+3+5+...+99 = 2500
    TEST_ASSERT_EQ(sum, 2500, "large: sum of odd numbers is 2500");

    TEST_PASS("large list operations");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    TEST_RUNNER_BEGIN("List Test Suite");

    TEST_INFO("Running initialization tests...");
    test_init();

    TEST_INFO("Running basic insertion tests...");
    test_insertion();

    TEST_INFO("Running tail insertion tests...");
    test_tail_insertion();

    TEST_INFO("Running deletion tests...");
    test_deletion();

    TEST_INFO("Running delete and reinitialize tests...");
    test_del_init();

    TEST_INFO("Running replace tests...");
    test_replace();

    TEST_INFO("Running replace and reinitialize tests...");
    test_replace_init();

    TEST_INFO("Running list state query tests...");
    test_list_queries();

    TEST_INFO("Running entry access tests...");
    test_entry_access();

    TEST_INFO("Running list traversal tests...");
    test_traversal();

    TEST_INFO("Running safe traversal tests...");
    test_safe_traversal();

    TEST_INFO("Running splice tests...");
    test_splice();

    TEST_INFO("Running splice init tests...");
    test_splice_init();

    TEST_INFO("Running cut position tests...");
    test_cut_position();

    TEST_INFO("Running safe entry access tests...");
    test_safe_entry_access();

    TEST_INFO("Running edge case tests...");
    test_edge_cases();

    TEST_INFO("Running large list tests...");
    test_large_list();

    TEST_RUNNER_END();
}
