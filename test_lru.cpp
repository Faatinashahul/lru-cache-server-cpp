#include <iostream>
#include <cassert>
#include "lru_cache.h"

int main() {
    LRUCache cache(2); // capacity 2

    std::string val;

    cache.put("a", "1");
    cache.put("b", "2");

    assert(cache.get("a", val) && val == "1"); // "a" now most recent
    cache.put("c", "3"); // capacity exceeded -> evicts "b" (least recently used)

    assert(cache.get("b", val) == false); // b should be evicted
    assert(cache.get("a", val) && val == "1");
    assert(cache.get("c", val) && val == "3");

    cache.put("a", "100"); // update existing key
    assert(cache.get("a", val) && val == "100");

    assert(cache.size() == 2);

    std::cout << "All LRU cache tests passed.\n";
    return 0;
}
