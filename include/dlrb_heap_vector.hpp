// dlrb_heap_vector.hpp  —  Doubly-Linked Red-Black Heap Vector
//
// A single vector<Node> backs four simultaneous views of the same data:
//   • vector  : storage-order traversal (insertion index)
//   • list    : doubly-linked, maintains insertion order by default
//   • rb      : red-black BST sorted by RBComp  (default std::less<T>)
//   • heap    : binary max-heap by HeapComp     (default std::greater<T>)
//
// All intra-structure links are vector indices, so reallocation never
// breaks the other two views.  Deletion is O(log n) across all views.

#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace dlrb {

// ─────────────────────────────────────────────────────────────────────────────
//  Sentinel index used in place of NULL pointers
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t NULL_IDX = static_cast<std::size_t>(-1);

enum class Color : bool { Red = false, Black = true };

// ─────────────────────────────────────────────────────────────────────────────
//  Node
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
struct Node {
    T           value;
    // list links
    std::size_t list_prev = NULL_IDX;
    std::size_t list_next = NULL_IDX;
    // red-black links
    std::size_t rb_parent = NULL_IDX;
    std::size_t rb_left   = NULL_IDX;
    std::size_t rb_right  = NULL_IDX;
    Color       rb_color  = Color::Red;
    // heap position (index inside heap_ array, not nodes_ array)
    std::size_t heap_pos  = NULL_IDX;
    bool        alive     = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
template<typename T, typename RBComp, typename HeapComp> class DLRBHeapVector;
template<typename Container, bool Const> class VectorIterator;
template<typename Container, bool Const> class ListIterator;
template<typename Container, bool Const> class RBIterator;
template<typename Container, bool Const> class HeapIterator;

// ─────────────────────────────────────────────────────────────────────────────
//  IterView – lightweight range adaptor for range-based for
// ─────────────────────────────────────────────────────────────────────────────
template<typename Iter>
struct IterView {
    Iter b_, e_;
    Iter begin() const { return b_; }
    Iter end()   const { return e_; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  VectorIterator – storage-order, skips dead slots
// ─────────────────────────────────────────────────────────────────────────────
template<typename Container, bool Const>
class VectorIterator {
    using CPtr = std::conditional_t<Const, const Container*, Container*>;
    CPtr        c_;
    std::size_t idx_;

    void skip_dead() {
        while (idx_ < c_->nodes_.size() && !c_->nodes_[idx_].alive)
            ++idx_;
    }

public:
    using value_type        = typename Container::value_type;
    using reference         = std::conditional_t<Const, const value_type&, value_type&>;
    using pointer           = std::conditional_t<Const, const value_type*, value_type*>;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    VectorIterator() : c_(nullptr), idx_(NULL_IDX) {}
    VectorIterator(CPtr c, std::size_t idx) : c_(c), idx_(idx) { skip_dead(); }

    reference   operator*()  const { return  c_->nodes_[idx_].value; }
    pointer     operator->() const { return &c_->nodes_[idx_].value; }
    std::size_t index()      const { return idx_; }

    VectorIterator& operator++() { ++idx_; skip_dead(); return *this; }
    VectorIterator  operator++(int) { auto t = *this; ++*this; return t; }

    VectorIterator& operator--() {
        while (idx_ > 0) { --idx_; if (c_->nodes_[idx_].alive) return *this; }
        idx_ = NULL_IDX;
        return *this;
    }
    VectorIterator operator--(int) { auto t = *this; --*this; return t; }

    bool operator==(const VectorIterator& o) const { return idx_ == o.idx_; }
    bool operator!=(const VectorIterator& o) const { return idx_ != o.idx_; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ListIterator – bidirectional, follows embedded list links
// ─────────────────────────────────────────────────────────────────────────────
template<typename Container, bool Const>
class ListIterator {
    using CPtr = std::conditional_t<Const, const Container*, Container*>;
    CPtr        c_;
    std::size_t idx_;

public:
    using value_type        = typename Container::value_type;
    using reference         = std::conditional_t<Const, const value_type&, value_type&>;
    using pointer           = std::conditional_t<Const, const value_type*, value_type*>;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    ListIterator() : c_(nullptr), idx_(NULL_IDX) {}
    ListIterator(CPtr c, std::size_t idx) : c_(c), idx_(idx) {}

    reference   operator*()  const { return  c_->nodes_[idx_].value; }
    pointer     operator->() const { return &c_->nodes_[idx_].value; }
    std::size_t index()      const { return idx_; }

    ListIterator& operator++() { idx_ = c_->nodes_[idx_].list_next; return *this; }
    ListIterator  operator++(int) { auto t = *this; ++*this; return t; }
    ListIterator& operator--() {
        // Decrementing end() (NULL_IDX) steps to the tail node, mirroring
        // the behaviour of std::list::iterator and RBIterator::operator--.
        if (idx_ == NULL_IDX) idx_ = c_->list_tail_;
        else                  idx_ = c_->nodes_[idx_].list_prev;
        return *this;
    }
    ListIterator  operator--(int) { auto t = *this; --*this; return t; }

    bool operator==(const ListIterator& o) const { return idx_ == o.idx_; }
    bool operator!=(const ListIterator& o) const { return idx_ != o.idx_; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  RBIterator – bidirectional in-order (sorted) traversal
// ─────────────────────────────────────────────────────────────────────────────
template<typename Container, bool Const>
class RBIterator {
    using CPtr = std::conditional_t<Const, const Container*, Container*>;
    CPtr        c_;
    std::size_t idx_;

    static std::size_t leftmost(CPtr c, std::size_t n) {
        while (n != NULL_IDX && c->nodes_[n].rb_left != NULL_IDX)
            n = c->nodes_[n].rb_left;
        return n;
    }
    static std::size_t rightmost(CPtr c, std::size_t n) {
        while (n != NULL_IDX && c->nodes_[n].rb_right != NULL_IDX)
            n = c->nodes_[n].rb_right;
        return n;
    }
    static std::size_t successor(CPtr c, std::size_t n) {
        if (c->nodes_[n].rb_right != NULL_IDX)
            return leftmost(c, c->nodes_[n].rb_right);
        std::size_t p = c->nodes_[n].rb_parent;
        while (p != NULL_IDX && n == c->nodes_[p].rb_right) { n = p; p = c->nodes_[p].rb_parent; }
        return p;
    }
    static std::size_t predecessor(CPtr c, std::size_t n) {
        if (c->nodes_[n].rb_left != NULL_IDX)
            return rightmost(c, c->nodes_[n].rb_left);
        std::size_t p = c->nodes_[n].rb_parent;
        while (p != NULL_IDX && n == c->nodes_[p].rb_left) { n = p; p = c->nodes_[p].rb_parent; }
        return p;
    }

public:
    using value_type        = typename Container::value_type;
    using reference         = std::conditional_t<Const, const value_type&, value_type&>;
    using pointer           = std::conditional_t<Const, const value_type*, value_type*>;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    RBIterator() : c_(nullptr), idx_(NULL_IDX) {}
    RBIterator(CPtr c, std::size_t idx) : c_(c), idx_(idx) {}

    reference   operator*()  const { return  c_->nodes_[idx_].value; }
    pointer     operator->() const { return &c_->nodes_[idx_].value; }
    std::size_t index()      const { return idx_; }

    RBIterator& operator++() { idx_ = successor(c_, idx_); return *this; }
    RBIterator  operator++(int) { auto t = *this; ++*this; return t; }

    RBIterator& operator--() {
        if (idx_ == NULL_IDX)
            idx_ = rightmost(c_, c_->rb_root_);
        else
            idx_ = predecessor(c_, idx_);
        return *this;
    }
    RBIterator operator--(int) { auto t = *this; --*this; return t; }

    bool operator==(const RBIterator& o) const { return idx_ == o.idx_; }
    bool operator!=(const RBIterator& o) const { return idx_ != o.idx_; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  HeapIterator – forward, iterates the heap array in heap order
//  heap[0] is always the top (max by default)
// ─────────────────────────────────────────────────────────────────────────────
template<typename Container, bool Const>
class HeapIterator {
    using CPtr = std::conditional_t<Const, const Container*, Container*>;
    CPtr        c_;
    std::size_t pos_; // position in heap_ array

public:
    using value_type        = typename Container::value_type;
    using reference         = std::conditional_t<Const, const value_type&, value_type&>;
    using pointer           = std::conditional_t<Const, const value_type*, value_type*>;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    HeapIterator() : c_(nullptr), pos_(0) {}
    HeapIterator(CPtr c, std::size_t pos) : c_(c), pos_(pos) {}

    reference   operator*()  const { return  c_->nodes_[c_->heap_[pos_]].value; }
    pointer     operator->() const { return &c_->nodes_[c_->heap_[pos_]].value; }
    std::size_t index()      const { return c_->heap_[pos_]; } // node index

    HeapIterator& operator++() { ++pos_; return *this; }
    HeapIterator  operator++(int) { auto t = *this; ++*this; return t; }

    bool operator==(const HeapIterator& o) const { return pos_ == o.pos_; }
    bool operator!=(const HeapIterator& o) const { return pos_ != o.pos_; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  DLRBHeapVector
// ─────────────────────────────────────────────────────────────────────────────
template<
    typename T,
    typename RBComp   = std::less<T>,
    typename HeapComp = std::greater<T>
>
class DLRBHeapVector {
public:
    // Iterator types need access to private members
    template<typename, bool> friend class VectorIterator;
    template<typename, bool> friend class ListIterator;
    template<typename, bool> friend class RBIterator;
    template<typename, bool> friend class HeapIterator;

    using value_type             = T;
    using size_type              = std::size_t;
    using node_type              = Node<T>;

    using vector_iterator        = VectorIterator<DLRBHeapVector, false>;
    using const_vector_iterator  = VectorIterator<DLRBHeapVector, true>;
    using list_iterator          = ListIterator<DLRBHeapVector, false>;
    using const_list_iterator    = ListIterator<DLRBHeapVector, true>;
    using rb_iterator            = RBIterator<DLRBHeapVector, false>;
    using const_rb_iterator      = RBIterator<DLRBHeapVector, true>;
    using heap_iterator          = HeapIterator<DLRBHeapVector, false>;
    using const_heap_iterator    = HeapIterator<DLRBHeapVector, true>;

    // ── Construction ──────────────────────────────────────────────────────────
    DLRBHeapVector() = default;
    explicit DLRBHeapVector(RBComp rb_comp, HeapComp heap_comp = HeapComp{})
        : rb_comp_(std::move(rb_comp)), heap_comp_(std::move(heap_comp)) {}

    // ── Capacity ──────────────────────────────────────────────────────────────
    std::size_t size()     const noexcept { return live_count_; }
    bool        empty()    const noexcept { return live_count_ == 0; }
    std::size_t capacity() const noexcept { return nodes_.capacity(); }
    void        reserve(std::size_t n) { nodes_.reserve(n); heap_.reserve(n); }

    // ── Insertion — append to list tail (default) ────────────────────────────
    // Returns the stable node index for the inserted element.
    std::size_t insert(const T& val) { return insert_impl(T(val)); }
    std::size_t insert(T&&      val) { return insert_impl(std::move(val)); }

    // ── Insertion — positional in the linked list ─────────────────────────────
    // The new element is placed at the requested list position; its position
    // in the RB tree and heap is determined by its value as normal.

    // Insert at the head of the list.
    std::size_t insert_front(const T& val) { return insert_front_impl(T(val)); }
    std::size_t insert_front(T&&      val) { return insert_front_impl(std::move(val)); }

    // Insert immediately after the node at after_idx.
    std::size_t insert_after(const T& val, std::size_t after_idx) {
        return insert_after_impl(T(val), after_idx);
    }
    std::size_t insert_after(T&& val, std::size_t after_idx) {
        return insert_after_impl(std::move(val), after_idx);
    }

    // Insert immediately before the node at before_idx.
    std::size_t insert_before(const T& val, std::size_t before_idx) {
        return insert_before_impl(T(val), before_idx);
    }
    std::size_t insert_before(T&& val, std::size_t before_idx) {
        return insert_before_impl(std::move(val), before_idx);
    }

    // ── List reordering — move an existing node to a new list position ────────
    // All moves are O(1).  The RB tree and heap are completely unaffected;
    // only the list_prev / list_next links of the touched nodes change.

    // Move node idx to the front (head) of the list.
    void list_move_to_front(std::size_t idx) {
        assert(idx < nodes_.size() && nd(idx).alive);
        if (idx == list_head_) return;
        list_remove(idx);
        list_push_front(idx);
    }

    // Move node idx to the back (tail) of the list.
    void list_move_to_back(std::size_t idx) {
        assert(idx < nodes_.size() && nd(idx).alive);
        if (idx == list_tail_) return;
        list_remove(idx);
        list_push_back(idx);
    }

    // Move node idx to immediately after after_idx.
    // Passing idx == after_idx, or after_idx already being idx's predecessor,
    // are both no-ops.
    void list_move_after(std::size_t idx, std::size_t after_idx) {
        assert(idx < nodes_.size() && nd(idx).alive);
        assert(after_idx < nodes_.size() && nd(after_idx).alive);
        if (idx == after_idx)                   return;
        if (nd(after_idx).list_next == idx)     return; // already in place
        list_remove(idx);
        list_splice_after(idx, after_idx);
    }

    // Move node idx to immediately before before_idx.
    void list_move_before(std::size_t idx, std::size_t before_idx) {
        assert(idx < nodes_.size() && nd(idx).alive);
        assert(before_idx < nodes_.size() && nd(before_idx).alive);
        if (idx == before_idx)                   return;
        if (nd(before_idx).list_prev == idx)     return; // already in place
        list_remove(idx);
        std::size_t prev = nd(before_idx).list_prev;
        if (prev == NULL_IDX) list_push_front(idx);
        else                  list_splice_after(idx, prev);
    }

    // ── Erasure ───────────────────────────────────────────────────────────────
    // All iterator types expose .index() to get the node index for erase().
    void erase(std::size_t idx);

    // ── Direct access ─────────────────────────────────────────────────────────
    T&       operator[](std::size_t idx)       { return nodes_[idx].value; }
    const T& operator[](std::size_t idx) const { return nodes_[idx].value; }

    // ── Heap extremum ─────────────────────────────────────────────────────────
    T&       heap_top()       { return nodes_[heap_[0]].value; }
    const T& heap_top() const { return nodes_[heap_[0]].value; }

    // ── RB extrema ────────────────────────────────────────────────────────────
    T&       rb_min()       { return nodes_[rb_leftmost(rb_root_)].value; }
    const T& rb_min() const { return nodes_[rb_leftmost(rb_root_)].value; }
    T&       rb_max()       { return nodes_[rb_rightmost(rb_root_)].value; }
    const T& rb_max() const { return nodes_[rb_rightmost(rb_root_)].value; }

    // ── Vector view ───────────────────────────────────────────────────────────
    vector_iterator       vector_begin()       { return { this, 0 }; }
    vector_iterator       vector_end()         { return { this, nodes_.size() }; }
    const_vector_iterator vector_begin() const { return { this, 0 }; }
    const_vector_iterator vector_end()   const { return { this, nodes_.size() }; }
    IterView<vector_iterator>       vector_view()       { return { vector_begin(), vector_end() }; }
    IterView<const_vector_iterator> vector_view() const { return { vector_begin(), vector_end() }; }

    // ── List view ─────────────────────────────────────────────────────────────
    list_iterator       list_begin()       { return { this, list_head_ }; }
    list_iterator       list_end()         { return { this, NULL_IDX   }; }
    const_list_iterator list_begin() const { return { this, list_head_ }; }
    const_list_iterator list_end()   const { return { this, NULL_IDX   }; }
    IterView<list_iterator>       list_view()       { return { list_begin(), list_end() }; }
    IterView<const_list_iterator> list_view() const { return { list_begin(), list_end() }; }

    // ── Red-black tree view (sorted order) ───────────────────────────────────
    rb_iterator       rb_begin()       { return { this, rb_leftmost(rb_root_) }; }
    rb_iterator       rb_end()         { return { this, NULL_IDX              }; }
    const_rb_iterator rb_begin() const { return { this, rb_leftmost(rb_root_) }; }
    const_rb_iterator rb_end()   const { return { this, NULL_IDX              }; }
    IterView<rb_iterator>       rb_view()       { return { rb_begin(), rb_end() }; }
    IterView<const_rb_iterator> rb_view() const { return { rb_begin(), rb_end() }; }

    // ── Heap view (heap-array order, top is first) ────────────────────────────
    heap_iterator       heap_begin()       { return { this, 0            }; }
    heap_iterator       heap_end()         { return { this, heap_.size() }; }
    const_heap_iterator heap_begin() const { return { this, 0            }; }
    const_heap_iterator heap_end()   const { return { this, heap_.size() }; }
    IterView<heap_iterator>       heap_view()       { return { heap_begin(), heap_end() }; }
    IterView<const_heap_iterator> heap_view() const { return { heap_begin(), heap_end() }; }

    // ── RB find ───────────────────────────────────────────────────────────────
    // Returns rb_end() if not found.  Only finds the first match when
    // duplicates are present (they land in the right subtree).
    rb_iterator rb_find(const T& val) {
        std::size_t n = rb_root_;
        while (n != NULL_IDX) {
            if (rb_comp_(val, nodes_[n].value))      n = nodes_[n].rb_left;
            else if (rb_comp_(nodes_[n].value, val)) n = nodes_[n].rb_right;
            else return { this, n };
        }
        return rb_end();
    }
    const_rb_iterator rb_find(const T& val) const {
        std::size_t n = rb_root_;
        while (n != NULL_IDX) {
            if (rb_comp_(val, nodes_[n].value))      n = nodes_[n].rb_left;
            else if (rb_comp_(nodes_[n].value, val)) n = nodes_[n].rb_right;
            else return { this, n };
        }
        return rb_end();
    }

private:
    std::vector<Node<T>>  nodes_;
    std::vector<std::size_t> free_;

    std::size_t list_head_  = NULL_IDX;
    std::size_t list_tail_  = NULL_IDX;

    std::size_t rb_root_    = NULL_IDX;

    std::vector<std::size_t> heap_;

    std::size_t live_count_ = 0;

    RBComp   rb_comp_;
    HeapComp heap_comp_;

    // ── Node accessor helpers ─────────────────────────────────────────────────
    Node<T>&       nd(std::size_t i)       { return nodes_[i]; }
    const Node<T>& nd(std::size_t i) const { return nodes_[i]; }

    std::size_t nd_left (std::size_t n) const { return n == NULL_IDX ? NULL_IDX : nd(n).rb_left;  }
    std::size_t nd_right(std::size_t n) const { return n == NULL_IDX ? NULL_IDX : nd(n).rb_right; }

    Color rb_color_of(std::size_t n) const {
        return n == NULL_IDX ? Color::Black : nd(n).rb_color;
    }

    // ── Allocation ────────────────────────────────────────────────────────────
    std::size_t alloc_node(T&& val) {
        std::size_t idx;
        if (!free_.empty()) {
            idx = free_.back();
            free_.pop_back();
            nd(idx) = Node<T>{ std::move(val) };
            nd(idx).alive = true;
        } else {
            idx = nodes_.size();
            nodes_.push_back(Node<T>{ std::move(val) });
            nd(idx).alive = true;
        }
        return idx;
    }

    void reclaim_node(std::size_t idx) {
        nd(idx).alive = false;
        free_.push_back(idx);
    }

    // ── Doubly-linked list ────────────────────────────────────────────────────
    void list_push_front(std::size_t idx) {
        nd(idx).list_next = list_head_;
        nd(idx).list_prev = NULL_IDX;
        if (list_head_ != NULL_IDX) nd(list_head_).list_prev = idx;
        else                        list_tail_ = idx;
        list_head_ = idx;
    }

    void list_push_back(std::size_t idx) {
        nd(idx).list_prev = list_tail_;
        nd(idx).list_next = NULL_IDX;
        if (list_tail_ != NULL_IDX) nd(list_tail_).list_next = idx;
        else                        list_head_ = idx;
        list_tail_ = idx;
    }

    // Place the (unlinked) node idx immediately after the live node after_idx.
    void list_splice_after(std::size_t idx, std::size_t after_idx) {
        std::size_t next = nd(after_idx).list_next;
        nd(idx).list_prev       = after_idx;
        nd(idx).list_next       = next;
        nd(after_idx).list_next = idx;
        if (next != NULL_IDX) nd(next).list_prev = idx;
        else                  list_tail_ = idx;
    }

    void list_remove(std::size_t idx) {
        std::size_t prev = nd(idx).list_prev;
        std::size_t next = nd(idx).list_next;
        if (prev != NULL_IDX) nd(prev).list_next = next; else list_head_ = next;
        if (next != NULL_IDX) nd(next).list_prev = prev; else list_tail_ = prev;
    }

    // ── Red-black tree – rotations ────────────────────────────────────────────
    void rb_left_rotate(std::size_t x) {
        std::size_t y = nd(x).rb_right;
        nd(x).rb_right = nd(y).rb_left;
        if (nd(y).rb_left != NULL_IDX) nd(nd(y).rb_left).rb_parent = x;
        nd(y).rb_parent = nd(x).rb_parent;
        if      (nd(x).rb_parent == NULL_IDX)            rb_root_ = y;
        else if (x == nd(nd(x).rb_parent).rb_left)       nd(nd(x).rb_parent).rb_left  = y;
        else                                              nd(nd(x).rb_parent).rb_right = y;
        nd(y).rb_left    = x;
        nd(x).rb_parent  = y;
    }

    void rb_right_rotate(std::size_t x) {
        std::size_t y = nd(x).rb_left;
        nd(x).rb_left = nd(y).rb_right;
        if (nd(y).rb_right != NULL_IDX) nd(nd(y).rb_right).rb_parent = x;
        nd(y).rb_parent = nd(x).rb_parent;
        if      (nd(x).rb_parent == NULL_IDX)            rb_root_ = y;
        else if (x == nd(nd(x).rb_parent).rb_right)      nd(nd(x).rb_parent).rb_right = y;
        else                                              nd(nd(x).rb_parent).rb_left  = y;
        nd(y).rb_right   = x;
        nd(x).rb_parent  = y;
    }

    // ── Red-black tree – insertion ────────────────────────────────────────────
    void rb_insert(std::size_t z) {
        std::size_t y = NULL_IDX, x = rb_root_;
        while (x != NULL_IDX) {
            y = x;
            x = rb_comp_(nd(z).value, nd(x).value) ? nd(x).rb_left : nd(x).rb_right;
        }
        nd(z).rb_parent = y;
        nd(z).rb_left   = NULL_IDX;
        nd(z).rb_right  = NULL_IDX;
        nd(z).rb_color  = Color::Red;
        if      (y == NULL_IDX)                          rb_root_ = z;
        else if (rb_comp_(nd(z).value, nd(y).value))     nd(y).rb_left  = z;
        else                                             nd(y).rb_right = z;
        rb_insert_fixup(z);
    }

    void rb_insert_fixup(std::size_t z) {
        while (nd(z).rb_parent != NULL_IDX &&
               nd(nd(z).rb_parent).rb_color == Color::Red)
        {
            std::size_t p = nd(z).rb_parent;
            std::size_t g = nd(p).rb_parent;
            if (g == NULL_IDX) break; // parent is root (handled below)

            if (p == nd(g).rb_left) {
                std::size_t u = nd(g).rb_right; // uncle
                if (rb_color_of(u) == Color::Red) {           // Case 1
                    nd(p).rb_color = Color::Black;
                    nd(u).rb_color = Color::Black;
                    nd(g).rb_color = Color::Red;
                    z = g;
                } else {
                    if (z == nd(p).rb_right) {                // Case 2 → Case 3
                        z = p; rb_left_rotate(z);
                        p = nd(z).rb_parent; g = nd(p).rb_parent;
                    }
                    nd(p).rb_color = Color::Black;            // Case 3
                    nd(g).rb_color = Color::Red;
                    rb_right_rotate(g);
                }
            } else {
                std::size_t u = nd(g).rb_left;
                if (rb_color_of(u) == Color::Red) {
                    nd(p).rb_color = Color::Black;
                    nd(u).rb_color = Color::Black;
                    nd(g).rb_color = Color::Red;
                    z = g;
                } else {
                    if (z == nd(p).rb_left) {
                        z = p; rb_right_rotate(z);
                        p = nd(z).rb_parent; g = nd(p).rb_parent;
                    }
                    nd(p).rb_color = Color::Black;
                    nd(g).rb_color = Color::Red;
                    rb_left_rotate(g);
                }
            }
        }
        nd(rb_root_).rb_color = Color::Black;
    }

    // ── Red-black tree – deletion ─────────────────────────────────────────────
    // Replaces subtree rooted at u with subtree rooted at v.
    void rb_transplant(std::size_t u, std::size_t v) {
        if      (nd(u).rb_parent == NULL_IDX)               rb_root_ = v;
        else if (u == nd(nd(u).rb_parent).rb_left)           nd(nd(u).rb_parent).rb_left  = v;
        else                                                 nd(nd(u).rb_parent).rb_right = v;
        if (v != NULL_IDX) nd(v).rb_parent = nd(u).rb_parent;
    }

    std::size_t rb_leftmost(std::size_t n) const {
        while (n != NULL_IDX && nd(n).rb_left != NULL_IDX) n = nd(n).rb_left;
        return n;
    }
    std::size_t rb_rightmost(std::size_t n) const {
        while (n != NULL_IDX && nd(n).rb_right != NULL_IDX) n = nd(n).rb_right;
        return n;
    }

    void rb_erase(std::size_t z) {
        std::size_t y = z;
        Color       y_orig = nd(y).rb_color;
        std::size_t x, x_parent;
        bool        x_is_left;

        if (nd(z).rb_left == NULL_IDX) {
            x        = nd(z).rb_right;
            x_parent = nd(z).rb_parent;
            x_is_left = (x_parent != NULL_IDX && nd(x_parent).rb_left == z);
            rb_transplant(z, x);
        } else if (nd(z).rb_right == NULL_IDX) {
            x        = nd(z).rb_left;
            x_parent = nd(z).rb_parent;
            x_is_left = (x_parent != NULL_IDX && nd(x_parent).rb_left == z);
            rb_transplant(z, x);
        } else {
            y        = rb_leftmost(nd(z).rb_right); // in-order successor
            y_orig   = nd(y).rb_color;
            x        = nd(y).rb_right;

            if (nd(y).rb_parent == z) {
                x_parent  = y;
                x_is_left = false; // x is y's right child
            } else {
                x_parent  = nd(y).rb_parent;
                x_is_left = (nd(x_parent).rb_left == y); // before transplant
                rb_transplant(y, x);
                nd(y).rb_right              = nd(z).rb_right;
                nd(nd(y).rb_right).rb_parent = y;
            }
            rb_transplant(z, y);
            nd(y).rb_left               = nd(z).rb_left;
            nd(nd(y).rb_left).rb_parent  = y;
            nd(y).rb_color               = nd(z).rb_color;
        }

        if (y_orig == Color::Black)
            rb_erase_fixup(x, x_parent, x_is_left);
    }

    void rb_erase_fixup(std::size_t x, std::size_t x_parent, bool x_is_left) {
        while (x != rb_root_ && rb_color_of(x) == Color::Black) {
            if (x_parent == NULL_IDX) break;

            if (x_is_left) {
                std::size_t w = nd(x_parent).rb_right; // sibling
                if (rb_color_of(w) == Color::Red) {                          // Case 1
                    nd(w).rb_color        = Color::Black;
                    nd(x_parent).rb_color = Color::Red;
                    rb_left_rotate(x_parent);
                    w = nd(x_parent).rb_right;
                }
                if (rb_color_of(nd_left(w)) == Color::Black &&
                    rb_color_of(nd_right(w)) == Color::Black) {              // Case 2
                    if (w != NULL_IDX) nd(w).rb_color = Color::Red;
                    x          = x_parent;
                    x_parent   = nd(x).rb_parent;
                    x_is_left  = (x_parent != NULL_IDX && nd(x_parent).rb_left == x);
                } else {
                    if (rb_color_of(nd_right(w)) == Color::Black) {          // Case 3
                        if (w != NULL_IDX) {
                            if (nd(w).rb_left != NULL_IDX)
                                nd(nd(w).rb_left).rb_color = Color::Black;
                            nd(w).rb_color = Color::Red;
                            rb_right_rotate(w);
                            w = nd(x_parent).rb_right;
                        }
                    }
                    // Case 4
                    if (w != NULL_IDX) nd(w).rb_color = nd(x_parent).rb_color;
                    nd(x_parent).rb_color = Color::Black;
                    if (w != NULL_IDX && nd(w).rb_right != NULL_IDX)
                        nd(nd(w).rb_right).rb_color = Color::Black;
                    rb_left_rotate(x_parent);
                    x = rb_root_;
                }
            } else {
                std::size_t w = nd(x_parent).rb_left; // sibling
                if (rb_color_of(w) == Color::Red) {                          // Case 1 mirror
                    nd(w).rb_color        = Color::Black;
                    nd(x_parent).rb_color = Color::Red;
                    rb_right_rotate(x_parent);
                    w = nd(x_parent).rb_left;
                }
                if (rb_color_of(nd_right(w)) == Color::Black &&
                    rb_color_of(nd_left(w))  == Color::Black) {              // Case 2 mirror
                    if (w != NULL_IDX) nd(w).rb_color = Color::Red;
                    x          = x_parent;
                    x_parent   = nd(x).rb_parent;
                    x_is_left  = (x_parent != NULL_IDX && nd(x_parent).rb_left == x);
                } else {
                    if (rb_color_of(nd_left(w)) == Color::Black) {           // Case 3 mirror
                        if (w != NULL_IDX) {
                            if (nd(w).rb_right != NULL_IDX)
                                nd(nd(w).rb_right).rb_color = Color::Black;
                            nd(w).rb_color = Color::Red;
                            rb_left_rotate(w);
                            w = nd(x_parent).rb_left;
                        }
                    }
                    // Case 4 mirror
                    if (w != NULL_IDX) nd(w).rb_color = nd(x_parent).rb_color;
                    nd(x_parent).rb_color = Color::Black;
                    if (w != NULL_IDX && nd(w).rb_left != NULL_IDX)
                        nd(nd(w).rb_left).rb_color = Color::Black;
                    rb_right_rotate(x_parent);
                    x = rb_root_;
                }
            }
        }
        if (x != NULL_IDX) nd(x).rb_color = Color::Black;
    }

    // ── Heap operations ───────────────────────────────────────────────────────
    void heap_sift_up(std::size_t pos) {
        while (pos > 0) {
            std::size_t parent = (pos - 1) / 2;
            std::size_t ci = heap_[pos], pi = heap_[parent];
            if (heap_comp_(nd(ci).value, nd(pi).value)) {
                std::swap(heap_[pos], heap_[parent]);
                nd(ci).heap_pos = parent;
                nd(pi).heap_pos = pos;
                pos = parent;
            } else break;
        }
    }

    void heap_sift_down(std::size_t pos) {
        std::size_t n = heap_.size();
        while (true) {
            std::size_t best  = pos;
            std::size_t left  = 2 * pos + 1;
            std::size_t right = 2 * pos + 2;
            if (left  < n && heap_comp_(nd(heap_[left]).value,  nd(heap_[best]).value)) best = left;
            if (right < n && heap_comp_(nd(heap_[right]).value, nd(heap_[best]).value)) best = right;
            if (best == pos) break;
            std::swap(heap_[pos], heap_[best]);
            nd(heap_[pos]).heap_pos  = pos;
            nd(heap_[best]).heap_pos = best;
            pos = best;
        }
    }

    void heap_push(std::size_t idx) {
        std::size_t pos = heap_.size();
        heap_.push_back(idx);
        nd(idx).heap_pos = pos;
        heap_sift_up(pos);
    }

    void heap_remove(std::size_t idx) {
        std::size_t pos  = nd(idx).heap_pos;
        std::size_t last = heap_.size() - 1;
        nd(idx).heap_pos = NULL_IDX;

        if (pos == last) { heap_.pop_back(); return; }

        std::size_t last_idx = heap_[last];
        heap_[pos]            = last_idx;
        nd(last_idx).heap_pos = pos;
        heap_.pop_back();

        // Both directions may be needed after an arbitrary removal
        heap_sift_up(pos);
        heap_sift_down(nd(last_idx).heap_pos);
    }

    // ── Combined insert helpers ───────────────────────────────────────────────
    std::size_t insert_impl(T&& val) {
        std::size_t idx = alloc_node(std::move(val));
        list_push_back(idx);
        rb_insert(idx);
        heap_push(idx);
        ++live_count_;
        return idx;
    }

    std::size_t insert_front_impl(T&& val) {
        std::size_t idx = alloc_node(std::move(val));
        list_push_front(idx);
        rb_insert(idx);
        heap_push(idx);
        ++live_count_;
        return idx;
    }

    std::size_t insert_after_impl(T&& val, std::size_t after_idx) {
        assert(after_idx < nodes_.size() && nodes_[after_idx].alive);
        // Capture the successor index before alloc_node can reallocate nodes_.
        // (The index itself is stable; we just want to read it before any
        //  potential move of the underlying storage.)
        std::size_t idx = alloc_node(std::move(val));
        list_splice_after(idx, after_idx);
        rb_insert(idx);
        heap_push(idx);
        ++live_count_;
        return idx;
    }

    std::size_t insert_before_impl(T&& val, std::size_t before_idx) {
        assert(before_idx < nodes_.size() && nodes_[before_idx].alive);
        // Read the predecessor index before alloc_node can reallocate.
        std::size_t prev = nd(before_idx).list_prev;
        std::size_t idx  = alloc_node(std::move(val));
        if (prev == NULL_IDX) list_push_front(idx);
        else                  list_splice_after(idx, prev);
        rb_insert(idx);
        heap_push(idx);
        ++live_count_;
        return idx;
    }
};

// ── erase (out-of-line so the fixup helpers are all visible) ─────────────────
template<typename T, typename RC, typename HC>
void DLRBHeapVector<T, RC, HC>::erase(std::size_t idx) {
    assert(idx < nodes_.size() && nodes_[idx].alive && "erase: invalid or dead index");
    list_remove(idx);
    rb_erase(idx);
    heap_remove(idx);
    reclaim_node(idx);
    --live_count_;
}

} // namespace dlrb
