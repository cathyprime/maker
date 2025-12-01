#define MAKER_UT
#define MAKER_BUFFER_DEFAULT 10
#define MAKER_IMPLEMENTATION
#include "maker.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

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
