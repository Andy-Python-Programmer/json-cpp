#pragma once

#include "frg/expected.hpp"
#include "frg/hash_map.hpp"
#include "frg/macros.hpp"
#include "frg/optional.hpp"
#include "frg/string.hpp"
#include "frg/unique.hpp"
#include "frg/vector.hpp"
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <variant>

struct MemoryAllocator {
public:
    void *allocate(size_t size);
    void free(void *ptr);
    void deallocate(void *ptr, size_t size);
    void *reallocate(void *ptr, size_t size);
};

enum class NumberType { Double, Integer };

namespace json {

enum class Error { UnexpectedEOF };

/// Returns whether the given character is considered a whitespace.
inline const bool is_whitespace(char value) {
    switch (value) {
    // Usual ASCII suspects
    case '\u{0009}': // \t
    case '\u{000B}': // vertical tab
    case '\u{000C}': // form feed
    case '\u{000D}': // \r
    case '\u{0020}': // space
        return true;

    default:
        return false;
    }
}

class JsonValue;

// frg::hash_map<Key, Value, Hash, Allocator>

using Object = frg::hash_map<frg::string<MemoryAllocator>, JsonValue,
                             frg::hash<frg::string_view>, MemoryAllocator>;

// frg::vector<Value, Allocator>
using Array = frg::vector<JsonValue, MemoryAllocator>;

class JsonValue {
    using Value = std::variant<std::monostate, bool, int64_t, double,
                               frg::string<MemoryAllocator>, Object, Array>;

public:
    JsonValue(Object &&value) : value(std::move(value)) {}
    JsonValue(Array &&value) : value(std::move(value)) {}
    JsonValue(frg::string<MemoryAllocator> &&value) : value(std::move(value)) {}
    JsonValue(bool value) : value(std::move(value)) {}
    JsonValue(int64_t value) : value(value) {}
    JsonValue(double value) : value(value) {}
    JsonValue(std::monostate value) : value(std::move(value)) {}
    JsonValue() : value(std::monostate{}) {}

    template <typename T> T &get() {
        if constexpr (std::is_same_v<T, void>)
            FRG_ASSERT(!"empty JsonValue");
        else if constexpr (std::is_same_v<T, Object>) {
            return static_cast<Object &>(std::get<Object>(this->value));
        } else if constexpr (std::is_same_v<T, Array>)
            return static_cast<Array &>(std::get<Array>(this->value));
        else
            return std::get<T>(static_cast<Value &>(this->value));
    }

    JsonValue &operator[](const size_t i) {
        if (!std::holds_alternative<Array>(this->value))
            FRG_ASSERT(!"JsonValue is not an array");

        return get<Array>()[i];
    }

    JsonValue &operator[](const char *key) {
        if (!std::holds_alternative<Object>(this->value))
            FRG_ASSERT(!"JsonValue is not an object");

        return get<Object>()[key];
    }

private:
    Value value;
};

class JsonParser {
public:
    JsonParser(const char *input, MemoryAllocator &allocator)
        : _input(input), _allocator(allocator) {}

    const char *_input;
    MemoryAllocator &_allocator;

    void eat_whitespace() {
        while (is_whitespace(*_input)) {
            ++_input;
        }
    }

    void advance_cursor() {
        FRG_ASSERT(*_input != '\0' && "Cannot advance past end of input");
        ++_input;
    }

    JsonValue parse() {
        eat_whitespace();

        if (frg::optional<JsonValue> number = parse_number();
            number.has_value())
            return std::move(number.value());

        switch (*_input) {
        case '{':
            return JsonValue(parse_object().unwrap());
        case '[':
            return JsonValue(parse_array().unwrap());
        case '"':
            return JsonValue(parse_string().unwrap());
        default:
            if (frg::optional<JsonValue> lit = parse_literal(); lit.has_value())
                return std::move(lit.value());
            else
                FRG_ASSERT(0);
        }
    }

    frg::expected<Error, Object> parse_object() {
        FRG_ASSERT(*_input == '{' && "Expected object to start with '{'");
        advance_cursor();
        eat_whitespace();

        auto object = Object(frg::hash<frg::string_view>{}, _allocator);

        if (*_input == '}') {
            advance_cursor();
            return object;
        }

        while (!is_eof() && *_input != '}') {
            auto key = parse_string().unwrap();
            // assert(key && "expected key");

            eat_whitespace();
            FRG_ASSERT(*_input == ':' && "expected ':'");

            advance_cursor();
            eat_whitespace();

            auto value = parse();
            // assert(value && "expected value");

            object.insert(key.begin(), std::move(value));
        }

        if (is_eof())
            return Error::UnexpectedEOF;

        advance_cursor();
        return object;
    }

    frg::expected<Error, Array> parse_array() {
        FRG_ASSERT(*_input == '[' && "expected array to start with '['");
        advance_cursor();
        eat_whitespace();

        auto array = Array(_allocator);

        if (*_input == ']') {
            advance_cursor();
            return array;
        }

        while (!is_eof() && *_input != ']') {
            auto value = parse();
            // assert(value && "expected value");

            array.push_back(std::move(value));
            eat_whitespace();

            if (*_input == ',') {
                advance_cursor();
                eat_whitespace();
            }
        }

        if (is_eof())
            return Error::UnexpectedEOF;

        advance_cursor();
        return array;
    }

    frg::expected<Error, frg::string<MemoryAllocator>> parse_string() {
        frg::string<MemoryAllocator> value(_allocator);

        FRG_ASSERT(*_input == '"' && "expected string to start with '\"'");
        advance_cursor();

        while (!is_eof() && *_input != '"') {
            value.push_back(*_input);
            advance_cursor();
        }

        if (is_eof())
            return Error::UnexpectedEOF;

        advance_cursor();
        return value;
    }

    frg::optional<JsonValue> parse_number() {
        frg::string<MemoryAllocator> number(_allocator);
        auto number_type = NumberType::Integer;

        if (*_input == '-') {
            number.push_back(*_input);
            advance_cursor();
        }

        while (!is_eof() && std::isdigit(*_input)) {
            number.push_back(*_input);
            advance_cursor();

            if (*_input == '.') {
                FRG_ASSERT(number_type == NumberType::Integer);

                number_type = NumberType::Double;
                number.push_back(*_input);
                advance_cursor();
            }
        }

        if (number.empty())
            return frg::null_opt;

        switch (number_type) {
        case NumberType::Double:
            return JsonValue(std::stod(number.begin()));
        case NumberType::Integer:
            return JsonValue((int64_t)std::stoi(number.begin()));
        }
    }

    frg::optional<JsonValue> parse_literal() {
        switch (*_input) {
        case 't': {
            consume_literal("true");
            return JsonValue(true);
        }

        case 'f': {
            consume_literal("false");
            return JsonValue(false);
        }

        case 'n': {
            consume_literal("null");
            return JsonValue(std::monostate{});
        }

        default:
            return frg::null_opt;
        }
    }

private:
    bool is_eof() const { return *_input == '\0'; }

    void consume_literal(const char *literal) {
        while (*literal != '\0') {
            FRG_ASSERT(*_input == *literal);
            ++_input;
            ++literal;
        }
    }
};
} // namespace json
