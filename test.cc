#include "include/json.hpp"
#include <cassert>
#include <cstdint>

MemoryAllocator &getAllocator() {
    // use frg::eternal to prevent a call to __cxa_atexit().
    // this is necessary because __cxa_atexit() call this function.
    static frg::eternal<MemoryAllocator> singleton{};
    return singleton.get();
}

void *MemoryAllocator::allocate(size_t size) { return malloc(size); }
void MemoryAllocator::free(void *ptr) {}
void MemoryAllocator::deallocate(void *ptr, size_t size) {}
void *MemoryAllocator::reallocate(void *ptr, size_t size) {
    return realloc(ptr, size);
}
namespace test {
void numbers() {
    auto parser =
        json::JsonParser("{\"main\": [69, -420, 69.420]}", getAllocator());

    auto root = parser.parse();
    auto &main = root["main"].get<json::Array>();

    assert(main[0].get<int64_t>() == 69);
    assert(main[1].get<int64_t>() == -420);
    assert(main[2].get<double>() == 69.420);
}
} // namespace test

int main() {
    // auto parser = json::JsonParser<MemoryAllocator>("{\"hello\":
    // \"world\"}"); auto value = parser.parse_value(); auto &root =
    // value.get<json::JsonObject<MemoryAllocator>>();

    // auto x = root.get("hello")->get<frg::string<MemoryAllocator>>();
    // std::cout << x.begin() << std::endl;

    //   assert(root.empty());
    // auto parser = json::JsonParser<MemoryAllocator>("{\"hello\":69}");
    // auto value = parser.parse_value();
    // auto &root = value.get<json::JsonObject<MemoryAllocator>>();

    // auto x = root.get("hello")->get<size_t>();
    // std::cout << x << std::endl;
    auto parser =
        json::JsonParser("{\"hello\":[69, false, true, 420]}", getAllocator());

    auto root = parser.parse();

    auto &x = root["hello"].get<json::Array>();

    std::cout << "size: " << x.size() << std::endl;
    std::cout << "first member: " << x[0].get<int64_t>() << std::endl;
    std::cout << "second member: " << x[1].get<bool>() << std::endl;
    std::cout << "third member: " << x[2].get<bool>() << std::endl;
    std::cout << "fourth member: " << x[3].get<int64_t>() << std::endl;

    test::numbers();

    return 0;
}
