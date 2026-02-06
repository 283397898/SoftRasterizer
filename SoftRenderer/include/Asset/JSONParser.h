#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SR {

/**
 * @brief 表示 JSON 中的一个值，可以是 null, bool, number, string, array 或 object
 */
struct JSONValue {
    /** @brief JSON 数据类型枚举 */
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    } type = Type::Null;

    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JSONValue> arrayValue;
    std::unordered_map<std::string, JSONValue> objectValue;

    JSONValue() = default;
    explicit JSONValue(bool value) : type(Type::Bool), boolValue(value) {}
    explicit JSONValue(double value) : type(Type::Number), numberValue(value) {}
    explicit JSONValue(const std::string& value) : type(Type::String), stringValue(value) {}
    explicit JSONValue(std::string&& value) : type(Type::String), stringValue(std::move(value)) {}
    explicit JSONValue(std::vector<JSONValue>&& value) : type(Type::Array), arrayValue(std::move(value)) {}
    explicit JSONValue(std::unordered_map<std::string, JSONValue>&& value) : type(Type::Object), objectValue(std::move(value)) {}

    bool IsNull() const { return type == Type::Null; }
    bool IsBool() const { return type == Type::Bool; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }
    bool IsArray() const { return type == Type::Array; }
    bool IsObject() const { return type == Type::Object; }

    /** @brief 数组索引访问 (const) */
    const JSONValue& operator[](size_t index) const;
    /** @brief 数组索引访问 */
    JSONValue& operator[](size_t index);
    /** @brief 对象键值访问 (const) */
    const JSONValue& operator[](const std::string& key) const;
    /** @brief 对象键值访问 */
    JSONValue& operator[](const std::string& key);
    /** @brief 检查对象是否包含特定键 */
    bool HasKey(const std::string& key) const;
};

/**
 * @brief JSON 解析器类
 */
class JSONParser {
public:
    /** @brief 解析 JSON 文本 */
    std::optional<JSONValue> Parse(const std::string& jsonText);
    /** @brief 获取最后一次解析错误信息 */
    const std::string& GetLastError() const;

private:
    std::string m_lastError;
};

} // namespace SR
