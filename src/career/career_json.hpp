// Minimal dependency-free JSON value used by the career save. It is deliberately
// small: the save is one pretty-printed file and v1 has no schema migrations.

#ifndef _HPP_CAREER_JSON
#define _HPP_CAREER_JSON

#include <cctype>
#include <cstdint>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace blunted {
namespace career {

struct JsonValue {
  enum Type { e_Null, e_Bool, e_Int, e_Double, e_String, e_Array, e_Object };

  Type type = e_Null;
  bool boolValue = false;
  long long intValue = 0;
  double doubleValue = 0.0;
  std::string stringValue;
  std::vector<JsonValue> arrayValue;
  std::map<std::string, JsonValue> objectValue;

  JsonValue() {}
  explicit JsonValue(bool value) : type(e_Bool), boolValue(value) {}
  explicit JsonValue(int value) : type(e_Int), intValue(value) {}
  explicit JsonValue(long long value) : type(e_Int), intValue(value) {}
  explicit JsonValue(double value) : type(e_Double), doubleValue(value) {}
  explicit JsonValue(const std::string &value) : type(e_String), stringValue(value) {}
  explicit JsonValue(const char *value) : type(e_String), stringValue(value) {}

  static JsonValue Array() {
    JsonValue value;
    value.type = e_Array;
    return value;
  }
  static JsonValue Object() {
    JsonValue value;
    value.type = e_Object;
    return value;
  }

  bool IsObject() const { return type == e_Object; }
  bool IsArray() const { return type == e_Array; }
  bool IsNull() const { return type == e_Null; }

  void Set(const std::string &key, const JsonValue &value) {
    type = e_Object;
    objectValue[key] = value;
  }
  const JsonValue *Find(const std::string &key) const {
    std::map<std::string, JsonValue>::const_iterator it = objectValue.find(key);
    return it == objectValue.end() ? nullptr : &it->second;
  }
  long long AsInt(long long fallback = 0) const {
    if (type == e_Int) return intValue;
    if (type == e_Double) return static_cast<long long>(doubleValue);
    return fallback;
  }
  int AsIntValue(int fallback = 0) const { return static_cast<int>(AsInt(fallback)); }
  double AsDouble(double fallback = 0.0) const {
    if (type == e_Double) return doubleValue;
    if (type == e_Int) return static_cast<double>(intValue);
    return fallback;
  }
  bool AsBool(bool fallback = false) const {
    if (type == e_Bool) return boolValue;
    return fallback;
  }
  std::string AsString(const std::string &fallback = "") const {
    if (type == e_String) return stringValue;
    return fallback;
  }
  const std::vector<JsonValue> &AsArray() const { return arrayValue; }

  std::string Dump() const {
    std::ostringstream out;
    DumpInternal(out, 0);
    out << "\n";
    return out.str();
  }

  static JsonValue Parse(const std::string &text, std::string *error) {
    try {
      Parser parser(text);
      JsonValue value = parser.ParseValue();
      parser.SkipWhitespace();
      if (!parser.AtEnd()) throw std::runtime_error("trailing characters");
      return value;
    } catch (const std::exception &exception) {
      if (error) *error = exception.what();
      return JsonValue();
    }
  }

  private:
    static void Escape(std::ostringstream &out, const std::string &value) {
      out << '"';
      for (size_t i = 0; i < value.size(); i++) {
        char c = value[i];
        switch (c) {
          case '"': out << "\\\""; break;
          case '\\': out << "\\\\"; break;
          case '\n': out << "\\n"; break;
          case '\r': out << "\\r"; break;
          case '\t': out << "\\t"; break;
          default:
            if (static_cast<unsigned char>(c) < 0x20) {
              out << "\\u" << std::hex << (static_cast<int>(c) & 0xff) << std::dec;
            } else {
              out << c;
            }
        }
      }
      out << '"';
    }

    static void Indent(std::ostringstream &out, int depth) {
      for (int i = 0; i < depth; i++) out << "  ";
    }

    void DumpInternal(std::ostringstream &out, int depth) const {
      switch (type) {
        case e_Null: out << "null"; break;
        case e_Bool: out << (boolValue ? "true" : "false"); break;
        case e_Int: out << intValue; break;
        case e_Double: {
          std::ostringstream number;
          number.precision(10);
          number << doubleValue;
          out << number.str();
          break;
        }
        case e_String: Escape(out, stringValue); break;
        case e_Array: {
          if (arrayValue.empty()) {
            out << "[]";
            break;
          }
          out << "[\n";
          for (size_t i = 0; i < arrayValue.size(); i++) {
            Indent(out, depth + 1);
            arrayValue[i].DumpInternal(out, depth + 1);
            if (i + 1 < arrayValue.size()) out << ",";
            out << "\n";
          }
          Indent(out, depth);
          out << "]";
          break;
        }
        case e_Object: {
          if (objectValue.empty()) {
            out << "{}";
            break;
          }
          out << "{\n";
          size_t index = 0;
          for (std::map<std::string, JsonValue>::const_iterator it = objectValue.begin();
               it != objectValue.end(); ++it, ++index) {
            Indent(out, depth + 1);
            Escape(out, it->first);
            out << ": ";
            it->second.DumpInternal(out, depth + 1);
            if (index + 1 < objectValue.size()) out << ",";
            out << "\n";
          }
          Indent(out, depth);
          out << "}";
          break;
        }
      }
    }

    class Parser {
      public:
        explicit Parser(const std::string &text) : text(text), position(0) {}

        bool AtEnd() const { return position >= text.size(); }

        void SkipWhitespace() {
          while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) position++;
        }

        JsonValue ParseValue() {
          SkipWhitespace();
          if (AtEnd()) throw std::runtime_error("unexpected end of input");
          char c = text[position];
          if (c == '{') return ParseObject();
          if (c == '[') return ParseArray();
          if (c == '"') return JsonValue(ParseString());
          if (c == 't') return ParseLiteral("true", JsonValue(true));
          if (c == 'f') return ParseLiteral("false", JsonValue(false));
          if (c == 'n') return ParseLiteral("null", JsonValue());
          return ParseNumber();
        }

      private:
        JsonValue ParseObject() {
          JsonValue result = JsonValue::Object();
          position++;
          SkipWhitespace();
          if (position < text.size() && text[position] == '}') {
            position++;
            return result;
          }
          while (true) {
            SkipWhitespace();
            if (position >= text.size() || text[position] != '"') throw std::runtime_error("expected object key");
            std::string key = ParseString();
            SkipWhitespace();
            if (position >= text.size() || text[position] != ':') throw std::runtime_error("expected ':'");
            position++;
            result.objectValue[key] = ParseValue();
            SkipWhitespace();
            if (position >= text.size()) throw std::runtime_error("unterminated object");
            if (text[position] == ',') {
              position++;
              continue;
            }
            if (text[position] == '}') {
              position++;
              return result;
            }
            throw std::runtime_error("expected ',' or '}'");
          }
        }

        JsonValue ParseArray() {
          JsonValue result = JsonValue::Array();
          position++;
          SkipWhitespace();
          if (position < text.size() && text[position] == ']') {
            position++;
            return result;
          }
          while (true) {
            result.arrayValue.push_back(ParseValue());
            SkipWhitespace();
            if (position >= text.size()) throw std::runtime_error("unterminated array");
            if (text[position] == ',') {
              position++;
              continue;
            }
            if (text[position] == ']') {
              position++;
              return result;
            }
            throw std::runtime_error("expected ',' or ']'");
          }
        }

        std::string ParseString() {
          if (text[position] != '"') throw std::runtime_error("expected string");
          position++;
          std::string result;
          while (position < text.size()) {
            char c = text[position++];
            if (c == '"') return result;
            if (c != '\\') {
              result.push_back(c);
              continue;
            }
            if (position >= text.size()) throw std::runtime_error("unterminated escape");
            char escape = text[position++];
            switch (escape) {
              case '"': result.push_back('"'); break;
              case '\\': result.push_back('\\'); break;
              case '/': result.push_back('/'); break;
              case 'b': result.push_back('\b'); break;
              case 'f': result.push_back('\f'); break;
              case 'n': result.push_back('\n'); break;
              case 'r': result.push_back('\r'); break;
              case 't': result.push_back('\t'); break;
              case 'u': {
                if (position + 4 > text.size()) throw std::runtime_error("bad unicode escape");
                int value = 0;
                for (int i = 0; i < 4; i++) {
                  char hex = text[position++];
                  value <<= 4;
                  if (hex >= '0' && hex <= '9') value += hex - '0';
                  else if (hex >= 'a' && hex <= 'f') value += hex - 'a' + 10;
                  else if (hex >= 'A' && hex <= 'F') value += hex - 'A' + 10;
                  else throw std::runtime_error("bad unicode escape");
                }
                result.push_back(static_cast<char>(value & 0xff));
                break;
              }
              default:
                throw std::runtime_error("bad escape");
            }
          }
          throw std::runtime_error("unterminated string");
        }

        JsonValue ParseNumber() {
          size_t start = position;
          if (position < text.size() && (text[position] == '-' || text[position] == '+')) position++;
          bool isDouble = false;
          while (position < text.size()) {
            char c = text[position];
            if (std::isdigit(static_cast<unsigned char>(c))) {
              position++;
            } else if (c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+') {
              isDouble = true;
              position++;
            } else {
              break;
            }
          }
          if (start == position) throw std::runtime_error("expected number");
          std::string token = text.substr(start, position - start);
          if (isDouble) return JsonValue(std::stod(token));
          return JsonValue(static_cast<long long>(std::stoll(token)));
        }

        JsonValue ParseLiteral(const char *literal, const JsonValue &value) {
          size_t length = std::string(literal).size();
          if (text.compare(position, length, literal) != 0) throw std::runtime_error("invalid literal");
          position += length;
          return value;
        }

        const std::string &text;
        size_t position;
    };
};

} // namespace career
} // namespace blunted

#endif
