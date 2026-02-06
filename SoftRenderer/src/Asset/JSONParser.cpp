#include "Asset/JSONParser.h"

#include <cctype>
#include <charconv>
#include <limits>
#include <chrono>
#include <cstdio>
#define NOMINMAX
#include <windows.h>
#undef min
#undef max

namespace SR {

/**
 * @brief 数组索引访问 (const)
 */
const JSONValue& JSONValue::operator[](size_t index) const {
    static JSONValue kNull;
    if (type != Type::Array || index >= arrayValue.size()) {
        return kNull;
    }
    return arrayValue[index];
}

/**
 * @brief 数组索引访问
 */
JSONValue& JSONValue::operator[](size_t index) {
    static JSONValue kNull;
    if (type != Type::Array || index >= arrayValue.size()) {
        return kNull;
    }
    return arrayValue[index];
}

/**
 * @brief 对象键值访问 (const)
 */
const JSONValue& JSONValue::operator[](const std::string& key) const {
    static JSONValue kNull;
    if (type != Type::Object) {
        return kNull;
    }
    auto it = objectValue.find(key);
    if (it == objectValue.end()) {
        return kNull;
    }
    return it->second;
}

/**
 * @brief 对象键值访问
 */
JSONValue& JSONValue::operator[](const std::string& key) {
    static JSONValue kNull;
    if (type != Type::Object) {
        return kNull;
    }
    auto it = objectValue.find(key);
    if (it == objectValue.end()) {
        return kNull;
    }
    return it->second;
}

/**
 * @brief 检查是否包含键
 */
bool JSONValue::HasKey(const std::string& key) const {
    if (type != Type::Object) {
        return false;
    }
    return objectValue.find(key) != objectValue.end();
}

namespace {

struct ParserState {
    const std::string& text;
    size_t pos = 0;
    std::string* error = nullptr;

    char Peek() const {
        if (pos >= text.size()) {
            return '\0';
        }
        return text[pos];
    }

    bool Consume(char ch) {
        if (Peek() == ch) {
            ++pos;
            return true;
        }
        return false;
    }

    void SkipWhitespace() {
        while (pos < text.size()) {
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos;
            } else {
                break;
            }
        }
    }

    void SetError(const std::string& message) {
        if (error && error->empty()) {
            *error = message;
        }
    }
};

bool ParseHex4(ParserState& state, uint32_t& out) {
    if (state.pos + 4 > state.text.size()) {
        return false;
    }
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        char c = state.text[state.pos + i];
        value <<= 4;
        if (c >= '0' && c <= '9') {
            value |= static_cast<uint32_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value |= static_cast<uint32_t>(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            value |= static_cast<uint32_t>(10 + c - 'A');
        } else {
            return false;
        }
    }
    state.pos += 4;
    out = value;
    return true;
}

void AppendUTF8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::optional<JSONValue> ParseValue(ParserState& state);

std::optional<JSONValue> ParseString(ParserState& state) {
    if (!state.Consume('"')) {
        state.SetError("Expected '\"' at string start");
        return std::nullopt;
    }
    std::string result;
    while (state.pos < state.text.size()) {
        char c = state.text[state.pos++];
        if (c == '"') {
            return JSONValue(std::move(result));
        }
        if (c == '\\') {
            if (state.pos >= state.text.size()) {
                state.SetError("Invalid escape sequence");
                return std::nullopt;
            }
            char esc = state.text[state.pos++];
            switch (esc) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    uint32_t code = 0;
                    if (!ParseHex4(state, code)) {
                        state.SetError("Invalid unicode escape");
                        return std::nullopt;
                    }
                    if (code >= 0xD800 && code <= 0xDBFF) {
                        if (state.pos + 2 <= state.text.size() && state.text[state.pos] == '\\' && state.text[state.pos + 1] == 'u') {
                            state.pos += 2;
                            uint32_t low = 0;
                            if (!ParseHex4(state, low)) {
                                state.SetError("Invalid unicode surrogate");
                                return std::nullopt;
                            }
                            if (low >= 0xDC00 && low <= 0xDFFF) {
                                uint32_t high = code;
                                uint32_t combined = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00);
                                AppendUTF8(result, combined);
                            } else {
                                state.SetError("Invalid unicode surrogate pair");
                                return std::nullopt;
                            }
                        } else {
                            state.SetError("Missing unicode surrogate pair");
                            return std::nullopt;
                        }
                    } else {
                        AppendUTF8(result, code);
                    }
                    break;
                }
                default:
                    state.SetError("Unknown escape sequence");
                    return std::nullopt;
            }
        } else {
            if (static_cast<unsigned char>(c) < 0x20) {
                state.SetError("Invalid control character in string");
                return std::nullopt;
            }
            result.push_back(c);
        }
    }
    state.SetError("Unterminated string");
    return std::nullopt;
}

std::optional<JSONValue> ParseNumber(ParserState& state) {
    size_t start = state.pos;
    if (state.Peek() == '-') {
        ++state.pos;
    }
    if (state.Peek() == '0') {
        ++state.pos;
    } else if (std::isdigit(static_cast<unsigned char>(state.Peek()))) {
        while (std::isdigit(static_cast<unsigned char>(state.Peek()))) {
            ++state.pos;
        }
    } else {
        state.SetError("Invalid number");
        return std::nullopt;
    }
    if (state.Peek() == '.') {
        ++state.pos;
        if (!std::isdigit(static_cast<unsigned char>(state.Peek()))) {
            state.SetError("Invalid number fraction");
            return std::nullopt;
        }
        while (std::isdigit(static_cast<unsigned char>(state.Peek()))) {
            ++state.pos;
        }
    }
    if (state.Peek() == 'e' || state.Peek() == 'E') {
        ++state.pos;
        if (state.Peek() == '+' || state.Peek() == '-') {
            ++state.pos;
        }
        if (!std::isdigit(static_cast<unsigned char>(state.Peek()))) {
            state.SetError("Invalid number exponent");
            return std::nullopt;
        }
        while (std::isdigit(static_cast<unsigned char>(state.Peek()))) {
            ++state.pos;
        }
    }
    double value = 0.0;
    auto begin = state.text.data() + start;
    auto end = state.text.data() + state.pos;
    auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc()) {
        state.SetError("Failed to parse number");
        return std::nullopt;
    }
    return JSONValue(value);
}

std::optional<JSONValue> ParseArray(ParserState& state) {
    if (!state.Consume('[')) {
        state.SetError("Expected '['");
        return std::nullopt;
    }
    state.SkipWhitespace();
    std::vector<JSONValue> values;
    if (state.Consume(']')) {
        return JSONValue(std::move(values));
    }
    while (true) {
        state.SkipWhitespace();
        auto value = ParseValue(state);
        if (!value) {
            return std::nullopt;
        }
        values.push_back(std::move(*value));
        state.SkipWhitespace();
        if (state.Consume(']')) {
            break;
        }
        if (!state.Consume(',')) {
            state.SetError("Expected ',' in array");
            return std::nullopt;
        }
    }
    return JSONValue(std::move(values));
}

std::optional<JSONValue> ParseObject(ParserState& state) {
    if (!state.Consume('{')) {
        state.SetError("Expected '{'");
        return std::nullopt;
    }
    state.SkipWhitespace();
    std::unordered_map<std::string, JSONValue> values;
    if (state.Consume('}')) {
        return JSONValue(std::move(values));
    }
    while (true) {
        state.SkipWhitespace();
        auto keyValue = ParseString(state);
        if (!keyValue || !keyValue->IsString()) {
            state.SetError("Expected string key");
            return std::nullopt;
        }
        std::string key = std::move(keyValue->stringValue);
        state.SkipWhitespace();
        if (!state.Consume(':')) {
            state.SetError("Expected ':' after key");
            return std::nullopt;
        }
        state.SkipWhitespace();
        auto value = ParseValue(state);
        if (!value) {
            return std::nullopt;
        }
        values.emplace(std::move(key), std::move(*value));
        state.SkipWhitespace();
        if (state.Consume('}')) {
            break;
        }
        if (!state.Consume(',')) {
            state.SetError("Expected ',' in object");
            return std::nullopt;
        }
    }
    return JSONValue(std::move(values));
}

std::optional<JSONValue> ParseValue(ParserState& state) {
    state.SkipWhitespace();
    char c = state.Peek();
    if (c == 'n') {
        if (state.text.compare(state.pos, 4, "null") == 0) {
            state.pos += 4;
            return JSONValue();
        }
        state.SetError("Invalid token 'n'");
        return std::nullopt;
    }
    if (c == 't') {
        if (state.text.compare(state.pos, 4, "true") == 0) {
            state.pos += 4;
            return JSONValue(true);
        }
        state.SetError("Invalid token 't'");
        return std::nullopt;
    }
    if (c == 'f') {
        if (state.text.compare(state.pos, 5, "false") == 0) {
            state.pos += 5;
            return JSONValue(false);
        }
        state.SetError("Invalid token 'f'");
        return std::nullopt;
    }
    if (c == '"') {
        return ParseString(state);
    }
    if (c == '[') {
        return ParseArray(state);
    }
    if (c == '{') {
        return ParseObject(state);
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        return ParseNumber(state);
    }
    state.SetError("Unexpected character while parsing value");
    return std::nullopt;
}

} // namespace

std::optional<JSONValue> JSONParser::Parse(const std::string& jsonText) {
    using Clock = std::chrono::high_resolution_clock;
    auto t0 = Clock::now();
    m_lastError.clear();
    ParserState state{jsonText, 0, &m_lastError};
    state.SkipWhitespace();
    auto value = ParseValue(state);
    if (!value) {
        if (m_lastError.empty()) {
            m_lastError = "Failed to parse JSON";
        }
        return std::nullopt;
    }
    state.SkipWhitespace();
    if (state.pos != jsonText.size()) {
        m_lastError = "Unexpected trailing characters";
        return std::nullopt;
    }
    auto t1 = Clock::now();
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "JSON parse(ms): %.3f\n",
        std::chrono::duration<double, std::milli>(t1 - t0).count());
    OutputDebugStringA(buffer);
    return value;
}

const std::string& JSONParser::GetLastError() const {
    return m_lastError;
}

} // namespace SR
