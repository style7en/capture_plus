#include "Json.h"

namespace json {

namespace {

struct Parser
{
    const char* p;
    const char* end;

    bool parseValue(Value& out)
    {
        skipWs();
        if (p >= end) return false;
        char c = *p;
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') { std::string s; if (!parseString(s)) return false; out = Value(s); return true; }
        if (c == 't' || c == 'f') return parseBool(out);
        if (c == 'n') return parseNull(out);
        return parseNumber(out);
    }

    bool parseObject(Value& out)
    {
        out = Value::makeObject();
        ++p; skipWs();
        if (p < end && *p == '}') { ++p; return true; }
        while (p < end)
        {
            skipWs();
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (p >= end || *p != ':') return false;
            ++p;
            Value v;
            if (!parseValue(v)) return false;
            out.set(key, std::move(v));
            skipWs();
            if (p >= end) return false;
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; return true; }
            return false;
        }
        return false;
    }

    bool parseArray(Value& out)
    {
        out = Value::makeArray();
        ++p; skipWs();
        if (p < end && *p == ']') { ++p; return true; }
        while (p < end)
        {
            Value v;
            if (!parseValue(v)) return false;
            out.push(std::move(v));
            skipWs();
            if (p >= end) return false;
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; return true; }
            return false;
        }
        return false;
    }

    bool parseString(std::string& out)
    {
        if (p >= end || *p != '"') return false;
        ++p;
        out.clear();
        while (p < end && *p != '"')
        {
            if (*p == '\\' && p + 1 < end)
            {
                ++p;
                char e = *p++;
                switch (e)
                {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u':
                    {
                        if (p + 4 > end) return false;
                        unsigned cp = 0;
                        for (int i = 0; i < 4; i++)
                        {
                            char h = *p++;
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= h - '0';
                            else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                            else return false;
                        }
                        if (cp < 0x80) out.push_back((char)cp);
                        else if (cp < 0x800) {
                            out.push_back((char)(0xC0 | (cp >> 6)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (cp >> 12)));
                            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default: out.push_back(e); break;
                }
            }
            else
            {
                out.push_back(*p++);
            }
        }
        if (p >= end) return false;
        ++p;
        return true;
    }

    bool parseNumber(Value& out)
    {
        const char* start = p;
        if (p < end && (*p == '-' || *p == '+')) ++p;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-'))
            ++p;
        if (p == start) return false;
        out = Value(std::strtod(std::string(start, p - start).c_str(), nullptr));
        return true;
    }

    bool parseBool(Value& out)
    {
        if (end - p >= 4 && strncmp(p, "true", 4) == 0) { out = Value(true); p += 4; return true; }
        if (end - p >= 5 && strncmp(p, "false", 5) == 0) { out = Value(false); p += 5; return true; }
        return false;
    }

    bool parseNull(Value& out)
    {
        if (end - p >= 4 && strncmp(p, "null", 4) == 0) { out = Value(); p += 4; return true; }
        return false;
    }

    void skipWs()
    {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    }
};

void writeString(std::string& out, const std::string& s)
{
    out.push_back('"');
    for (char c : s)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20)
                {
                    char buf[8];
                    snprintf(buf, sizeof buf, "\\u%04x", c & 0xFF);
                    out += buf;
                }
                else out.push_back(c);
        }
    }
    out.push_back('"');
}

void writeImpl(std::string& out, const Value& v, bool pretty, int depth)
{
    const char* pad = pretty ? "  " : "";
    auto indent = [&](int d) { if (pretty) for (int i = 0; i < d; i++) out += pad; };
    switch (v.type())
    {
        case Value::Null:    out += "null"; break;
        case Value::Bool:    out += v.asBool() ? "true" : "false"; break;
        case Value::Number:
        {
            double d = v.asNumber();
            long long li = (long long)d;
            if (d == (double)li) out += std::to_string(li);
            else { char buf[32]; snprintf(buf, sizeof buf, "%.17g", d); out += buf; }
            break;
        }
        case Value::String:  writeString(out, v.asString()); break;
        case Value::ObjectT:
        {
            out.push_back('{');
            bool first = true;
            for (auto& kv : v.object())
            {
                if (!first) out.push_back(',');
                first = false;
                if (pretty) { out.push_back('\n'); indent(depth + 1); }
                writeString(out, kv.first);
                out.push_back(':');
                if (pretty) out.push_back(' ');
                writeImpl(out, kv.second, pretty, depth + 1);
            }
            if (pretty && !first) { out.push_back('\n'); indent(depth); }
            out.push_back('}');
            break;
        }
        case Value::ArrayT:
        {
            out.push_back('[');
            bool first = true;
            for (auto& it : v.array())
            {
                if (!first) out.push_back(',');
                first = false;
                if (pretty) { out.push_back('\n'); indent(depth + 1); }
                writeImpl(out, it, pretty, depth + 1);
            }
            if (pretty && !first) { out.push_back('\n'); indent(depth); }
            out.push_back(']');
            break;
        }
    }
}

}

bool parse(const std::string& s, Value& out)
{
    Parser ps{ s.c_str(), s.c_str() + s.size() };
    if (!ps.parseValue(out)) return false;
    ps.skipWs();
    return ps.p == ps.end;
}

std::string write(const Value& v, bool pretty)
{
    std::string out;
    writeImpl(out, v, pretty, 0);
    return out;
}

}
