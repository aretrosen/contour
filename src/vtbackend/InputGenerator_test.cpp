// SPDX-License-Identifier: Apache-2.0
#include <vtbackend/InputGenerator.h>

#include <crispy/escape.h>

#include <libunicode/convert.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace std;
using namespace vtbackend;
using Buffer = vtbackend::InputGenerator::Sequence;
using crispy::escape;

TEST_CASE("InputGenerator.Modifier.encodings")
{
    // Ensures we can construct the correct values that are needed
    // as parameters for input events in the VT protocol.

    auto constexpr Alt = Modifiers { Modifier::Alt };
    auto constexpr Shift = Modifiers { Modifier::Shift };
    auto constexpr Control = Modifiers { Modifier::Control };
    auto constexpr Super = Modifiers { Modifier::Super };

    CHECK((1 + Shift.value()) == 2);
    CHECK((1 + Alt.value()) == 3);
    CHECK((1 + (Shift | Alt).value()) == 4);
    CHECK((1 + Control.value()) == 5);
    CHECK((1 + (Shift | Control).value()) == 6);
    CHECK((1 + (Alt | Control).value()) == 7);
    CHECK((1 + (Shift | Alt | Control).value()) == 8);
    CHECK((1 + Super.value()) == 9);
    CHECK((1 + (Super | Shift).value()) == 10);
    CHECK((1 + (Super | Alt).value()) == 11);
    CHECK((1 + (Super | Alt | Shift).value()) == 12);
    CHECK((1 + (Super | Control).value()) == 13);
    CHECK((1 + (Super | Control | Shift).value()) == 14);
    CHECK((1 + (Super | Control | Alt).value()) == 15);
    CHECK((1 + (Super | Control | Alt | Shift).value()) == 16);
}

TEST_CASE("InputGenerator.consume")
{
    auto received = string {};
    auto input = InputGenerator {};
    input.generateRaw("ABCDEF"sv);
    REQUIRE(input.peek() == "ABCDEF"sv);
    input.consume(2);
    REQUIRE(input.peek() == "CDEF"sv);
    input.consume(3);
    REQUIRE(input.peek() == "F"sv);

    input.generateRaw("abcdef"sv);
    REQUIRE(input.peek() == "Fabcdef"sv);
    input.consume(7);
    REQUIRE(input.peek() == ""sv);
}

TEST_CASE("InputGenerator.Ctrl+Space", "[terminal,input]")
{
    auto input = InputGenerator {};
    input.generate(L' ', Modifier::Control, KeyboardEventType::Press);
    CHECK(escape(input.peek()) == "\\x00");
}

TEST_CASE("InputGenerator.Ctrl+A", "[terminal,input]")
{
    auto input = InputGenerator {};
    input.generate('A', Modifier::Control, KeyboardEventType::Press);
    auto const c0 = string(1, static_cast<char>(0x01));
    REQUIRE(escape(input.peek()) == escape(c0));
}

TEST_CASE("InputGenerator.Ctrl+D", "[terminal,input]")
{
    auto input = InputGenerator {};
    input.generate('D', Modifier::Control, KeyboardEventType::Press);
    auto const c0 = string(1, static_cast<char>(0x04));
    REQUIRE(escape(input.peek()) == escape(c0));
}

TEST_CASE("InputGenerator.Ctrl+[", "[terminal,input]")
{
    auto input = InputGenerator {};
    input.generate('[', Modifier::Control, KeyboardEventType::Press);
    auto const c0 = string(1, static_cast<char>(0x1b)); // 27
    REQUIRE(escape(input.peek()) == escape(c0));
}

TEST_CASE("InputGenerator.Ctrl+\\", "[terminal,input]")
{
    auto input = InputGenerator {};
    input.generate('\\', Modifier::Control, KeyboardEventType::Press);
    auto const c0 = string(1, static_cast<char>(0x1c)); // 28
    REQUIRE(escape(input.peek()) == escape(c0));
}

TEST_CASE("InputGenerator.Ctrl+]", "[terminal,input]")
{
    auto input = InputGenerator {};
    input.generate(']', Modifier::Control, KeyboardEventType::Press);
    auto const c0 = string(1, static_cast<char>(0x1d)); // 29
    REQUIRE(escape(input.peek()) == escape(c0));
}

TEST_CASE("InputGenerator.Ctrl+^", "[terminal,input]")
{
    auto input = InputGenerator {};
    input.generate('^', Modifier::Control, KeyboardEventType::Press);
    auto const c0 = string(1, static_cast<char>(0x1e)); // 30
    REQUIRE(escape(input.peek()) == escape(c0));
}

TEST_CASE("InputGenerator.Ctrl+_", "[terminal,input]")
{
    auto input = InputGenerator {};
    input.generate('_', Modifier::Control, KeyboardEventType::Press);
    auto const c0 = string(1, static_cast<char>(0x1f)); // 31
    REQUIRE(escape(input.peek()) == escape(c0));
}

TEST_CASE("InputGenerator.all(Ctrl + A..Z)", "[terminal,input]")
{
    for (char ch = 'A'; ch <= 'Z'; ++ch)
    {
        INFO(fmt::format("Testing Ctrl+{}", ch));
        auto input = InputGenerator {};
        input.generate(static_cast<char32_t>(ch), Modifier::Control, KeyboardEventType::Press);
        auto const c0 = string(1, static_cast<char>(ch - 'A' + 1));
        REQUIRE(escape(input.peek()) == escape(c0));
    }
}

// {{{ ExtendedKeyboardInputGenerator

TEST_CASE("ExtendedKeyboardInputGenerator.CSIu.Ctrl+L", "[terminal,input]")
{
    auto input = ExtendedKeyboardInputGenerator {};
    input.enter(KeyboardEventFlag::DisambiguateEscapeCodes);
    input.generateChar('L', 'L', Modifier::Control, KeyboardEventType::Press);
    REQUIRE(escape(input.take()) == escape("\033[108;5u"sv));
}

TEST_CASE("ExtendedKeyboardInputGenerator.CSIu.Escape", "[terminal,input]")
{
    auto input = ExtendedKeyboardInputGenerator {};
    input.enter(KeyboardEventFlag::DisambiguateEscapeCodes);

    input.generateKey(Key::Escape, Modifier::None, KeyboardEventType::Press);
    REQUIRE(escape(input.take()) == escape("\033[27u"sv));

    input.generateKey(Key::Escape, Modifier::Shift, KeyboardEventType::Press);
    REQUIRE(escape(input.take()) == escape("\033[27;2u"sv));

    // repeat is not being encoded here, because we did not request ReportEventTypes.
    input.generateKey(Key::Escape, Modifier::Shift, KeyboardEventType::Repeat);
    REQUIRE(escape(input.take()) == escape("\033[27;2u"sv));

    input.generateKey(Key::Escape, Modifier::Shift, KeyboardEventType::Release);
    REQUIRE(escape(input.take()) == escape(""sv));

    // Now we do request ReportEventTypes, so we should get the repeat and release event.
    input.flags().enable(KeyboardEventFlag::ReportEventTypes);

    input.generateKey(Key::Escape, Modifier::Shift, KeyboardEventType::Repeat);
    REQUIRE(escape(input.take()) == escape("\033[27;1:2u"sv));

    input.generateKey(Key::Escape, Modifier::Shift, KeyboardEventType::Release);
    REQUIRE(escape(input.take()) == escape("\033[27;1:3u"sv));
}

// }}}
