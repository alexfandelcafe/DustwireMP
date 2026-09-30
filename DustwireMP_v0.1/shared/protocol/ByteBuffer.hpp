#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace dustwire::protocol {

class Writer {
public:
    void U8(std::uint8_t v) { data_.push_back(v); }
    void U16(std::uint16_t v) { putLE(v); }
    void U32(std::uint32_t v) { putLE(v); }
    void String(const std::string& s) {
        if (s.size() > 65535) throw std::runtime_error("string too long");
        U16(static_cast<std::uint16_t>(s.size()));
        data_.insert(data_.end(), s.begin(), s.end());
    }
    const std::vector<std::uint8_t>& Data() const { return data_; }
private:
    template <typename T> void putLE(T v) {
        for (std::size_t i = 0; i < sizeof(T); ++i)
            data_.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
    }
    std::vector<std::uint8_t> data_;
};

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    std::uint8_t U8() { ensure(1); return data_[pos_++]; }
    std::uint16_t U16() { return getLE<std::uint16_t>(); }
    std::uint32_t U32() { return getLE<std::uint32_t>(); }
    std::string String() {
        const auto len = U16();
        ensure(len);
        std::string out(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return out;
    }
    bool Empty() const { return pos_ >= size_; }
private:
    void ensure(std::size_t n) const {
        if (pos_ + n > size_) throw std::runtime_error("packet underflow");
    }
    template <typename T> T getLE() {
        ensure(sizeof(T));
        T out{};
        for (std::size_t i = 0; i < sizeof(T); ++i)
            out |= static_cast<T>(data_[pos_++]) << (i * 8);
        return out;
    }
    const std::uint8_t* data_{};
    std::size_t size_{};
    std::size_t pos_{};
};

}
