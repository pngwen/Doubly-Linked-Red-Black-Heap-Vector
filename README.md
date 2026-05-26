# 🏆 The Doubly-Linked Red-Black Heap Vector Tree 🏆
### *The Last Data Structure You Will Ever Need™*

---

> *"In the beginning, God created the heavens and the earth. Then, after a well-deserved rest, a higher power created the Doubly-Linked Red-Black Heap Vector Tree, and the engineers wept tears of joy."*
>
> — Nobody, yet. But give it time.

---

## Why Does This Exist?

Throughout the long and storied history of computer science, humanity has been burdened with an unconscionable choice: **pick one data structure and live with the consequences.**

Do you want fast sorted lookup? *Use a tree.* Do you want cache-friendly sequential access? *Use a vector.* Do you want insertion-order memory? *Use a linked list.* Do you need priority-queue semantics? *Use a heap.*

What if you want **all four simultaneously, over the same data, with zero copies**?

Computer scientists will tell you this is unreasonable. They will cite "separation of concerns." They will mutter about "single responsibility." They will gently suggest you "just use a database."

**Those people lack vision.**

The `DLRBHeapVector` stores every element exactly once in a flat `std::vector<Node>` and simultaneously maintains:

| View | What you get |
|---|---|
| **Vector** | Raw storage-order access; the pure, unfiltered truth of insertion sequence |
| **Doubly-Linked List** | Bidirectional traversal in insertion order, with O(1) removal from anywhere |
| **Red-Black Tree** | A perfectly balanced, self-healing BST giving O(log n) sorted access and lookup |
| **Binary Heap** | A priority queue with O(1) top-of-heap access and O(log n) push/pop |

Four data structures. One allocation. Zero excuses.

---

## Requirements

- A C++17 compiler (GCC ≥ 7, Clang ≥ 5, MSVC ≥ 19.14)
- A willingness to question everything your data-structures professor told you
- `make` (optional, but it makes you feel professional)

---

## Installation

It's a single header. Copy it. You're done. No CMake incantations. No package manager rituals. No spiritual preparation required (though it is recommended).

```bash
cp include/dlrb_heap_vector.hpp /your/project/include/
```

```cpp
#include "dlrb_heap_vector.hpp"
using namespace dlrb;
```

That's it. You are now in possession of the most overengineered general-purpose container in the known universe.

---

## Quick Start

```cpp
#include "dlrb_heap_vector.hpp"
using namespace dlrb;

// Default: RB tree uses std::less<T> (ascending),
//          heap uses std::greater<T> (max-heap).
DLRBHeapVector<int> c;

// insert() returns a stable node index — bookmark it for O(1) access later.
std::size_t idx_a = c.insert(5);
std::size_t idx_b = c.insert(3);
std::size_t idx_c = c.insert(9);
std::size_t idx_d = c.insert(1);
std::size_t idx_e = c.insert(7);

// Direct access by index is O(1) and never invalidated by reallocation.
c[idx_a] = 5; // still 5. Profound.

// The four views of the same five values:
// vector : 5 3 9 1 7   (insertion order)
// list   : 5 3 9 1 7   (also insertion order)
// rb     : 1 3 5 7 9   (sorted ascending)
// heap   : 9 7 5 1 3   (heap array order; top is 9)
```

---

## Template Parameters

```cpp
template<
    typename T,
    typename RBComp   = std::less<T>,     // RB tree comparator
    typename HeapComp = std::greater<T>   // Heap comparator
>
class DLRBHeapVector;
```

| Parameter | Default | Effect |
|---|---|---|
| `T` | *(required)* | The type of element stored |
| `RBComp` | `std::less<T>` | Comparison for the red-black BST; default gives ascending sorted order |
| `HeapComp` | `std::greater<T>` | Comparison for the binary heap; default gives a **max-heap** |

### Example: min-heap with descending RB tree

```cpp
// Now rb_view() iterates highest-to-lowest,
// and heap_top() returns the minimum element.
DLRBHeapVector<int, std::greater<int>, std::less<int>> c;
```

### Example: custom struct with lambdas

```cpp
struct Task { int priority; std::string name; };

auto by_priority = [](const Task& a, const Task& b) {
    return a.priority < b.priority;
};

DLRBHeapVector<Task, decltype(by_priority), decltype(by_priority)> c(
    by_priority, by_priority
);
```

---

## Core API

### Insertion

```cpp
std::size_t idx = c.insert(value);      // copy
std::size_t idx = c.insert(std::move(value)); // move
```

Returns a **stable node index** that:
- Is valid for the lifetime of the element
- Is unaffected by any future insertions or reallocations
- Can be used for O(1) direct access via `c[idx]`
- Is the key to `erase()` — hold onto it if you plan to delete

### Erasure

```cpp
c.erase(idx);   // remove by node index; O(log n) across all four views
```

Every iterator type exposes `.index()` to retrieve the node index, so you can erase from any view:

```cpp
c.erase(c.rb_begin().index());         // erase smallest element
c.erase(c.heap_begin().index());       // erase the heap's top element (pop)
c.erase(c.list_begin().index());       // erase the oldest insertion
c.erase(some_vector_iterator.index()); // erase by storage position
```

### Size & capacity

```cpp
c.size();        // number of live elements
c.empty();       // true if no live elements
c.capacity();    // underlying vector capacity
c.reserve(n);    // pre-allocate for n elements (avoids reallocation)
```

### Convenience accessors

```cpp
c.heap_top();    // const ref to the top-priority element; O(1)
c.rb_min();      // const ref to the RB-tree minimum; O(log n)
c.rb_max();      // const ref to the RB-tree maximum; O(log n)
c[idx];          // direct mutable access by node index; O(1)
```

### Lookup

```cpp
auto it = c.rb_find(value);   // O(log n) search in the RB tree
if (it != c.rb_end()) {
    // found
    c.erase(it.index());
}
```

---

## Iterators

Each view has its own iterator type with full `const` variants and standard typedefs. All four support range-based `for` loops through the corresponding `*_view()` helper.

### Vector iterator — storage order

```cpp
// Mutable
for (auto it = c.vector_begin(); it != c.vector_end(); ++it) { ... }
for (int& v : c.vector_view()) { ... }

// Const (on a const container or explicitly)
const auto& cc = c;
for (const int& v : cc.vector_view()) { ... }
```

Bidirectional. Skips dead (erased) slots automatically. End sentinel is `nodes_.size()`.

### List iterator — insertion order

```cpp
for (auto it = c.list_begin(); it != c.list_end(); ++it) { ... }
for (int& v : c.list_view()) { ... }
```

Bidirectional. Follows embedded `list_prev` / `list_next` links.  
`--c.list_end()` reaches the tail, just like `std::list`.

```cpp
// Walk backwards from tail to head
auto it = c.list_end();
--it;  // now at the most-recently-inserted surviving element
while (true) {
    use(*it);
    if (it == c.list_begin()) break;
    --it;
}
```

### RB iterator — sorted order

```cpp
for (auto it = c.rb_begin(); it != c.rb_end(); ++it) { ... }
for (int& v : c.rb_view()) { ... }
```

Bidirectional in-order traversal. Forward is ascending (with default `RBComp`); backward is descending.  
`--c.rb_end()` reaches the maximum element.

```cpp
// Iterate in reverse sorted order
auto it = c.rb_end();
while (it != c.rb_begin()) {
    --it;
    use(*it);
}
```

### Heap iterator — heap-array order

```cpp
for (auto it = c.heap_begin(); it != c.heap_end(); ++it) { ... }
for (int& v : c.heap_view()) { ... }
```

Forward-only. Iterates the internal heap array: `heap[0]` is always the top, children follow in the standard `2i+1` / `2i+2` layout. This is heap-array order, **not** sorted extraction order — use repeated `heap_top()` + `erase()` for that.

#### Heap sort / priority-queue drain

```cpp
// Extract elements largest-first (max-heap default)
while (!c.empty()) {
    int top = c.heap_top();
    c.erase(c.heap_begin().index()); // O(log n)
}
```

---

## Iterator Summary

| Iterator | `begin` / `end` | Bidirectional? | Order |
|---|---|---|---|
| `vector_iterator` | `vector_begin()` / `vector_end()` | ✅ | Storage (insertion) |
| `list_iterator` | `list_begin()` / `list_end()` | ✅ | Insertion |
| `rb_iterator` | `rb_begin()` / `rb_end()` | ✅ | Sorted by `RBComp` |
| `heap_iterator` | `heap_begin()` / `heap_end()` | ❌ forward only | Heap-array |

Every iterator exposes:
- `operator*` / `operator->` — dereference to `T`
- `.index()` — the stable node index, usable with `erase()` and `operator[]`

---

## Full Example: Task Scheduler

```cpp
#include "dlrb_heap_vector.hpp"
#include <iostream>
#include <string>
using namespace dlrb;

struct Task {
    int         priority;
    std::string name;
};

int main() {
    // RB tree sorted by name; heap ordered by priority (max = runs first)
    auto by_name     = [](const Task& a, const Task& b){ return a.name < b.name; };
    auto by_priority = [](const Task& a, const Task& b){ return a.priority > b.priority; };

    DLRBHeapVector<Task, decltype(by_name), decltype(by_priority)> sched(
        by_name, by_priority
    );

    sched.insert({3, "render"});
    sched.insert({9, "physics"});
    sched.insert({1, "audio"});
    sched.insert({7, "network"});
    sched.insert({5, "input"});

    // Insertion order — the log of what came in
    std::cout << "Arrival order:\n";
    for (auto& t : sched.list_view())
        std::cout << "  [" << t.priority << "] " << t.name << '\n';

    // Sorted alphabetically — the ops manual
    std::cout << "\nAlphabetical:\n";
    for (auto& t : sched.rb_view())
        std::cout << "  " << t.name << '\n';

    // Execute by priority
    std::cout << "\nExecution order:\n";
    while (!sched.empty()) {
        auto& top = sched.heap_top();
        std::cout << "  running [" << top.priority << "] " << top.name << '\n';
        sched.erase(sched.heap_begin().index());
    }
}
```

Output:
```
Arrival order:
  [3] render
  [9] physics
  [1] audio
  [7] network
  [5] input

Alphabetical:
  audio
  input
  network
  physics
  render

Execution order:
  running [9] physics
  running [7] network
  running [5] input
  running [3] render
  running [1] audio
```

---

## Building & Running Tests

```bash
make          # debug build (default)
make run      # build + run the 20-section, 666-check test suite
make release  # optimised (-O3 -DNDEBUG)
make asan     # AddressSanitizer + UBSanitizer
make clean
make CXX=g++  # override compiler
```

The test suite validates all four views on every mutation, including a 750-operation stress test that drains the entire container via heap-pop while checking invariants at each step.

---

## Complexity Reference

| Operation | Vector | List | RB Tree | Heap |
|---|---|---|---|---|
| `insert` | O(1) amortised | O(1) | O(log n) | O(log n) |
| `erase` | O(1) | O(1) | O(log n) | O(log n) |
| Iterator step | O(1) | O(1) | O(log n) amortised | O(1) |
| Top / min / max | — | — | O(log n) | O(1) |
| `rb_find` | — | — | O(log n) | — |

All operations are dominated by **O(log n)** — the price of maintaining four simultaneous invariants. In exchange, you never have to choose.

---

## Frequently Asked Questions

**Q: Should I actually use this in production?**  
A: The structure is genuinely correct and fully tested under AddressSanitizer. Whether your production codebase *deserves* it is a different question.

**Q: Why does the heap iterator not go backwards?**  
A: The heap has no meaningful backward traversal — it's an array, not a sorted structure. If you want sorted extraction, drain it forward. If you want a different order, use the RB iterator.

**Q: What happens when I erase through one view — do the others update?**  
A: Yes, immediately and completely. `erase(idx)` removes the element from the linked list, the red-black tree, and the heap in a single call. All iterators to *other* elements remain valid.

**Q: Can I store non-copyable types?**  
A: Yes. `insert(T&&)` move-inserts without copying. `T` must be at least move-constructible and move-assignable (required for the free-list slot reuse path).

**Q: This is a ridiculous data structure.**  
A: That is not a question. But you're right, and we stand by it.

---

## License

MIT. Take it, use it, marvel at it. Attribution appreciated but not required.

---

*Built with an unreasonable amount of determination and a single header file.*
