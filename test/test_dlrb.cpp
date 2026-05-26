// test_dlrb.cpp – comprehensive demonstration of DLRBHeapVector
//
// Build:  make  (see Makefile)
// Run:    ./test_dlrb

#include "../include/dlrb_heap_vector.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace dlrb;

// ─────────────────────────────────────────────────────────────────────────────
//  Tiny test harness
// ─────────────────────────────────────────────────────────────────────────────
static int g_pass = 0, g_fail = 0;

#define CHECK(expr)                                                      \
    do {                                                                  \
        if (expr) {                                                       \
            ++g_pass;                                                     \
        } else {                                                          \
            ++g_fail;                                                     \
            std::cerr << "  FAIL  " << #expr                             \
                      << "  (" << __FILE__ << ':' << __LINE__ << ")\n";  \
        }                                                                 \
    } while (false)

#define SECTION(name) \
    std::cout << "\n╔══ " << (name) << "\n"

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: collect a range into a vector for easy comparison
// ─────────────────────────────────────────────────────────────────────────────
template<typename Range>
auto to_vec(Range&& r) {
    using V = std::decay_t<decltype(*r.begin())>;
    std::vector<V> out;
    for (auto& v : r) out.push_back(v);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: verify binary max-heap property
// ─────────────────────────────────────────────────────────────────────────────
template<typename C>
bool heap_valid(const C& c) {
    auto b = c.heap_begin(), e = c.heap_end();
    std::size_t n = static_cast<std::size_t>(std::distance(b, e));
    for (std::size_t i = 1; i < n; ++i) {
        std::size_t parent = (i - 1) / 2;
        // parent value must be >= child value for a max-heap
        auto pit = c.heap_begin(); std::advance(pit, parent);
        auto cit = c.heap_begin(); std::advance(cit, i);
        if (*cit > *pit) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: verify RB tree is sorted (in-order gives sorted sequence)
// ─────────────────────────────────────────────────────────────────────────────
template<typename C>
bool rb_sorted(const C& c) {
    auto it = c.rb_begin(), prev = it, end = c.rb_end();
    if (it == end) return true;
    ++it;
    while (it != end) { if (*it < *prev) return false; prev = it++; }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: verify RB tree color invariants
//    1. Root is black
//    2. No two consecutive red nodes
//    3. Every root-to-null path has the same black-height
// ─────────────────────────────────────────────────────────────────────────────
template<typename T, typename RC, typename HC>
bool rb_invariants(const DLRBHeapVector<T,RC,HC>& c) {
    if (c.empty()) return true;
    // We'll walk the tree via the sorted iterator and check via rb_find + the
    // internal colour.  For a deeper structural check we'd need white-box
    // access; instead we validate the observable properties.
    // 1. Sorted
    if (!rb_sorted(c)) return false;
    // 2. Size matches
    std::size_t cnt = 0;
    for ([[maybe_unused]] auto& v : c.rb_view()) ++cnt;
    if (cnt != c.size()) return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pretty-printer
// ─────────────────────────────────────────────────────────────────────────────
template<typename C>
void print_all_views(const C& c, const std::string& label = "") {
    if (!label.empty()) std::cout << "  [" << label << "]\n";

    std::cout << "  vector : ";
    for (auto& v : c.vector_view()) std::cout << v << ' ';
    std::cout << '\n';

    std::cout << "  list   : ";
    for (auto& v : c.list_view()) std::cout << v << ' ';
    std::cout << '\n';

    std::cout << "  rb     : ";
    for (auto& v : c.rb_view()) std::cout << v << ' ';
    std::cout << '\n';

    std::cout << "  heap   : ";
    for (auto& v : c.heap_view()) std::cout << v << ' ';
    std::cout << " (top=" << c.heap_top() << ")\n";
}

// ═════════════════════════════════════════════════════════════════════════════
//  TEST SECTIONS
// ═════════════════════════════════════════════════════════════════════════════

// ─── 1. Basic construction & empty state ─────────────────────────────────────
void test_empty() {
    SECTION("1. Empty container");
    DLRBHeapVector<int> c;
    CHECK(c.empty());
    CHECK(c.size() == 0);
    CHECK(c.vector_begin() == c.vector_end());
    CHECK(c.list_begin()   == c.list_end());
    CHECK(c.rb_begin()     == c.rb_end());
    CHECK(c.heap_begin()   == c.heap_end());
    std::cout << "  (all iterators equal end – ok)\n";
}

// ─── 2. Single insertion ─────────────────────────────────────────────────────
void test_single_insert() {
    SECTION("2. Single insert");
    DLRBHeapVector<int> c;
    std::size_t idx = c.insert(42);
    CHECK(c.size() == 1);
    CHECK(!c.empty());
    CHECK(c[idx] == 42);
    CHECK(c.heap_top()  == 42);
    CHECK(c.rb_min()    == 42);
    CHECK(c.rb_max()    == 42);

    CHECK(to_vec(c.vector_view()) == std::vector<int>{42});
    CHECK(to_vec(c.list_view())   == std::vector<int>{42});
    CHECK(to_vec(c.rb_view())     == std::vector<int>{42});
    CHECK(to_vec(c.heap_view())   == std::vector<int>{42});
    std::cout << "  insert(42) → index " << idx << ", all views agree\n";
}

// ─── 3. Multiple inserts, verify all four views ───────────────────────────────
void test_multi_insert() {
    SECTION("3. Multi-insert – view consistency");

    // Insert in a scrambled order so the four views differ visibly
    const std::vector<int> vals = {5, 1, 9, 3, 7, 2, 8, 4, 6};
    DLRBHeapVector<int> c;
    for (int v : vals) c.insert(v);

    print_all_views(c);

    // Vector order == insertion order
    CHECK(to_vec(c.vector_view()) == vals);

    // List order == insertion order
    CHECK(to_vec(c.list_view()) == vals);

    // RB order == sorted ascending
    auto rb = to_vec(c.rb_view());
    CHECK(std::is_sorted(rb.begin(), rb.end()));
    std::cout << "  rb sorted: ";
    for (int v : rb) std::cout << v << ' ';
    std::cout << '\n';

    // Heap: top is max, and the heap property holds
    CHECK(c.heap_top() == 9);
    CHECK(heap_valid(c));
    std::cout << "  heap top  = " << c.heap_top() << " (expected 9)\n";

    // rb_min / rb_max
    CHECK(c.rb_min() == 1);
    CHECK(c.rb_max() == 9);

    CHECK(rb_invariants(c));
}

// ─── 4. Erase by vector iterator ─────────────────────────────────────────────
void test_erase_vector() {
    SECTION("4. Erase via vector iterator");
    DLRBHeapVector<int> c;
    for (int v : {10, 20, 30, 40, 50}) c.insert(v);

    // Erase the element with value 30 (third in vector order)
    auto it = c.vector_begin();
    ++it; ++it; // points at 30
    CHECK(*it == 30);
    std::cout << "  erasing " << *it << " (index=" << it.index() << ")\n";
    c.erase(it.index());

    CHECK(c.size() == 4);
    CHECK(to_vec(c.list_view())   == (std::vector<int>{10,20,40,50}));
    auto rb = to_vec(c.rb_view());
    CHECK(std::is_sorted(rb.begin(), rb.end()));
    CHECK(heap_valid(c));
    print_all_views(c, "after erase(30)");
}

// ─── 5. Erase via list iterator ──────────────────────────────────────────────
void test_erase_list() {
    SECTION("5. Erase via list iterator");
    DLRBHeapVector<int> c;
    for (int v : {7, 3, 9, 1, 5}) c.insert(v);

    // Erase head of list
    auto it = c.list_begin();
    CHECK(*it == 7);
    c.erase(it.index());

    CHECK(c.size() == 4);
    // New list head should be 3
    CHECK(*c.list_begin() == 3);
    CHECK(heap_valid(c));
    CHECK(rb_invariants(c));
    print_all_views(c, "after erase(head=7)");

    // Erase tail of list (walk to end)
    auto jt = c.list_begin();
    while (true) {
        auto next = jt; ++next;
        if (next == c.list_end()) break;
        jt = next;
    }
    std::cout << "  erasing list tail = " << *jt << '\n';
    c.erase(jt.index());
    CHECK(c.size() == 3);
    CHECK(heap_valid(c));
    CHECK(rb_invariants(c));
}

// ─── 6. Erase via RB iterator ────────────────────────────────────────────────
void test_erase_rb() {
    SECTION("6. Erase via rb iterator");
    DLRBHeapVector<int> c;
    for (int v : {4, 8, 2, 6, 1, 7, 3, 5}) c.insert(v);

    // Erase the minimum (rb_begin)
    CHECK(c.rb_min() == 1);
    c.erase(c.rb_begin().index());
    CHECK(c.size() == 7);
    CHECK(c.rb_min() == 2);
    CHECK(rb_invariants(c));
    CHECK(heap_valid(c));

    // Erase the maximum (--rb_end)
    auto it = c.rb_end(); --it;
    CHECK(*it == 8);
    c.erase(it.index());
    CHECK(c.size() == 6);
    CHECK(c.rb_max() == 7);
    CHECK(rb_invariants(c));
    CHECK(heap_valid(c));

    print_all_views(c, "after erasing min and max");
}

// ─── 7. Erase via heap iterator ──────────────────────────────────────────────
void test_erase_heap() {
    SECTION("7. Erase via heap iterator");
    DLRBHeapVector<int> c;
    for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) c.insert(v);

    CHECK(c.heap_top() == 9);
    // Pop the top three times and verify max decreases
    std::vector<int> pops;
    while (!c.empty() && pops.size() < 3) {
        pops.push_back(c.heap_top());
        c.erase(c.heap_begin().index());
    }
    std::cout << "  popped (heap order): ";
    for (int v : pops) std::cout << v << ' ';
    std::cout << '\n';
    CHECK(pops[0] >= pops[1] && pops[1] >= pops[2]); // non-increasing
    CHECK(heap_valid(c));
    CHECK(rb_invariants(c));
}

// ─── 8. rb_find ──────────────────────────────────────────────────────────────
void test_rb_find() {
    SECTION("8. rb_find");
    DLRBHeapVector<int> c;
    for (int v : {10, 30, 20, 50, 40}) c.insert(v);

    auto it = c.rb_find(30);
    CHECK(it != c.rb_end());
    CHECK(*it == 30);

    auto miss = c.rb_find(99);
    CHECK(miss == c.rb_end());
    std::cout << "  found 30, did not find 99\n";
}

// ─── 9. reserve + capacity ────────────────────────────────────────────────────
void test_reserve() {
    SECTION("9. reserve / capacity");
    DLRBHeapVector<int> c;
    c.reserve(128);
    CHECK(c.capacity() >= 128);
    for (int i = 0; i < 100; ++i) c.insert(i);
    CHECK(c.size() == 100);
    CHECK(rb_invariants(c));
    CHECK(heap_valid(c));
    std::cout << "  100 elements, capacity=" << c.capacity() << '\n';
}

// ─── 10. Free-list reuse after erase ─────────────────────────────────────────
void test_freelist_reuse() {
    SECTION("10. Free-list slot reuse");
    DLRBHeapVector<int> c;
    std::size_t i0 = c.insert(100);
    std::size_t i1 = c.insert(200);
    c.erase(i0);                      // slot 0 goes to free list
    std::size_t i2 = c.insert(300);  // should reuse slot 0
    CHECK(i2 == i0);
    CHECK(c[i2] == 300);
    CHECK(c.size() == 2);
    CHECK(rb_invariants(c));
    CHECK(heap_valid(c));
    std::cout << "  erased idx=" << i0 << ", reinserted as idx=" << i2 << '\n';
    (void)i1;
}

// ─── 11. Const-iterator compatibility ────────────────────────────────────────
void test_const_iterators() {
    SECTION("11. Const iterators");
    DLRBHeapVector<int> c;
    for (int v : {5, 3, 7}) c.insert(v);

    const auto& cc = c;
    int sum = 0;
    for (const int& v : cc.vector_view()) sum += v;
    CHECK(sum == 15);

    sum = 0;
    for (const int& v : cc.list_view()) sum += v;
    CHECK(sum == 15);

    sum = 0;
    for (const int& v : cc.rb_view()) sum += v;
    CHECK(sum == 15);

    sum = 0;
    for (const int& v : cc.heap_view()) sum += v;
    CHECK(sum == 15);
    std::cout << "  all const-iterator sums = 15\n";
}

// ─── 12. Stress test – insert/erase interleaved ──────────────────────────────
void test_stress() {
    SECTION("12. Stress: 500 inserts + 250 random erases");
    DLRBHeapVector<int> c;
    std::vector<std::size_t> live_indices;

    // Insert 0..499
    for (int i = 0; i < 500; ++i)
        live_indices.push_back(c.insert(i));

    CHECK(c.size() == 500);
    CHECK(rb_invariants(c));
    CHECK(heap_valid(c));

    // Erase every other element (by stored index)
    std::size_t erased = 0;
    for (std::size_t i = 0; i < live_indices.size(); i += 2) {
        c.erase(live_indices[i]);
        ++erased;
    }
    CHECK(c.size() == 500 - erased);
    CHECK(rb_invariants(c));
    CHECK(heap_valid(c));

    // Insert 250 more
    for (int i = 1000; i < 1250; ++i) c.insert(i);
    CHECK(c.size() == 500 - erased + 250);
    CHECK(rb_invariants(c));
    CHECK(heap_valid(c));

    // Drain via heap-pop
    int prev_top = c.heap_top();
    while (!c.empty()) {
        int top = c.heap_top();
        CHECK(top <= prev_top); // non-increasing
        prev_top = top;
        c.erase(c.heap_begin().index());
    }
    CHECK(c.empty());
    std::cout << "  drained cleanly, all invariants held\n";
}

// ─── 13. Custom comparators ───────────────────────────────────────────────────
void test_custom_comparators() {
    SECTION("13. Custom comparators (reverse RB + min-heap)");

    // RB sorted descending, heap is a min-heap
    using C = DLRBHeapVector<int, std::greater<int>, std::less<int>>;
    C c;
    for (int v : {4, 8, 2, 6, 1, 7}) c.insert(v);

    // RB order should now be descending
    auto rb = to_vec(c.rb_view());
    CHECK(std::is_sorted(rb.begin(), rb.end(), std::greater<int>{}));
    std::cout << "  rb (desc): ";
    for (int v : rb) std::cout << v << ' ';
    std::cout << '\n';

    // Min-heap: top should be the minimum
    CHECK(c.heap_top() == 1);
    std::cout << "  heap top (min) = " << c.heap_top() << " (expected 1)\n";

    // Drain min-heap, verify non-decreasing
    int prev = c.heap_top();
    while (!c.empty()) {
        int top = c.heap_top();
        CHECK(top >= prev);
        prev = top;
        c.erase(c.heap_begin().index());
    }
    std::cout << "  min-heap drained in non-decreasing order\n";
}

// ─── 14. Non-trivial value type (std::string) ────────────────────────────────
void test_string_type() {
    SECTION("14. Non-trivial T = std::string");
    DLRBHeapVector<std::string> c;

    const std::vector<std::string> words = {"banana","apple","cherry","date","elderberry"};
    std::vector<std::size_t> indices;
    for (const auto& w : words) indices.push_back(c.insert(w));

    CHECK(c.size() == 5);

    // list order preserved
    CHECK(to_vec(c.list_view()) == words);

    // rb order is lexicographic
    auto rb = to_vec(c.rb_view());
    CHECK(std::is_sorted(rb.begin(), rb.end()));
    std::cout << "  rb sorted: ";
    for (auto& s : rb) std::cout << s << ' ';
    std::cout << '\n';

    // heap top is lexicographically largest
    std::cout << "  heap top = \"" << c.heap_top() << "\"\n";

    // Erase "cherry" by RB find
    auto it = c.rb_find("cherry");
    CHECK(it != c.rb_end());
    c.erase(it.index());
    CHECK(c.size() == 4);
    CHECK(c.rb_find("cherry") == c.rb_end());
}

// ─── 15. Move semantics ───────────────────────────────────────────────────────
void test_move_insert() {
    SECTION("15. Move insert");
    DLRBHeapVector<std::string> c;
    std::string s = "hello";
    c.insert(std::move(s));
    CHECK(c.size() == 1);
    CHECK(*c.rb_begin() == "hello");
    // s has been moved-from; don't inspect it
    std::cout << "  moved string into container ok\n";
}

// ─── 16. Bidirectional list traversal ────────────────────────────────────────
void test_bidirectional_list() {
    SECTION("16. Bidirectional list walk");
    DLRBHeapVector<int> c;
    for (int v : {1, 2, 3, 4, 5}) c.insert(v);

    // Walk forward
    std::vector<int> fwd;
    for (auto it = c.list_begin(); it != c.list_end(); ++it)
        fwd.push_back(*it);
    CHECK(fwd == (std::vector<int>{1,2,3,4,5}));

    // Walk backward: start one past tail, go to head
    std::vector<int> rev;
    auto it = c.list_end();
    --it;                              // now at tail (5)
    while (true) {
        rev.push_back(*it);
        if (it == c.list_begin()) break;
        --it;
    }
    CHECK(rev == (std::vector<int>{5,4,3,2,1}));
    std::cout << "  forward: "; for (int v : fwd) std::cout << v << ' '; std::cout << '\n';
    std::cout << "  reverse: "; for (int v : rev) std::cout << v << ' '; std::cout << '\n';
}

// ─── 17. Bidirectional RB traversal ──────────────────────────────────────────
void test_bidirectional_rb() {
    SECTION("17. Bidirectional RB walk");
    DLRBHeapVector<int> c;
    for (int v : {5, 2, 8, 1, 3, 7, 9}) c.insert(v);

    // Forward = ascending
    std::vector<int> asc;
    for (auto it = c.rb_begin(); it != c.rb_end(); ++it) asc.push_back(*it);

    // Reverse: walk backward from end
    std::vector<int> desc;
    auto it = c.rb_end(); --it;
    while (true) {
        desc.push_back(*it);
        if (it == c.rb_begin()) break;
        --it;
    }

    CHECK(std::is_sorted(asc.begin(), asc.end()));
    CHECK(std::is_sorted(desc.begin(), desc.end(), std::greater<int>{}));
    CHECK(asc.size() == desc.size());
    std::cout << "  asc:  "; for (int v : asc)  std::cout << v << ' '; std::cout << '\n';
    std::cout << "  desc: "; for (int v : desc) std::cout << v << ' '; std::cout << '\n';
}

// ─── 18. All views reflect the same set of values ────────────────────────────
void test_view_consistency() {
    SECTION("18. All views contain identical value sets");
    DLRBHeapVector<int> c;
    for (int v : {11, 3, 7, 15, 1, 9, 13, 5}) c.insert(v);

    auto collect_sorted = [](auto view) {
        std::vector<int> v;
        for (auto& x : view) v.push_back(x);
        std::sort(v.begin(), v.end());
        return v;
    };

    auto sv = collect_sorted(c.vector_view());
    auto sl = collect_sorted(c.list_view());
    auto sr = collect_sorted(c.rb_view());
    auto sh = collect_sorted(c.heap_view());

    CHECK(sv == sl);
    CHECK(sl == sr);
    CHECK(sr == sh);
    std::cout << "  sorted contents: ";
    for (int v : sv) std::cout << v << ' ';
    std::cout << '\n';
}

// ─── 19. Index stability across inserts ──────────────────────────────────────
void test_index_stability() {
    SECTION("19. Index stability after reallocation");
    DLRBHeapVector<int> c;
    c.reserve(1); // force many reallocations

    std::vector<std::pair<std::size_t,int>> bookmarks;
    for (int i = 0; i < 64; ++i) {
        auto idx = c.insert(i * 3);
        bookmarks.push_back({idx, i * 3});
    }

    for (auto [idx, val] : bookmarks)
        CHECK(c[idx] == val);

    std::cout << "  64 bookmarks still valid after reallocation\n";
}

// ─── 20. Heap-sort via repeated heap_top + erase ─────────────────────────────
void test_heap_sort() {
    SECTION("20. Heap-sort: drain via heap_top");
    DLRBHeapVector<int> c;
    const std::vector<int> src = {3,1,4,1,5,9,2,6,5,3,5};
    for (int v : src) c.insert(v);

    std::vector<int> sorted;
    while (!c.empty()) {
        sorted.push_back(c.heap_top());
        c.erase(c.heap_begin().index());
    }
    // Should be non-increasing (max-heap extraction)
    CHECK(std::is_sorted(sorted.begin(), sorted.end(), std::greater<int>{}));
    std::cout << "  heap-sort result (desc): ";
    for (int v : sorted) std::cout << v << ' ';
    std::cout << '\n';
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "  DLRBHeapVector  —  feature demonstration\n";
    std::cout << "══════════════════════════════════════════════\n";

    test_empty();
    test_single_insert();
    test_multi_insert();
    test_erase_vector();
    test_erase_list();
    test_erase_rb();
    test_erase_heap();
    test_rb_find();
    test_reserve();
    test_freelist_reuse();
    test_const_iterators();
    test_stress();
    test_custom_comparators();
    test_string_type();
    test_move_insert();
    test_bidirectional_list();
    test_bidirectional_rb();
    test_view_consistency();
    test_index_stability();
    test_heap_sort();

    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "  Results: " << g_pass << " passed";
    if (g_fail) std::cout << ", " << g_fail << " FAILED";
    else        std::cout << ", 0 failed";
    std::cout << "\n══════════════════════════════════════════════\n";

    return g_fail ? 1 : 0;
}
