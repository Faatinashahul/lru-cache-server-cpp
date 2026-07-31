#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <unordered_map>
#include <string>
#include <stdexcept>

// A doubly linked list node holding a key-value pair.
// We store the key inside the node (not just the value) because when we
// evict the least-recently-used node from the tail, we need its key to
// also erase the corresponding entry from the hashmap. Without the key
// here, eviction would be O(n) instead of O(1).
struct Node {
    std::string key;
    std::string value;
    Node* prev;
    Node* next;
    Node(const std::string& k, const std::string& v)
        : key(k), value(v), prev(nullptr), next(nullptr) {}
};

// LRUCache: fixed-capacity cache with O(1) get and put.
//
// Design:
// - unordered_map<key, Node*> gives O(1) lookup of a node by key.
// - A doubly linked list keeps nodes ordered by recency: head = most
//   recently used, tail = least recently used.
// - On every get()/put(), the accessed node is moved to the head.
// - On put() when at capacity, the tail node is evicted.
//
// Why a doubly linked list and not std::list?
// std::list would work too, but implementing it manually here is
// intentional -- it's the part of this project that demonstrates real
// DSA understanding rather than relying on a library abstraction.
class LRUCache {
public:
    explicit LRUCache(size_t capacity)
        : capacity_(capacity), head_(nullptr), tail_(nullptr) {
        if (capacity_ == 0) {
            throw std::invalid_argument("Cache capacity must be > 0");
        }
    }

    ~LRUCache() {
        Node* curr = head_;
        while (curr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
    }

    // Returns true and sets `value` if key exists; false otherwise.
    // On a hit, moves the node to the front (most recently used).
    bool get(const std::string& key, std::string& value) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }
        Node* node = it->second;
        moveToFront(node);
        value = node->value;
        return true;
    }

    // Inserts or updates a key. If the key exists, updates its value and
    // moves it to the front. If inserting a new key at capacity, evicts
    // the least-recently-used node (the tail) first.
    void put(const std::string& key, const std::string& value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->value = value;
            moveToFront(it->second);
            return;
        }

        if (map_.size() >= capacity_) {
            evictLRU();
        }

        Node* node = new Node(key, value);
        map_[key] = node;
        pushFront(node);
    }

    bool remove(const std::string& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        unlink(it->second);
        delete it->second;
        map_.erase(it);
        return true;
    }

    size_t size() const { return map_.size(); }
    size_t capacity() const { return capacity_; }

private:
    size_t capacity_;
    std::unordered_map<std::string, Node*> map_;
    Node* head_; // most recently used
    Node* tail_; // least recently used

    void unlink(Node* node) {
        if (node->prev) node->prev->next = node->next;
        if (node->next) node->next->prev = node->prev;
        if (node == head_) head_ = node->next;
        if (node == tail_) tail_ = node->prev;
        node->prev = node->next = nullptr;
    }

    void pushFront(Node* node) {
        node->next = head_;
        node->prev = nullptr;
        if (head_) head_->prev = node;
        head_ = node;
        if (!tail_) tail_ = node;
    }

    void moveToFront(Node* node) {
        if (node == head_) return;
        unlink(node);
        pushFront(node);
    }

    void evictLRU() {
        if (!tail_) return;
        Node* victim = tail_;
        unlink(victim);
        map_.erase(victim->key);
        delete victim;
    }
};

#endif // LRU_CACHE_H
