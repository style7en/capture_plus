#include "Pch.h"
#include "Json.h"
#include "Util.h"
#include "SelectionTracker.h"
#include "HotkeyManager.h"

static int g_failures = 0;
static int g_tests = 0;

static void check(bool cond, const char* expr, int line)
{
    g_tests++;
    if (!cond) { printf("FAIL line %d: %s\n", line, expr); g_failures++; }
}

#define CHECK(c) check((c), #c, __LINE__)

static void testJson()
{
    json::Value v;
    CHECK(json::parse(std::string("{\"a\":1,\"b\":\"hi\",\"c\":true,\"d\":[1,2,3],\"e\":null}"), v));
    CHECK(v.isObject());
    CHECK(v.find("a") && v.find("a")->asNumber() == 1.0);
    CHECK(v.find("b") && v.find("b")->asString() == "hi");
    CHECK(v.find("c") && v.find("c")->asBool() == true);
    const json::Value* d = v.find("d");
    CHECK(d && d->isArray() && d->size() == 3);
    CHECK(d->at(0) && d->at(0)->asNumber() == 1.0);
    CHECK(d->at(2) && d->at(2)->asNumber() == 3.0);
    CHECK(v.find("e") && v.find("e")->isNull());

    std::string out = json::write(v);
    json::Value v2;
    CHECK(json::parse(out, v2));
    CHECK(v2.find("a") && v2.find("a")->asNumber() == 1.0);

    json::Value vs;
    CHECK(json::parse(std::string("\"\\u4e2d\\u6587\""), vs));
    CHECK(vs.asString() == "\xe4\xb8\xad\xe6\x96\x87");

    json::Value empty;
    CHECK(json::parse(std::string("{}"), empty));
    CHECK(empty.isObject() && empty.size() == 0);
    CHECK(json::parse(std::string("[]"), empty));
    CHECK(empty.isArray() && empty.size() == 0);

    json::Value bad;
    CHECK(!json::parse(std::string("{bad}"), bad));
    CHECK(!json::parse(std::string(""), bad));
}

static void testUtil()
{
    CHECK(util::Trim("  hello  ") == "hello");
    CHECK(util::Trim("\t\n hi \r\n") == "hi");
    CHECK(util::Trim("") == "");
    CHECK(util::Trim("   ") == "");

    const unsigned char data[] = {'f','o','o'};
    CHECK(util::Base64Encode(data, 3) == "Zm9v");
    const unsigned char data2[] = {'f','o'};
    CHECK(util::Base64Encode(data2, 2) == "Zm8=");
    const unsigned char data3[] = {'f'};
    CHECK(util::Base64Encode(data3, 1) == "Zg==");

    std::string utf8 = "test \xe4\xb8\xad";
    std::wstring w = util::ToWide(utf8);
    CHECK(util::ToUtf8(w) == utf8);
    CHECK(util::ToWide(std::string()) == std::wstring());

    std::string host, path; bool https; int port;
    CHECK(util::SplitUrl("https://api.openai.com/v1", host, path, https, port));
    CHECK(host == "api.openai.com");
    CHECK(path == "/v1");
    CHECK(https == true);
    CHECK(port == 443);

    CHECK(util::SplitUrl("http://localhost:8080/api", host, path, https, port));
    CHECK(host == "localhost");
    CHECK(path == "/api");
    CHECK(https == false);
    CHECK(port == 8080);

    CHECK(util::SplitUrl("example.com", host, path, https, port));
    CHECK(host == "example.com");
    CHECK(path == "/");
    CHECK(https == true);

    CHECK(!util::SplitUrl("", host, path, https, port));
}

static void testSelection()
{
    NormRect r = selnorm::normalize(100, 100, -50, -30);
    CHECK(r.x == 50.0 && r.y == 70.0 && r.w == 50.0 && r.h == 30.0);

    r = selnorm::normalize(10, 20, 100, 200);
    CHECK(r.x == 10.0 && r.y == 20.0 && r.w == 100.0 && r.h == 200.0);

    CHECK(selnorm::isValid(10, 10));
    CHECK(!selnorm::isValid(7, 100));
    CHECK(!selnorm::isValid(100, 7));

    SelectionTracker t;
    CHECK(!t.isDragging() && !t.isLocked());
    t.begin(100, 100);
    CHECK(t.isDragging());
    t.update(150, 130);
    CHECK(t.rect().w == 50.0 && t.rect().h == 30.0);
    CHECK(t.end());
    CHECK(t.isLocked() && !t.isDragging());
    t.begin(0, 0);
    CHECK(t.rect().x == 100.0);

    t.reset();
    CHECK(!t.isDragging() && !t.isLocked());
    t.begin(0, 0);
    t.update(3, 3);
    CHECK(!t.end());
    CHECK(!t.isLocked());
}

static void testHotkey()
{
    UINT mods, vk;
    CHECK(HotkeyParse("Ctrl+Alt+A", mods, vk));
    CHECK(mods == (MOD_CONTROL | MOD_ALT));
    CHECK(vk == 0x41);

    CHECK(HotkeyParse("Shift+F1", mods, vk));
    CHECK(mods == MOD_SHIFT);
    CHECK(vk == 0x70);

    CHECK(HotkeyParse("Ctrl+Shift+Alt+Win+Z", mods, vk));
    CHECK(mods == (MOD_CONTROL | MOD_SHIFT | MOD_ALT | MOD_WIN));
    CHECK(vk == 0x5A);

    std::string fmt = HotkeyFormat(MOD_CONTROL | MOD_ALT, 0x41);
    CHECK(fmt == "Ctrl+Alt+A");

    CHECK(!HotkeyParse("A", mods, vk));
    CHECK(!HotkeyParse("Ctrl+", mods, vk));
    CHECK(!HotkeyParse("", mods, vk));
    CHECK(!HotkeyParse("Ctrl+XYZ", mods, vk));
}

int main()
{
    testJson();
    testUtil();
    testSelection();
    testHotkey();

    printf("\n%d tests, %d failures\n", g_tests, g_failures);
    return g_failures == 0 ? 0 : 1;
}
