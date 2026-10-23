#pragma once

#include <cerrno>

#include <cstdint>

#include <cstdlib>

#include <fstream>

#include <limits>

#include <stdexcept>

#include <string>

#include <vector>

namespace tool

{

using Bytes = std::vector<std::uint8_t>;

inline Bytes readFile(const std::string& path)

{

    std::ifstream input(path, std::ios::binary | std::ios::ate);

    if (!input)

        throw std::runtime_error(path + ": could not open input");

    const std::streamoff end = input.tellg();

    if (end < 0)

        throw std::runtime_error(path + ": could not determine input size");

    if (static_cast<std::uintmax_t>(end) > std::numeric_limits<std::size_t>::max())

        throw std::runtime_error(path + ": input is too large for this process");

    Bytes data(static_cast<std::size_t>(end));

    input.seekg(0, std::ios::beg);

    if (!data.empty())

        input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

    if (!input)

        throw std::runtime_error(path + ": could not read complete input");

    return data;

}

inline bool spanWithin(std::uint64_t offset, std::uint64_t size, std::uint64_t total)

{

    return offset <= total && size <= total - offset;

}

inline std::uint16_t readLe16(const Bytes& data, std::uint64_t offset, const char* field)

{

    if (!spanWithin(offset, 2, data.size()))

        throw std::runtime_error(std::string("truncated ") + field);

    return static_cast<std::uint16_t>(data[static_cast<std::size_t>(offset)]) |

           static_cast<std::uint16_t>(data[static_cast<std::size_t>(offset + 1)] << 8);

}

inline std::int16_t readLeS16(const Bytes& data, std::uint64_t offset, const char* field)

{

    return static_cast<std::int16_t>(readLe16(data, offset, field));

}

inline std::uint32_t readLe32(const Bytes& data, std::uint64_t offset, const char* field)

{

    if (!spanWithin(offset, 4, data.size()))

        throw std::runtime_error(std::string("truncated ") + field);

    const std::size_t i = static_cast<std::size_t>(offset);

    return static_cast<std::uint32_t>(data[i]) |

           (static_cast<std::uint32_t>(data[i + 1]) << 8) |

           (static_cast<std::uint32_t>(data[i + 2]) << 16) |

           (static_cast<std::uint32_t>(data[i + 3]) << 24);

}

inline std::int32_t readLeS32(const Bytes& data, std::uint64_t offset, const char* field)

{

    return static_cast<std::int32_t>(readLe32(data, offset, field));

}

inline std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b, const char* field)

{

    if (b > std::numeric_limits<std::uint64_t>::max() - a)

        throw std::runtime_error(std::string(field) + " addition overflow");

    return a + b;

}

inline std::uint64_t checkedMultiply(std::uint64_t a, std::uint64_t b, const char* field)

{

    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a)

        throw std::runtime_error(std::string(field) + " multiplication overflow");

    return a * b;

}

inline std::uint64_t parseUnsigned(const std::string& text, const char* field)

{

    if (text.empty() || text[0] == '-')

        throw std::runtime_error(std::string("invalid ") + field + ": " + text);

    errno = 0;

    char* end = nullptr;

    const unsigned long long value = std::strtoull(text.c_str(), &end, 0);

    if (errno == ERANGE || end == text.c_str() || *end != '\0')

        throw std::runtime_error(std::string("invalid ") + field + ": " + text);

    return static_cast<std::uint64_t>(value);

}

}
