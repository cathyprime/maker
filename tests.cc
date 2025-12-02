#define MAKER_UT
#define MAKER_IMPLEMENTATION
#include "maker.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <cstring>
#include <cstdio>

using namespace maker;

template<typename T>
constexpr static inline void ignore_result(T t) {}

template<typename F>
inline void assert_alloc_delta(int delta, F f)
{
    size_t before = tmp_buffer.idx;
    f();
    size_t after = tmp_buffer.idx;
    CHECK(after - before == delta);
}

namespace maker
{
    doctest::String toString(const String_View &sv)
    {
        char buffer[100];
        std::sprintf(buffer, "\"%.*s\"_sv", (int)sv.len, sv.data);
        return doctest::String(buffer).c_str();
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

    SUBCASE("Allocation out of range dies")
    {
        CHECK_THROWS(buf->alloc(11));
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
        assert_alloc_delta(4, [&] {
            cmd.resize();
        });
        CHECK(cmd.capacity == 4);
        CHECK(cmd.items != nullptr);

        SUBCASE("Subsequent resize")
        {
            assert_alloc_delta(8, [&] {
                cmd.resize();
            });
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
        assert_alloc_delta(4, [&] {
            cmd.push(word);
        });
        CHECK(cmd.capacity == 4);
        CHECK(cmd.length == 1);

        SUBCASE("Many pushes")
        {
            cmd.push(word);
            cmd.push(word);
            cmd.push(word);
            REQUIRE(cmd.capacity == 4);
            REQUIRE(cmd.length == 4);

            assert_alloc_delta(8, [&] {
                cmd.push(word);
            });
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
TEST_CASE("Manipulating")
{
    String_View sv = "  hello  ";

    SUBCASE("Trim")
    {
        CAPTURE(sv);
        auto cpy = sv.trim();
        CAPTURE(cpy);
        CHECK(cpy.len == 5);
        CHECK(std::strncmp(cpy.data, "hello", 5) == 0);
    }

    SUBCASE("Left")
    {
        CAPTURE(sv);
        auto cpy = sv.trim_left();
        CAPTURE(cpy);
        CHECK(cpy.len == 7);
        CHECK(std::strncmp(cpy.data, "hello  ", 5) == 0);
    }

    SUBCASE("Right")
    {
        CAPTURE(sv);
        auto cpy = sv.trim_right();
        CAPTURE(cpy);
        CHECK(cpy.len == 7);
        CHECK(std::strncmp(cpy.data, "  hello", 7) == 0);
    }

    SUBCASE("Chop")
    {
        auto cpy = sv.chop('l');

        CAPTURE(cpy);
        CAPTURE(sv);

        CHECK(cpy.len == 4);
        CHECK(std::strncmp(cpy.data, "  he", 4) == 0);

        CHECK(sv.len == 4);
        CHECK(std::strncmp(sv.data, "lo  ", 4) == 0);
    }

    SUBCASE("Chop left")
    {
        auto cpy = sv.chop_left(5);

        CAPTURE(cpy);
        CAPTURE(sv);

        CHECK(cpy.len == 5);
        CHECK(std::strncmp(cpy.data, "  hel", 5) == 0);

        CHECK(sv.len == 4);
        CHECK(std::strncmp(sv.data, "lo  ", 4) == 0);
    }
}
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
                char *word;
                assert_alloc_delta(6, [&] {
                    word = sv.cstr();
                });
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
                char *word;
                assert_alloc_delta(1, [&] {
                    word = sv.cstr();
                });
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
                char *word;
                assert_alloc_delta(0, [&] {
                    word = sv.cstr();
                });
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
        char word1[] = "hello";
        char word2[] = "hello";
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

TEST_SUITE_BEGIN("String_Builder");
TEST_CASE("Resize sb"
          * doctest::description("Resizing the internal buffer"))
{
    String_Builder sb;
    REQUIRE(sb.len == 0);
    REQUIRE(sb.cap == 0);

    SUBCASE("Initial resize")
    {
        assert_alloc_delta(4, [&] {
            sb.resize();
        });
        CHECK(sb.cap == 4);
        CHECK(sb.data != nullptr);

        SUBCASE("Subsequent resize")
        {
            assert_alloc_delta(8, [&] {
                sb.resize();
            });
            CHECK(sb.cap == 8);
            CHECK(sb.data != nullptr);
        }
    }
}

TEST_CASE("Push sb")
{
    SUBCASE("char once")
    {
        String_Builder sb;
        assert_alloc_delta(4, [&] {
            sb.push('c');
        });

        SUBCASE("char many")
        {
            REQUIRE(sb.len == 1);
            REQUIRE(sb.cap == 4);

            sb.push('h');
            sb.push('a');
            sb.push('r');

            assert_alloc_delta(8, [&] {
                sb.push('r');
            });
        }
    }

    SUBCASE("nullptr")
    {
        String_Builder sb;
        assert_alloc_delta(0, [&]{
            sb.push(nullptr);
        });

        CHECK(sb.cap == 0);
        CHECK(sb.len == 0);
        CHECK(sb.data == nullptr);

        SUBCASE("valid string")
        {
            assert_alloc_delta(12, [&] {
                sb.push("hello");
            });

            CHECK(sb.len == 5);
            CHECK(std::strncmp(sb.data, "hello", 5) == 0);

            SUBCASE("join")
            {
                sb.push(' ');
                assert_alloc_delta(16, [&] {
                    sb.push("world");
                });
                CHECK(std::strncmp(sb.data, "hello world", 11) == 0);

                SUBCASE("to sv")
                {
                    auto sv = sb.to_sv();
                    CHECK(std::strncmp(sv.data, sb.data, sb.len) == 0);
                }
            }
        }
    }
}
TEST_SUITE_END();
