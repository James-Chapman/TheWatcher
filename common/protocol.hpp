#pragma once

#include <cbor.h>

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace thewatcher::proto
{

enum class FrameType : uint8_t
{
    HEARTBEAT = 0x01,
    METRICS = 0x02,
    COMMAND = 0x03,
    COMMAND_ACK = 0x04,
    CONFIG_UPDATE = 0x05,
    ENROLL_REQUEST = 0x06,
    ENROLL_RESPONSE = 0x07,
    CONFIG_REQUEST = 0x08,
};

// Wire frame sent over every ZMQ message (CBOR-encoded).
struct Frame
{
    uint8_t type; // FrameType cast to uint8_t
    std::string agent_id;
    int64_t timestamp_ms = 0;
    std::vector<uint8_t> payload; // type-specific CBOR payload
};

// Enrollment sub-types (carried in Frame::payload).
struct EnrollRequest
{
    std::string agent_id;
    std::string hostname;
    std::string platform;
    std::string curve_public_key_z85; // 40-char z85
};

struct EnrollResponse
{
    bool approved = false;
    std::string message;
    std::string server_public_key_z85;
    std::string server_public_key_fingerprint;
};

// ── libcbor helpers ───────────────────────────────────────────────────────────

namespace detail
{

struct CborDeleter
{
    void operator()(cbor_item_t* item) const noexcept
    {
        if (item != nullptr)
        {
            cbor_decref(&item);
        }
    }
};

using CborPtr = std::unique_ptr<cbor_item_t, CborDeleter>;

inline CborPtr adopt(cbor_item_t* item)
{
    if (item == nullptr)
    {
        throw std::runtime_error("libcbor allocation failed");
    }
    return CborPtr(item);
}

inline void push(cbor_item_t* array, cbor_item_t* value)
{
    if (array == nullptr || value == nullptr)
    {
        throw std::runtime_error("Invalid CBOR array/value");
    }

    if (!cbor_array_push(array, value))
    {
        cbor_decref(&value);
        throw std::runtime_error("Failed to append CBOR array item");
    }

    // cbor_array_push increments the child refcount; release our local ref.
    cbor_decref(&value);
}

inline cbor_item_t* array_get(cbor_item_t* array, size_t index)
{
    if (!cbor_isa_array(array) || cbor_array_size(array) <= index)
    {
        throw std::runtime_error("Invalid CBOR array shape");
    }

    cbor_item_t** items = cbor_array_handle(array);
    if (items == nullptr || items[index] == nullptr)
    {
        throw std::runtime_error("Invalid CBOR array item");
    }

    return items[index];
}

inline cbor_item_t* make_uint8(uint8_t value)
{
    return cbor_build_uint8(value);
}

inline cbor_item_t* make_int64(int64_t value)
{
    if (value >= 0)
    {
        return cbor_build_uint64(static_cast<uint64_t>(value));
    }

    // CBOR negative integer n is encoded as -1 - n.
    const uint64_t encoded = static_cast<uint64_t>(-(value + 1));
    return cbor_build_negint64(encoded);
}

inline cbor_item_t* make_string(const std::string& value)
{
    return cbor_build_stringn(value.data(), value.size());
}

inline cbor_item_t* make_bytes(const std::vector<uint8_t>& value)
{
    return cbor_build_bytestring(
        reinterpret_cast<cbor_data>(const_cast<uint8_t*>(value.data())),
        value.size()
    );
}

inline cbor_item_t* make_bool(bool value)
{
    return cbor_build_bool(value);
}

// libcbor's cbor_get_uint64 reads 8 raw bytes from item->data, which is only
// valid for CBOR_INT_64 width items. For narrower widths it returns garbage
// from adjacent memory. Always dispatch through this helper.
inline uint64_t cbor_uint_value(const cbor_item_t* item)
{
    switch (cbor_int_get_width(item))
    {
    case CBOR_INT_8:
        return cbor_get_uint8(const_cast<cbor_item_t*>(item));
    case CBOR_INT_16:
        return cbor_get_uint16(const_cast<cbor_item_t*>(item));
    case CBOR_INT_32:
        return cbor_get_uint32(const_cast<cbor_item_t*>(item));
    case CBOR_INT_64:
        return cbor_get_uint64(const_cast<cbor_item_t*>(item));
    }
    throw std::runtime_error("Unknown CBOR integer width");
}

inline uint8_t read_uint8(cbor_item_t* item)
{
    if (!cbor_isa_uint(item))
    {
        throw std::runtime_error("Expected CBOR unsigned integer");
    }

    const uint64_t value = cbor_uint_value(item);
    if (value > std::numeric_limits<uint8_t>::max())
    {
        throw std::runtime_error("CBOR integer does not fit uint8_t");
    }

    return static_cast<uint8_t>(value);
}

inline int64_t read_int64(cbor_item_t* item)
{
    if (cbor_isa_uint(item))
    {
        const uint64_t value = cbor_uint_value(item);
        if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        {
            throw std::runtime_error("CBOR integer does not fit int64_t");
        }
        return static_cast<int64_t>(value);
    }

    if (cbor_isa_negint(item))
    {
        const uint64_t encoded = cbor_uint_value(item);
        if (encoded > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        {
            throw std::runtime_error("CBOR negative integer does not fit int64_t");
        }
        return -1 - static_cast<int64_t>(encoded);
    }

    throw std::runtime_error("Expected CBOR integer");
}

inline std::string read_string(cbor_item_t* item)
{
    if (!cbor_isa_string(item) || !cbor_string_is_definite(item))
    {
        throw std::runtime_error("Expected definite CBOR string");
    }

    const auto* data = cbor_string_handle(item);
    const size_t size = cbor_string_length(item);

    return std::string(reinterpret_cast<const char*>(data), size);
}

inline std::vector<uint8_t> read_bytes(cbor_item_t* item)
{
    if (!cbor_isa_bytestring(item) || !cbor_bytestring_is_definite(item))
    {
        throw std::runtime_error("Expected definite CBOR byte string");
    }

    const auto* data = cbor_bytestring_handle(item);
    const size_t size = cbor_bytestring_length(item);

    return std::vector<uint8_t>(data, data + size);
}

inline bool read_bool(cbor_item_t* item)
{
    if (!cbor_is_bool(item))
    {
        throw std::runtime_error("Expected CBOR bool");
    }

    return cbor_get_bool(item);
}

// ── primary templates for to_cbor / from_cbor ────────────────────────────────
// to_cbor and from_cbor are templates so that explicit specialisations defined
// in any header are resolved via the primary at instantiation time. This
// sidesteps the qualified-name two-phase-lookup gotcha that plain overloads
// would hit when called via detail::to_cbor(obj) inside pack<T>.

template <typename T>
CborPtr to_cbor(const T&);

template <typename T>
T from_cbor(cbor_item_t*);

template <>
inline CborPtr to_cbor(const Frame& f)
{
    auto root = adopt(cbor_new_definite_array(4));

    push(root.get(), make_uint8(f.type));
    push(root.get(), make_string(f.agent_id));
    push(root.get(), make_int64(f.timestamp_ms));
    push(root.get(), make_bytes(f.payload));

    return root;
}

template <>
inline CborPtr to_cbor(const EnrollRequest& r)
{
    auto root = adopt(cbor_new_definite_array(4));

    push(root.get(), make_string(r.agent_id));
    push(root.get(), make_string(r.hostname));
    push(root.get(), make_string(r.platform));
    push(root.get(), make_string(r.curve_public_key_z85));

    return root;
}

template <>
inline CborPtr to_cbor(const EnrollResponse& r)
{
    auto root = adopt(cbor_new_definite_array(4));

    push(root.get(), make_bool(r.approved));
    push(root.get(), make_string(r.message));
    push(root.get(), make_string(r.server_public_key_z85));
    push(root.get(), make_string(r.server_public_key_fingerprint));

    return root;
}

template <>
inline Frame from_cbor<Frame>(cbor_item_t* item)
{
    if (!cbor_isa_array(item) || cbor_array_size(item) != 4)
    {
        throw std::runtime_error("Invalid Frame CBOR payload");
    }

    Frame f;
    f.type = read_uint8(array_get(item, 0));
    f.agent_id = read_string(array_get(item, 1));
    f.timestamp_ms = read_int64(array_get(item, 2));
    f.payload = read_bytes(array_get(item, 3));

    return f;
}

template <>
inline EnrollRequest from_cbor<EnrollRequest>(cbor_item_t* item)
{
    if (!cbor_isa_array(item) || cbor_array_size(item) != 4)
    {
        throw std::runtime_error("Invalid EnrollRequest CBOR payload");
    }

    EnrollRequest r;
    r.agent_id = read_string(array_get(item, 0));
    r.hostname = read_string(array_get(item, 1));
    r.platform = read_string(array_get(item, 2));
    r.curve_public_key_z85 = read_string(array_get(item, 3));

    return r;
}

template <>
inline EnrollResponse from_cbor<EnrollResponse>(cbor_item_t* item)
{
    if (!cbor_isa_array(item) || cbor_array_size(item) != 4)
    {
        throw std::runtime_error("Invalid EnrollResponse CBOR payload");
    }

    EnrollResponse r;
    r.approved = read_bool(array_get(item, 0));
    r.message = read_string(array_get(item, 1));
    r.server_public_key_z85 = read_string(array_get(item, 2));
    r.server_public_key_fingerprint = read_string(array_get(item, 3));

    return r;
}

} // namespace detail

// ── public helpers ────────────────────────────────────────────────────────────

template <typename T>
std::vector<uint8_t> pack(const T& obj)
{
    auto root = detail::to_cbor(obj);

    unsigned char* buffer = nullptr;
    size_t buffer_size = 0;

    cbor_serialize_alloc(root.get(), &buffer, &buffer_size);
    if (buffer == nullptr || buffer_size == 0)
    {
        throw std::runtime_error("Failed to serialize CBOR");
    }

    std::vector<uint8_t> out(buffer, buffer + buffer_size);
    std::free(buffer);

    return out;
}

template <typename T>
T unpack(const void* data, std::size_t size);

template <typename T>
T unpack(const std::vector<uint8_t>& data)
{
    return unpack<T>(data.data(), data.size());
}

template <typename T>
T unpack(const void* data, std::size_t size)
{
    if (data == nullptr || size == 0)
    {
        throw std::runtime_error("Cannot decode empty CBOR buffer");
    }

    cbor_load_result result {};
    auto root = detail::adopt(
    cbor_load(
                static_cast<cbor_data>(const_cast<std::uint8_t*>(
                static_cast<const std::uint8_t*>(data)
            )),
            size,
            &result
        )
    );

    if (result.error.code != CBOR_ERR_NONE)
    {
        throw std::runtime_error("Failed to parse CBOR payload");
    }

    if (result.read != size)
    {
        throw std::runtime_error("Trailing bytes after CBOR payload");
    }

    return detail::from_cbor<T>(root.get());
}

inline std::vector<uint8_t> encode_frame(const Frame& f)
{
    return pack(f);
}

inline Frame decode_frame(const void* data, std::size_t size)
{
    return unpack<Frame>(data, size);
}

} // namespace thewatcher::proto