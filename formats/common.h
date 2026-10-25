#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

inline std::uint16_t read16(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8));
}

inline std::uint32_t read32(const std::uint8_t* data)
{
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

inline std::int16_t readI16(const std::uint8_t* data)
{
    const std::uint16_t bits = read16(data);
    std::int16_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::int32_t readI32(const std::uint8_t* data)
{
    const std::uint32_t bits = read32(data);
    std::int32_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline float floatFromBits(std::uint32_t bits)
{
    float value;
    static_assert(sizeof(value) == sizeof(bits), "32-bit float required");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline float readFloat(const std::uint8_t* data)
{
    return floatFromBits(read32(data));
}

inline void write16(std::uint8_t* data, std::uint16_t value)
{
    data[0] = static_cast<std::uint8_t>(value);
    data[1] = static_cast<std::uint8_t>(value >> 8);
}

inline void write32(std::uint8_t* data, std::uint32_t value)
{
    data[0] = static_cast<std::uint8_t>(value);
    data[1] = static_cast<std::uint8_t>(value >> 8);
    data[2] = static_cast<std::uint8_t>(value >> 16);
    data[3] = static_cast<std::uint8_t>(value >> 24);
}

inline void write16(std::vector<std::uint8_t>& data, std::uint16_t value)
{
    data.push_back(static_cast<std::uint8_t>(value));
    data.push_back(static_cast<std::uint8_t>(value >> 8));
}

inline void write32(std::vector<std::uint8_t>& data, std::uint32_t value)
{
    data.push_back(static_cast<std::uint8_t>(value));
    data.push_back(static_cast<std::uint8_t>(value >> 8));
    data.push_back(static_cast<std::uint8_t>(value >> 16));
    data.push_back(static_cast<std::uint8_t>(value >> 24));
}
