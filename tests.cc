#define MAKER_UT
#define MAKER_IMPLEMENTATION
#include "maker.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <cstring>
#include <string>
#include <cstdio>

using namespace maker;

template<typename T>
constexpr static inline void ignore_result(T t) {}

namespace maker
{
    doctest::String toString(const String_View &sv)
    {
        char buffer[100];
        std::sprintf(buffer, "\"%.*s\"_sv", (int)sv.len, sv.data);
        return std::string(buffer).c_str();
    }
};

TEST_SUITE_BEGIN("Temp_Buffer");
TEST_CASE("Allocation"
          * doctest::description("using .alloc and .alloc_count to allocate memory"))
{
    Temp_Buffer *buf = new Temp_Buffer{};
    size_t offset = buf->start_size - 10;
    const_cast<size_t&>(buf->idx) = offset;

    REQUIRE(buf->idx == offset);
    REQUIRE(buf->save_point == 0);

    SUBCASE("")
    {
        SUBCASE("Allocation out of range returns nullptr")
        {
            void *ptr = buf->alloc(11);
            CHECK(ptr == nullptr);
        }

        SUBCASE("Allocation moves idx by requsted size")
        {
            size_t sz = 5;
            size_t idx_start = buf->idx;
            ignore_result(buf->alloc(sz));
            CHECK(buf->idx == idx_start + sz);
        }

        SUBCASE("Allocated memory in buffer")
        {
            char *ptr = (char*) buf->alloc(1);
            *ptr++ = 'h';
            *ptr = 'i';
            CHECK(buf->buffer[offset + 0] == 'h');
            CHECK(buf->buffer[offset + 1] == 'i');
        }
    }

    SUBCASE("")
    {
        SUBCASE("Allocation out of range returns nullptr")
        {
            void *ptr = buf->alloc_count(3, 5);
            CHECK(ptr == nullptr);
        }

        SUBCASE("Allocation moves idx by requsted size")
        {
            size_t sz = 3;
            size_t count = 2;
            size_t idx_start = buf->idx;
            ignore_result(buf->alloc_count(count, sz));
            CHECK(buf->idx == idx_start + (count * sz));
        }

        SUBCASE("Allocated memory in buffer")
        {
            char *ptr = (char*) buf->alloc_count(1, 1);
            *ptr++ = 'h';
            *ptr = 'i';
            CHECK(buf->buffer[offset + 0] == 'h');
            CHECK(buf->buffer[offset + 1] == 'i');
        }
    }

    delete buf;
}

TEST_CASE("Saving"
          * doctest::description("Checking the ability to save/load buffer"))
{
    Temp_Buffer *buf = new Temp_Buffer{};

    REQUIRE(buf->idx == 0);
    REQUIRE(buf->save_point == 0);
    ignore_result(buf->alloc(5));
    REQUIRE(buf->idx == 5);
    REQUIRE(buf->save_point == 0);

    SUBCASE("Save")
    {
        buf->save();
        CHECK(buf->save_point == 5);
    }

    SUBCASE("Load")
    {
        buf->load();
        CHECK(buf->idx == 0);
    }

    delete buf;
}
TEST_SUITE_END();

TEST_SUITE_BEGIN("Command");
TEST_CASE("Resize"
          * doctest::description("Resizing the internal buffer"))
{
    Command cmd;
    REQUIRE(cmd.length == 0);
    REQUIRE(cmd.capacity == 0);

    SUBCASE("Initial resize")
    {
        cmd.resize();
        CHECK(cmd.capacity == 4);
        CHECK(cmd.items != nullptr);

        SUBCASE("Subsequent resize")
        {
            cmd.resize();
            CHECK(cmd.capacity == 8);
            CHECK(cmd.items != nullptr);
        }
    }
}

TEST_CASE("Push"
          * doctest::description("Pushing to the command"))
{
    Command cmd;
    char word[] = "Word";

    SUBCASE("First push will resize")
    {
        cmd.push(word);
        CHECK(cmd.capacity == 4);
        CHECK(cmd.length == 1);
        CHECK(*cmd.items == (char*)word);

        SUBCASE("Many resizes")
        {
            cmd.push(word);
            cmd.push(word);
            cmd.push(word);
            REQUIRE(cmd.capacity == 4);
            REQUIRE(cmd.length == 4);

            cmd.push(word);
            CHECK(cmd.capacity == 8);
            CHECK(cmd.length == 5);
        }
    }
}
TEST_SUITE_END();

TEST_SUITE_BEGIN("String operations");
TEST_CASE("strlen")
{
    SUBCASE("valid string")
    {
        char word_from_arr[14] = {
            'H', 'e', 'l', 'l', 'o', ',', ' ',
            'w', 'o', 'r', 'l', 'd', '!', '\0'
        };
        CHECK(temp::strlen(word_from_arr) == 13);

        char word_from_literal[6] = "Hello";
        CHECK(temp::strlen(word_from_literal) == 5);
    }

    SUBCASE("just zero byte")
    {
        const char *word = "\0";
        CHECK(temp::strlen(word) == 0);
    }

    SUBCASE("nullptr")
    {
        CHECK(temp::strlen(nullptr) == 0);
    }
}

TEST_CASE("strcpy")
{
    SUBCASE("valid string")
    {
        const char *word = "Hello";
        char buffer[6];

        temp::strcpy(buffer, word);
        CHECK(std::strcmp(buffer, word) == 0);
    }

    SUBCASE("just zero byte")
    {
        const char *word = "\0";
        char buffer[5] = "xxxx";

        auto *result = temp::strcpy(buffer, word);
        CHECK(result == buffer);
        CHECK(buffer[0] == '\0');
        CHECK(buffer[1] == 'x');
        CHECK(buffer[2] == 'x');
        CHECK(buffer[3] == 'x');
        CHECK(buffer[4] == '\0');
    }

    SUBCASE("nullptr in")
    {
        char buffer[5] = "xxxx";
        auto *result = temp::strcpy(buffer, nullptr);

        CHECK(result == nullptr);
        CHECK(buffer[0] == 'x');
        CHECK(buffer[1] == 'x');
        CHECK(buffer[2] == 'x');
        CHECK(buffer[3] == 'x');
        CHECK(buffer[4] == '\0');
    }

    SUBCASE("nullptr out")
    {
        auto *result = temp::strcpy(nullptr, "\0");

        CHECK(result == nullptr);
    }
}

TEST_CASE("strdup")
{
    SUBCASE("valid string")
    {
        const char *word = "Hello";
        auto *copy = temp::strdup(word);

        CHECK(std::strcmp(copy, word) == 0);
    }

    SUBCASE("just zero byte")
    {
        auto *copy = temp::strdup("\0");

        CHECK(copy[0] == '\0');
    }

    SUBCASE("nullptr")
    {
        auto *copy = temp::strdup(nullptr);

        CHECK(copy == nullptr);
    }
}

TEST_CASE("strcmp")
{
    SUBCASE("same strings")
    {
        const char *left = "hello";
        const char *right = "hello";

        CHECK(temp::strcmp(left, right) == 0);
    }

    SUBCASE("left longer")
    {
        const char *left = "hellope";
        const char *right = "hello";

        CHECK(temp::strcmp(left, right) > 0);
    }

    SUBCASE("right longer")
    {
        const char *left = "hello";
        const char *right = "hellope";

        CHECK(temp::strcmp(left, right) < 0);
    }

    SUBCASE("same length, different chars; should be smaller")
    {
        const char *left = "aaa";
        const char *right = "zzz";

        CHECK(temp::strcmp(left, right) < 0);
    }

    SUBCASE("same length, different chars; should be larger")
    {
        const char *left = "zzz";
        const char *right = "aaa";

        CHECK(temp::strcmp(left, right) > 0);
    }
}
TEST_CASE("strncmp")
{
    SUBCASE("same strings")
    {
        const char *left = "hello";
        const char *right = "hello";

        CHECK(temp::strncmp(left, right, 5) == 0);
    }

    SUBCASE("left longer, len 5")
    {
        const char *left = "hellope";
        const char *right = "hello";

        CHECK(temp::strncmp(left, right, 5) == 0);
    }

    SUBCASE("left longer")
    {
        const char *left = "hellope";
        const char *right = "hello";

        CHECK(temp::strncmp(left, right, 8) > 0);
    }

    SUBCASE("right longer, len 5")
    {
        const char *left = "hello";
        const char *right = "hellope";

        CHECK(temp::strncmp(left, right, 5) == 0);
    }

    SUBCASE("right longer")
    {
        const char *left = "hello";
        const char *right = "hellope";

        CHECK(temp::strncmp(left, right, 8) < 0);
    }

    SUBCASE("same length, different chars; should be smaller")
    {
        const char *left = "aaa";
        const char *right = "zzz";

        CHECK(temp::strncmp(left, right, 3) < 0);
    }

    SUBCASE("same length, different chars; should be larger")
    {
        const char *left = "zzz";
        const char *right = "aaa";

        CHECK(temp::strncmp(left, right, 3) > 0);
    }
}
TEST_SUITE_END();

TEST_SUITE_BEGIN("String_View");
TEST_CASE("Cstr")
{
    SUBCASE("from cstr")
    {
        SUBCASE("valid string")
        {
            const char *word = "hello";
            String_View sv = word;
            CHECK(sv.len == 5);
            CHECK(sv.data == word);
        }

        SUBCASE("just null byte")
        {
            const char *word = "";
            String_View sv = word;
            CHECK(sv.len == 0);
            CHECK(sv.data == word);
        }

        SUBCASE("nullptr")
        {
            String_View sv = nullptr;
            CHECK(sv.len == 0);
            CHECK(sv.data == nullptr);
        }
    }

    SUBCASE("to cstr")
    {
        SUBCASE("")
        {
            String_View sv = "hello";
            REQUIRE(sv.len == 5);
            REQUIRE(sv.data != nullptr);

            SUBCASE("valid string")
            {
                size_t start_idx = tmp_buffer.idx;
                char *word = sv.cstr();
                size_t after = tmp_buffer.idx;
                CHECK(after - start_idx == 6);
                CHECK(std::strcmp(word, "hello") == 0);
            }
        }

        SUBCASE("")
        {
            String_View sv = "";
            REQUIRE(sv.len == 0);
            REQUIRE(sv.data != nullptr);

            SUBCASE("just null byte")
            {
                size_t start_idx = tmp_buffer.idx;
                char *word = sv.cstr();
                size_t after = tmp_buffer.idx;
                CHECK(after - start_idx == 1);
                CHECK(std::strcmp(word, "") == 0);
            }
        }

        SUBCASE("")
        {
            String_View sv = nullptr;
            REQUIRE(sv.len == 0);
            REQUIRE(sv.data == nullptr);

            SUBCASE("nullptr")
            {
                size_t start_idx = tmp_buffer.idx;
                char *word = sv.cstr();
                size_t after = tmp_buffer.idx;
                CHECK(after - start_idx == 0);
                CHECK(word == nullptr);
            }
        }
    }
}
TEST_CASE("comparison")
{
    String_View sv1;
    String_View sv2;

    SUBCASE("same word, same length")
    {
        const char *word = "hello";
        sv1.len = 5;
        sv1.data = word;
        sv2.len = 5;
        sv2.data = word;

        CHECK(sv1 == sv2);
    }

    SUBCASE("same word, same length, different pointers")
    {
        const char *word1 = "hello";
        const char *word2 = ::strdup("hello");
        REQUIRE(word1 != word2);

        sv1.len = 6;
        sv2.len = 6;
        sv1.data = word1;
        sv2.data = word2;
        CHECK(sv1 == sv2);
    }

    SUBCASE("different word, different lengths")
    {
        sv1.len = 3;
        sv1.data = "aa";
        sv2.len = 4;
        sv2.data = "bbb";

        CHECK_FALSE(sv1 == sv2);
    }

    SUBCASE("different word, same size")
    {
        sv1.len = 3;
        sv2.len = 3;
        sv1.data = "aa";
        sv2.data = "bb";

        CHECK_FALSE(sv1 == sv2);
    }

    SUBCASE("one nullptr")
    {
        const char *word = "word";
        sv1.len = 5;
        sv1.data = word;
        sv2.len = 0;
        sv2.data = nullptr;

        CHECK_FALSE(sv1 == sv2);
    }

    SUBCASE("different word")
    {
        sv1.len = 2;
        sv1.data = "a";
        sv2.len = 3;
        sv2.data = "bb";

        CHECK_FALSE(sv1 == sv2);
    }
}
TEST_SUITE_END();
