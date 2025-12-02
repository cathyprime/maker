#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef MAKER_BUFFER_DEFAULT
#define MAKER_BUFFER_DEFAULT 8 * 1024
#endif

namespace maker
{
    namespace temp
    {
        size_t strlen(const char *str);
        char *strcpy(char *dest, const char *str);
        char *strdup(const char *str);
        int strcmp(const char *lhs, const char *rhs);
        int strncmp(const char *lhs, const char *rhs, size_t n);
    }

    struct Command
    {
        char **items = nullptr;
        const size_t length = 0;
        const size_t capacity = 0;

        void push(char*);
        void resize();
    };

    struct Temp_Buffer
    {
        void save();
        void load();
        void *alloc(size_t sz);
        void *alloc_count(size_t n, size_t sz);

        constexpr static size_t start_size = MAKER_BUFFER_DEFAULT;
        static thread_local char buffer[start_size];
        const size_t idx = 0;
        const size_t save_point = 0;
    } extern thread_local tmp_buffer;

    struct String_View
    {
        const char *data = nullptr;
        size_t len = 0;

        String_View() = default;
        String_View(const char *cstr);
        bool operator==(const String_View &other) const;
        bool operator!=(const String_View &other) const;

        String_View(const String_View &rhs) = default;
        String_View(String_View&&) = delete;

        char *cstr() const;
    };
} // maker

#ifdef MAKER_IMPLEMENTATION

thread_local char maker::Temp_Buffer::buffer[];
thread_local maker::Temp_Buffer maker::tmp_buffer;

constexpr size_t item_size = sizeof(char*);

template<typename T>
constexpr inline T &mut(const T& t)
{
    return const_cast<T&>(t);
}

template<typename T>
constexpr inline void swap(T &val1, T &val2)
{
    val1 ^= val2;
    val2 ^= val1;
    val1 ^= val2;
}

void maker::Temp_Buffer::save()
{
    const_cast<size_t&>(save_point) = idx;
}

void maker::Temp_Buffer::load()
{
    swap(mut(idx), mut(save_point));
}

void *maker::Temp_Buffer::alloc(size_t sz)
{
    size_t start_pos = idx;

    if (idx + sz > start_size)
        return nullptr;

    mut(idx) += sz;
    return &buffer[start_pos];
}

void *maker::Temp_Buffer::alloc_count(size_t n, size_t sz)
{
    size_t start_pos = idx;

    if (idx + (n * sz) > start_size)
        return nullptr;

    mut(idx) += n * sz;
    return &buffer[start_pos];
}

void maker::Command::resize()
{
    if (capacity == 0)
    {
        const_cast<size_t&>(capacity) = 4;
        items = (char**) calloc(4, item_size);
        return;
    }

    const_cast<size_t&>(capacity) *= 2;
    items = (char**) reallocarray(items, capacity, item_size);
}

void maker::Command::push(char *cmd)
{
    if (length >= capacity || capacity == 0) resize();
    items[const_cast<size_t&>(length)++] = cmd;
}

size_t maker::temp::strlen(const char *str)
{
    if (!str) return 0;
    const char *seeker = str;
    while(*seeker++);
    return static_cast<size_t>(seeker - str) - 1;
}

char *maker::temp::strcpy(char *dest, const char *str)
{
    if (!str)  return nullptr;
    if (!dest) return nullptr;
    char *d = dest;
    do
    {
        *dest++ = *str++;
    }
    while (*str);
    return d;
}

char *maker::temp::strdup(const char *str)
{
    char *new_str = (char*) maker::tmp_buffer.alloc_count(strlen(str) + 1, sizeof(*str));
    return strcpy(new_str, str);
}

int maker::temp::strcmp(const char *left, const char *right)
{
    while(*left && (*left == *right))
    {
        left++;
        right++;
    }
    return *(unsigned char*)left - *(unsigned char*)right;
}

int maker::temp::strncmp(const char *left, const char *right, size_t n)
{
    if (n == 0) return 0;

    while (n-- > 0 && *left && (*left == *right))
    {
        if (n == 0)
            return 0;
        left++;
        right++;
    }

    return *(const unsigned char*)left - *(const unsigned char*)right;
}

maker::String_View::String_View(const char *cstr)
    : data(cstr)
    , len(temp::strlen(cstr))
{}

char *maker::String_View::cstr() const
{
    if (!data) return nullptr;
    char *ret = static_cast<char *>(maker::tmp_buffer.alloc(len + 1));
    for (size_t idx = 0; idx < len; ++idx)
        ret[idx] = data[idx];
    ret[len + 1] = '\0';
    return ret;
}

bool maker::String_View::operator==(const String_View &other) const
{
    if (len != other.len) return false;
    if (data == other.data) return true;
    return temp::strncmp(data, other.data, len) == 0;
}

bool maker::String_View::operator!=(const String_View &other) const
{
    return !(*this == other);
}

#endif
