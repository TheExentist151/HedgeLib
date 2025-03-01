#ifndef HL_BLOB_H_INCLUDED
#define HL_BLOB_H_INCLUDED
#include "hl_text.h"

namespace hl
{
class blob
{
    friend struct in_blob_size_setter;

protected:
    /** @brief Pointer to the data this blob contains. */
    std::unique_ptr<u8[]> m_data;
    /** @brief Size of the data this blob contains, in bytes. */
    std::size_t m_size;

    inline blob() noexcept :
        m_size(0) {}

public:
    template<typename T = void>
    inline const T* data() const noexcept
    {
        return reinterpret_cast<const T*>(m_data.get());
    }

    template<typename T = void>
    inline T* data() noexcept
    {
        return reinterpret_cast<T*>(m_data.get());
    }

    inline std::size_t size() const noexcept
    {
        return m_size;
    }

    inline operator const void*() const noexcept
    {
        return m_data.get();
    }

    inline operator void*() noexcept
    {
        return m_data.get();
    }

    HL_API blob& operator=(const blob& other);

    HL_API blob& operator=(blob&& other) noexcept;

    HL_API blob(std::size_t size, const void* initialData = nullptr);

    HL_API blob(const nchar* filePath);

    inline blob(const nstring& filePath) :
        blob(filePath.c_str()) {}

    HL_API blob(const blob& other);

    HL_API blob(blob&& other) noexcept;
};

struct nullable_blob : public blob
{
    inline bool empty() const noexcept
    {
        return (m_size == 0U);
    }

    inline explicit operator bool() const noexcept
    {
        return (m_size != 0U);
    }

    nullable_blob() noexcept = default;

    nullable_blob(std::size_t size, const void* initialData = nullptr) :
        blob(size, initialData) {}

    nullable_blob(const nchar* filePath) :
        blob(filePath) {}

    inline nullable_blob(const nstring& filePath) :
        nullable_blob(filePath.c_str()) {}

};
} // hl
#endif
