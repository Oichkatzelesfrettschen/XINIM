extern "C" void* memcpy(void* destination, const void* source, unsigned long count) {
    auto* dst = static_cast<unsigned char*>(destination);
    const auto* src = static_cast<const unsigned char*>(source);
    for (unsigned long index = 0; index < count; ++index) {
        dst[index] = src[index];
    }
    return destination;
}

extern "C" int memcmp(const void* lhs, const void* rhs, unsigned long count) {
    const auto* left = static_cast<const unsigned char*>(lhs);
    const auto* right = static_cast<const unsigned char*>(rhs);
    for (unsigned long index = 0; index < count; ++index) {
        if (left[index] != right[index]) {
            return static_cast<int>(left[index]) - static_cast<int>(right[index]);
        }
    }
    return 0;
}
