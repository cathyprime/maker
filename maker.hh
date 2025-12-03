#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <spawn.h>
#include <ctype.h>
#include <sys/wait.h>

extern "C" char **environ;

#ifndef MAKER_BUFFER_DEFAULT
#define MAKER_BUFFER_DEFAULT 8 * 1024
#endif

#ifdef MAKER_TEST
#define ASSERT(cond, msg) if (!(cond)) throw msg
#else
#define ASSERT(cond, msg) if (!(cond)) assert(cond && msg)
#endif

namespace maker
{
    namespace temp
    {
        constexpr size_t strlen(const char *str);
        char *strcpy(char *dest, const char *str);
        char *strdup(const char *str);
        int strcmp(const char *lhs, const char *rhs);
        int strncmp(const char *lhs, const char *rhs, size_t n);
        int memcmp(const void *left, const void *right, size_t count);
    }

    struct Proc
    {
        pid_t pid = 0;
        void wait() const;
    };

    struct Command
    {
        char **items = nullptr;
        size_t length = 0;
        size_t capacity = 0;

        Command &push(char*);
        Command &push_null();
        void resize();
        void reset();
    };

    struct Temp_Buffer
    {
        void save();
        void load();
        char *alloc(size_t sz);
        void *resize_buffer(void *ptr, size_t old_n, size_t new_n);

        constexpr static size_t start_size = MAKER_BUFFER_DEFAULT;
        static thread_local char buffer[start_size];
        size_t idx = 0;
        size_t save_point = 0;
    } extern thread_local tmp_buffer;

    struct String_View
    {
        const char *data = nullptr;
        size_t len = 0;

        constexpr String_View() = default;
        constexpr String_View(const char *cstr);
        bool operator==(const String_View &other) const;
        bool operator!=(const String_View &other) const;

        String_View(const String_View &rhs) = default;

        char *cstr() const;
        String_View trim() const;
        String_View trim_left() const;
        String_View trim_right() const;
        String_View chop(char delim);
        String_View chop_left(size_t n);
    };

    struct String_Builder
    {
        char *data = nullptr;
        size_t len = 0;
        size_t cap = 0;

        String_Builder() = default;

        String_View to_sv() const;
        String_Builder &push(char c);
        String_Builder &push(const char *str);
        String_Builder &push_null();
        void resize();
    };

    Proc start_process(const Command &cmd);
} // maker

#ifdef MAKER_IMPLEMENTATION

maker::Proc maker::start_process(const Command &cmd)
{
    using namespace maker;

    Proc proc;

    ASSERT(
        posix_spawnp(&proc.pid, cmd.items[0], nullptr, nullptr, cmd.items, environ) == 0,
        "spawnp_failed"
    );

    return proc;
}

void maker::Proc::wait() const
{
    if (pid == 0) return;
    int status;
    waitpid(pid, &status, 0);
}

thread_local char maker::Temp_Buffer::buffer[];
thread_local maker::Temp_Buffer maker::tmp_buffer;

constexpr size_t item_size = sizeof(char*);

template<typename T>
constexpr inline void swap(T &val1, T &val2)
{
    val1 ^= val2;
    val2 ^= val1;
    val1 ^= val2;
}

void maker::Temp_Buffer::save()
{
    save_point = idx;
}

void maker::Temp_Buffer::load()
{
    swap(idx, save_point);
}

char *maker::Temp_Buffer::alloc(size_t n)
{
    ASSERT(idx + n <= start_size, "increase static storage size LOL");
    size_t start_pos = idx;
    idx += n;
    return &buffer[start_pos];
}

void *maker::Temp_Buffer::resize_buffer(void *ptr, size_t old_n, size_t new_n)
{
    if (!ptr) return nullptr;
    if (old_n >= new_n) return ptr;

    char *new_buf = alloc(new_n);
    char *src = (char *)ptr;

    for (size_t idx = 0; idx < old_n; ++idx)
        new_buf[idx] = src[idx];

    return new_buf;
}

void maker::Command::resize()
{
    if (capacity == 0)
    {
        capacity = 4;
        items = (char **) tmp_buffer.alloc(4 * sizeof(char*));
        return;
    }

    items = (char **)tmp_buffer.resize_buffer(items, capacity * sizeof(char*), capacity * 2 * sizeof(char*));
    capacity *= 2;
}

maker::Command &maker::Command::push(char *cmd)
{
    if (length >= capacity || capacity == 0) resize();
    items[length++] = cmd;
    return *this;
}

maker::Command &maker::Command::push_null()
{
    return push(nullptr);
}

constexpr size_t maker::temp::strlen(const char *str)
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
    do *dest++ = *str;
    while (*str++);
    return d;
}

char *maker::temp::strdup(const char *str)
{
    char *new_str = maker::tmp_buffer.alloc(strlen(str) + 1);
    return strcpy(new_str, str);
}

int maker::temp::strcmp(const char *left, const char *right)
{
    while(*left && (*left == *right))
    {
        left++;
        right++;
    }
    return *(const unsigned char*)left - *(const unsigned char*)right;
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

constexpr maker::String_View::String_View(const char *cstr)
    : data(cstr)
    , len(temp::strlen(cstr))
{}

char *maker::String_View::cstr() const
{
    if (!data) return nullptr;
    char *ret = maker::tmp_buffer.alloc(len + 1);
    for (size_t idx = 0; idx < len; ++idx)
        ret[idx] = data[idx];
    ret[len] = '\0';
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

maker::String_View maker::String_View::trim() const
{
    String_View sv = *this;
    size_t idx = sv.len - 1;
    while (isspace(sv.data[idx]) && idx != 0)
    {
        sv.len--;
        idx--;
    }

    while (isspace(*sv.data) && sv.len != 0)
    {
        sv.data++;
        sv.len--;
    }

    return sv;
}

maker::String_View maker::String_View::trim_left() const
{
    String_View sv = *this;
    while (isspace(*sv.data) && sv.len != 0)
    {
        sv.data++;
        sv.len--;
    }
    return sv;
}

maker::String_View maker::String_View::trim_right() const
{
    String_View sv = *this;
    size_t idx = sv.len - 1;
    while (isspace(sv.data[idx]) && idx != 0)
    {
        sv.len--;
        idx--;
    }
    return sv;
}

maker::String_View maker::String_View::chop(char delim)
{
    size_t i = 0;
    while (data[i] != delim && i < len)
        i++;

    String_View sv = *this;
    sv.len = i;

    if (i < len)
    {
        len -= i + 1;
        data += i + 1;
    }
    else
    {
        len -= i;
        data += i;
    }

    return sv;
}

maker::String_View maker::String_View::chop_left(size_t n)
{
    if (n > len) n = len;

    String_View sv = *this;
    sv.len = n;

    data += n;
    len -= n;

    return sv;
}

void maker::String_Builder::resize()
{
    if (cap == 0)
    {
        cap = 4;
        data = tmp_buffer.alloc(4);
        return;
    }

    data = (char *)tmp_buffer.resize_buffer(data, cap, cap * 2);
    cap *= 2;
}

maker::String_Builder &maker::String_Builder::push(char c)
{
    if (len >= cap) resize();
    data[len++] = c;
    return *this;
}

maker::String_Builder &maker::String_Builder::push(const char *str)
{
    if (!str) return *this;

    size_t str_len = temp::strlen(str);
    for (size_t i = 0; i < str_len; ++i)
    {
        if (len >= cap) resize();
        data[len++] = str[i];
    }
    return *this;
}

maker::String_View maker::String_Builder::to_sv() const
{
    String_View sv;
    sv.data = data;
    sv.len = len;
    return sv;
}

#endif
