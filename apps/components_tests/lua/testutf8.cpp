#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <components/lua/luastate.hpp>
#include <components/testing/expecterror.hpp>

#include <string>

// The utf8 library that the scripts get, with the behavior of the one of Lua 5.3: positions count bytes from one, a
// negative position counts from the end and a bad sequence is reported instead of being skipped.
namespace
{
    using namespace testing;

    struct LuaUtf8Test : Test
    {
        LuaUtil::LuaState mLuaState{ nullptr, nullptr };

        LuaUtf8Test()
        {
            sol::state_view lua = mLuaState.unsafeState();
            // "H", an a with two dots, the euro sign and a smiling face: one, two, three and four bytes long.
            lua.safe_script(R"(
                text = "H\xC3\xA4\xE2\x82\xAC\xF0\x9F\x98\x80"
                function join(...)
                    local t = {}
                    for i = 1, select('#', ...) do t[i] = tostring((select(i, ...))) end
                    return table.concat(t, ",")
                end
            )");
        }

        std::string eval(const std::string& code)
        {
            sol::state_view lua = mLuaState.unsafeState();
            return lua.safe_script("return tostring(" + code + ")").get<std::string>();
        }
    };

    TEST_F(LuaUtf8Test, charBuildsASequenceFromCodepoints)
    {
        EXPECT_EQ(eval("utf8.char(72, 228, 8364, 128512) == text"), "true");
        EXPECT_EQ(eval("#utf8.char(72, 228, 8364, 128512)"), "10");
        EXPECT_EQ(eval("utf8.char()"), "");
        EXPECT_EQ(eval("utf8.char(0x7F) == '\\x7F'"), "true");
        EXPECT_EQ(eval("utf8.char(0x80) == '\\xC2\\x80'"), "true");
        EXPECT_EQ(eval("utf8.char(0x7FF) == '\\xDF\\xBF'"), "true");
        EXPECT_EQ(eval("utf8.char(0x800) == '\\xE0\\xA0\\x80'"), "true");
        EXPECT_EQ(eval("utf8.char(0xFFFF) == '\\xEF\\xBF\\xBF'"), "true");
        EXPECT_EQ(eval("utf8.char(0x10000) == '\\xF0\\x90\\x80\\x80'"), "true");
        EXPECT_EQ(eval("utf8.char(0x10FFFF) == '\\xF4\\x8F\\xBF\\xBF'"), "true");
    }

    TEST_F(LuaUtf8Test, charRefusesWhatIsNotACodepoint)
    {
        sol::state_view lua = mLuaState.unsafeState();
        EXPECT_ERROR(lua.safe_script("utf8.char(72, -1)"), "bad argument #2 to 'char' (value out of range)");
        EXPECT_ERROR(lua.safe_script("utf8.char(0x110000)"), "bad argument #1 to 'char' (value out of range)");
        EXPECT_ERROR(lua.safe_script("utf8.char(1.5)"), "number has no integer representation");
        EXPECT_ERROR(lua.safe_script("utf8.char({})"), "number expected, got table");
    }

    TEST_F(LuaUtf8Test, charpatternMatchesOneCharacterAtATime)
    {
        EXPECT_EQ(eval("(function() local n = 0 for _ in string.gmatch(text, utf8.charpattern) do n = n + 1 end "
                       "return n end)()"),
            "4");
    }

    TEST_F(LuaUtf8Test, codesWalksOverTheCharacters)
    {
        EXPECT_EQ(eval("(function() local t = {} for p, c in utf8.codes(text) do t[#t + 1] = p .. ':' .. c end "
                       "return table.concat(t, ' ') end)()"),
            "1:72 2:228 4:8364 7:128512");
        EXPECT_EQ(eval("(function() local n = 0 for _ in utf8.codes('') do n = n + 1 end return n end)()"), "0");
    }

    TEST_F(LuaUtf8Test, codesStopsAtABrokenSequence)
    {
        sol::state_view lua = mLuaState.unsafeState();
        EXPECT_ERROR(lua.safe_script("for _ in utf8.codes('a\\xFF') do end"), "Invalid UTF-8 code");
        EXPECT_ERROR(lua.safe_script("for _ in utf8.codes('a\\xC3') do end"), "Invalid UTF-8 code");
    }

    TEST_F(LuaUtf8Test, lenCountsCharacters)
    {
        EXPECT_EQ(eval("utf8.len(text)"), "4");
        EXPECT_EQ(eval("utf8.len('')"), "0");
        EXPECT_EQ(eval("utf8.len('abc')"), "3");
        EXPECT_EQ(eval("utf8.len(text, 2)"), "3");
        EXPECT_EQ(eval("utf8.len(text, 2, 4)"), "2");
        EXPECT_EQ(eval("utf8.len(text, -4)"), "1");
        EXPECT_EQ(eval("utf8.len(text, 1, -5)"), "3");
        EXPECT_EQ(eval("utf8.len(text, 1, 0)"), "0");
        EXPECT_EQ(eval("utf8.len('abc', 4)"), "0");
    }

    TEST_F(LuaUtf8Test, lenReportsWhereASequenceIsBroken)
    {
        EXPECT_EQ(eval("join(utf8.len('a\\xFFb'))"), "nil,2");
        EXPECT_EQ(eval("join(utf8.len('\\xC3'))"), "nil,1");
        EXPECT_EQ(eval("join(utf8.len('ab\\xE2\\x82'))"), "nil,3");
    }

    TEST_F(LuaUtf8Test, lenRefusesPositionsOutsideTheString)
    {
        sol::state_view lua = mLuaState.unsafeState();
        EXPECT_ERROR(lua.safe_script("utf8.len('abc', 0)"), "initial position out of bounds");
        EXPECT_ERROR(lua.safe_script("utf8.len('abc', 1, 4)"), "final position out of bounds");
        EXPECT_ERROR(lua.safe_script("utf8.len('abc', 'x')"), "bad argument #2 to 'len' (number expected, got string)");
    }

    TEST_F(LuaUtf8Test, codepointReadsTheCharactersOfARange)
    {
        EXPECT_EQ(eval("join(utf8.codepoint(text, 1, -1))"), "72,228,8364,128512");
        EXPECT_EQ(eval("join(utf8.codepoint(text))"), "72");
        EXPECT_EQ(eval("join(utf8.codepoint(text, 2))"), "228");
        EXPECT_EQ(eval("join(utf8.codepoint(text, 4, 7))"), "8364,128512");
        EXPECT_EQ(eval("join(utf8.codepoint(text, -4))"), "128512");
        EXPECT_EQ(eval("select('#', utf8.codepoint(text, 3, 2))"), "0");
    }

    TEST_F(LuaUtf8Test, codepointRefusesBadRangesAndBadSequences)
    {
        sol::state_view lua = mLuaState.unsafeState();
        EXPECT_ERROR(lua.safe_script("utf8.codepoint('abc', 0)"), "initial position out of bounds");
        EXPECT_ERROR(lua.safe_script("utf8.codepoint('abc', 1, 4)"), "final position out of bounds");
        EXPECT_ERROR(lua.safe_script("utf8.codepoint('\\xFF')"), "Invalid UTF-8 code");
        EXPECT_ERROR(lua.safe_script("utf8.codepoint('a\\xC3', 1, 2)"), "Invalid UTF-8 code");
    }

    TEST_F(LuaUtf8Test, offsetFindsWhereACharacterStarts)
    {
        // The characters start at the bytes 1, 2, 4 and 7 and the string ends at the byte 10.
        EXPECT_EQ(eval("utf8.offset(text, 1)"), "1");
        EXPECT_EQ(eval("utf8.offset(text, 2)"), "2");
        EXPECT_EQ(eval("utf8.offset(text, 3)"), "4");
        EXPECT_EQ(eval("utf8.offset(text, 4)"), "7");
        EXPECT_EQ(eval("utf8.offset(text, 5)"), "11");
        EXPECT_EQ(eval("tostring(utf8.offset(text, 6))"), "nil");
        EXPECT_EQ(eval("utf8.offset(text, 0)"), "1");
        EXPECT_EQ(eval("utf8.offset(text, -1)"), "7");
        EXPECT_EQ(eval("utf8.offset(text, -2)"), "4");
        EXPECT_EQ(eval("utf8.offset(text, -4)"), "1");
        EXPECT_EQ(eval("tostring(utf8.offset(text, -5))"), "nil");
    }

    TEST_F(LuaUtf8Test, offsetCanStartFromAnotherPosition)
    {
        EXPECT_EQ(eval("utf8.offset(text, 1, 2)"), "2");
        EXPECT_EQ(eval("utf8.offset(text, 2, 2)"), "4");
        EXPECT_EQ(eval("utf8.offset(text, -1, 4)"), "2");
        EXPECT_EQ(eval("utf8.offset(text, 0, 3)"), "2");
        EXPECT_EQ(eval("utf8.offset(text, 0, 6)"), "4");
        EXPECT_EQ(eval("utf8.offset(text, 1, -4)"), "7");
    }

    TEST_F(LuaUtf8Test, offsetRefusesToStartInTheMiddleOfACharacter)
    {
        sol::state_view lua = mLuaState.unsafeState();
        EXPECT_ERROR(lua.safe_script("utf8.offset(text, 1, 3)"), "initial position is a continuation byte");
        EXPECT_ERROR(lua.safe_script("utf8.offset(text, 1, 12)"), "position out of bounds");
    }
}
