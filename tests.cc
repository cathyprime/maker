#define MAKER_UT
#define MAKER_BUFFER_DEFAULT 10
#define MAKER_IMPLEMENTATION
#include "maker.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <cstring>

using namespace maker;

template<typename T>
constexpr static inline void ignore_result(T t) {}

TEST_SUITE_BEGIN("Temp_Buffer");
TEST_CASE("Allocation"
          * doctest::description("using .alloc and .alloc_count to allocate memory"))
{
    Temp_Buffer *buf = new Temp_Buffer{};

    REQUIRE(buf->idx == 0);
    REQUIRE(buf->save_point == 0);

    SUBCASE("Default size macro respected")
    {
        CHECK(buf->start_size == MAKER_BUFFER_DEFAULT);
    }

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
            CHECK(buf->buffer[0] == 'h');
            CHECK(buf->buffer[1] == 'i');
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
            CHECK(buf->buffer[0] == 'h');
            CHECK(buf->buffer[1] == 'i');
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
    REQUIRE(buf->save_point == 0);
    REQUIRE(buf->idx == 5);

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

    SUBCASE("just zerobyte")
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

    SUBCASE("just zerobyte")
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

    SUBCASE("just zerobyte")
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

TEST_SUITE_END();
