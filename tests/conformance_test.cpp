#include "sedmat/sedmat.hpp"

#include <cassert>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {

// Minimal zero-dependency JSON parser for loading testdata/conformance/v3.6.json.
struct Json;
using JsonArr = std::vector<Json>;
using JsonObj = std::map<std::string, Json>;

struct Json {
    std::variant<std::nullptr_t, bool, long long, std::string, JsonArr, JsonObj> val;

    const JsonObj& obj() const { return std::get<JsonObj>(val); }
    const JsonArr& arr() const { return std::get<JsonArr>(val); }
    const std::string& str() const { return std::get<std::string>(val); }
    bool b() const { return std::get<bool>(val); }
    long long num() const { return std::get<long long>(val); }
    bool has(const std::string& k) const { return obj().count(k) > 0; }
    const Json& at(const std::string& k) const { return obj().at(k); }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s) {}
    Json parse() { return parse_val(); }

private:
    const std::string& s_;
    std::size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) pos_++;
    }
    Json parse_val() {
        skip_ws();
        char c = s_[pos_];
        if (c == '{') return parse_obj();
        if (c == '[') return parse_arr();
        if (c == '"') return Json{parse_str()};
        if (c == 't') { pos_ += 4; return Json{true}; }
        if (c == 'f') { pos_ += 5; return Json{false}; }
        if (c == 'n') { pos_ += 4; return Json{nullptr}; }
        return parse_num();
    }
    Json parse_obj() {
        pos_++;
        JsonObj m;
        while (true) {
            skip_ws();
            if (s_[pos_] == '}') { pos_++; break; }
            std::string k = parse_str();
            skip_ws();
            assert(s_[pos_] == ':');
            pos_++;
            m[k] = parse_val();
            skip_ws();
            if (s_[pos_] == ',') pos_++;
        }
        return Json{std::move(m)};
    }
    Json parse_arr() {
        pos_++;
        JsonArr a;
        while (true) {
            skip_ws();
            if (s_[pos_] == ']') { pos_++; break; }
            a.push_back(parse_val());
            skip_ws();
            if (s_[pos_] == ',') pos_++;
        }
        return Json{std::move(a)};
    }
    std::string parse_str() {
        assert(s_[pos_] == '"');
        pos_++;
        std::string out;
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') break;
            if (c == '\\') {
                char e = s_[pos_++];
                if (e == '"') out.push_back('"');
                else if (e == '\\') out.push_back('\\');
                else if (e == '/') out.push_back('/');
                else if (e == 'n') out.push_back('\n');
                else if (e == 'r') out.push_back('\r');
                else if (e == 't') out.push_back('\t');
                else out.push_back(e);
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
    Json parse_num() {
        std::size_t start = pos_;
        if (s_[pos_] == '-') pos_++;
        while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) pos_++;
        return Json{std::stoll(s_.substr(start, pos_ - start))};
    }
};

std::string read_file(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    assert(ifs.good());
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

void test_conformance_v36_json() {
    auto root = JsonParser(read_file("testdata/conformance/v3.6.json")).parse();
    int parse_count = 0;
    for (const auto& tc : root.at("parse_vectors").arr()) {
        const std::string& name = tc.at("name").str();
        const std::string& input = tc.at("input").str();
        bool want_err = tc.has("want_error") && tc.at("want_error").b();

        if (want_err) {
            bool threw = false;
            try {
                (void)sedmat::enrich(sedmat::parse(input));
            } catch (const sedmat::SedmatError&) {
                threw = true;
            }
            if (!threw) {
                std::cerr << "FAIL parse/" << name << ": expected error\n";
                std::exit(1);
            }
            parse_count++;
            continue;
        }

        auto prog = sedmat::enrich(sedmat::parse(input));
        assert(prog.expressions.size() == 1);
        const auto& expr = prog.expressions[0];

        if (tc.has("command")) assert(sedmat::command_name(expr.command) == tc.at("command").str());
        if (tc.has("pattern")) assert(expr.pattern == tc.at("pattern").str());
        if (tc.has("replacement")) assert(expr.replacement == tc.at("replacement").str());
        if (tc.has("global")) assert(expr.global == tc.at("global").b());
        if (tc.has("nth")) assert(expr.nth_match == static_cast<std::size_t>(tc.at("nth").num()));
        if (tc.has("address")) {
            assert(expr.address.has_value());
            assert(expr.address->start == tc.at("address").at("start").num());
            assert(expr.address->end == tc.at("address").at("end").num());
            assert(expr.address->has_range == tc.at("address").at("has_range").b());
        }
        if (tc.has("expect_span_count")) {
            assert(expr.brace_spans.size() == static_cast<std::size_t>(tc.at("expect_span_count").num()));
        }
        if (tc.has("expect_table_create")) {
            assert(expr.table_create.has_value());
            assert(expr.table_create->rows == static_cast<std::size_t>(tc.at("expect_table_create").at("rows").num()));
            assert(expr.table_create->columns == static_cast<std::size_t>(tc.at("expect_table_create").at("columns").num()));
        }
        if (tc.has("expect_brace")) {
            assert(expr.brace.has_value());
            const auto& b = *expr.brace;
            const auto& eb = tc.at("expect_brace");
            if (eb.has("bold")) assert(b.bold == eb.at("bold").b());
            if (eb.has("color")) assert(b.color && upper(*b.color) == upper(eb.at("color").str()));
            if (eb.has("reset")) assert(b.reset == eb.at("reset").b());
            if (eb.has("no_reset")) assert(b.no_reset == eb.at("no_reset").b());
            if (eb.has("has_break")) assert(b.has_break == eb.at("has_break").b());
            if (eb.has("break")) assert(b.break_kind == eb.at("break").str());
            if (eb.has("cols")) assert(b.cols == eb.at("cols").num());
            if (eb.has("check_set") && eb.at("check_set").b()) assert(b.check.has_value());
            if (eb.has("toc")) assert(b.toc == eb.at("toc").num());
            if (eb.has("comment")) assert(b.comment == eb.at("comment").str());
            if (eb.has("bookmark")) assert(b.bookmark == eb.at("bookmark").str());
            if (eb.has("url")) assert(b.url == eb.at("url").str());
            if (eb.has("img_ref")) assert(b.img_ref == eb.at("img_ref").str());
        }
        parse_count++;
    }

    int exec_count = 0;
    for (const auto& tc : root.at("execution_vectors").arr()) {
        const std::string& name = tc.at("name").str();
        std::vector<std::string> paras;
        for (const auto& p : tc.at("paragraphs").arr()) paras.push_back(p.str());
        const std::string& script = tc.at("script").str();
        bool want_err = tc.has("want_error") && tc.at("want_error").b();

        sedmat::MemoryDocument doc(paras);
        sedmat::Executor exec(doc);
        if (want_err) {
            bool threw = false;
            try {
                (void)exec.execute_string(script);
            } catch (const sedmat::SedmatError&) {
                threw = true;
            }
            if (!threw) {
                std::cerr << "FAIL exec/" << name << ": expected error\n";
                std::exit(1);
            }
            exec_count++;
            continue;
        }
        (void)exec.execute_string(script);
        std::vector<std::string> want;
        for (const auto& p : tc.at("expected_paragraphs").arr()) want.push_back(p.str());
        auto got = doc.paragraph_texts();
        if (got != want) {
            std::cerr << "FAIL exec/" << name << "\n";
            std::exit(1);
        }
        exec_count++;
    }
    std::cout << "PASS conformance v3.6: " << parse_count << " parse + " << exec_count << " exec vectors\n";
}

void test_ported_sed_fixtures() {
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator("testdata/fixtures")) {
        if (entry.path().extension() == ".sed") {
            auto prog = sedmat::parse_program(read_file(entry.path()));
            assert(!prog.expressions.empty());
            count++;
        }
    }
    assert(count >= 9);
    std::cout << "PASS ported fixtures: " << count << " .sed files\n";
}

void test_v37_container_scopes_and_introspection() {
    sedmat::MemoryDocument doc({"Slide 1 Title", "Draft subtitle"});
    doc.tabs[0].id = "slide-1";
    doc.tabs[0].title = "Intro";
    doc.add_tab("slide-2", "Metrics", {"Latency is 500ms", "Keep line"});
    doc.add_tab("slide-3", "Q4 Budget", {"Total in USD"});

    sedmat::Executor exec(doc);
    auto dry = exec.execute_string("@2:s/500ms/120ms/", sedmat::ExecuteOptions{true, ""});
    assert(dry.dry_run && dry.total_matches == 1);
    assert(doc.paragraph_texts("slide-2")[0] == "Latency is 500ms");

    (void)exec.execute_string(R"(
@1:{z=#0F172A transition=fade}
@2:s/500ms/{b c=green}120ms/g
@2:{notes="Highlight 120ms WebTransport benchmark"}
@"Q4 Budget":s/USD/EUR/g
@$a/Next Steps/
@4{hide}
)");

    assert(doc.tabs[0].background == "#0F172A");
    assert(doc.tabs[0].transition == "fade");
    assert((doc.paragraph_texts("slide-2") == std::vector<std::string>{"Latency is 120ms", "Keep line"}));
    assert(doc.tabs[1].notes == "Highlight 120ms WebTransport benchmark");
    assert((doc.paragraph_texts("Q4 Budget") == std::vector<std::string>{"Total in EUR"}));
    assert(doc.tabs.size() == 4);
    assert(doc.tabs[3].hidden);

    auto proj = doc.project();
    const auto& seg = sedmat::select_segment(proj, "slide-2");
    assert(sedmat::format_structure(seg).find("[1] paragraph (NORMAL_TEXT): \"Latency is 120ms\"") != std::string::npos);
    assert(sedmat::format_numbered(seg).find("[2] Keep line") != std::string::npos);
    std::cout << "PASS v3.7 container scopes, dry-run, and introspection\n";
}

void test_intellisense() {
    auto diag = sedmat::extract_diagnostic(
        "SEDMAT error at offset 9: Unclosed brace expression \xE2\x80\x94 add a closing '}'");
    assert(!diag.ok);
    assert(diag.error_offset.has_value() && *diag.error_offset == 9);
    assert(diag.error_length == 1);
    assert(diag.summary == "Unclosed brace expression");
    assert(diag.hint.has_value() && *diag.hint == "add a closing '}'");

    auto doc = sedmat::get_completions("s/foo/{", 7);
    bool has_bold = false;
    bool has_transition = false;
    for (const auto& s : doc.suggestions) {
        if (s.label == "b") has_bold = true;
        if (s.label == "transition=") has_transition = true;
    }
    assert(has_bold);
    assert(!has_transition);

    sedmat::ContextInfo slides_ctx;
    slides_ctx.mode = sedmat::Profile::Slides;
    slides_ctx.active_container = 3;
    auto slides = sedmat::get_completions("s/foo/{tr", 9, slides_ctx);
    bool slides_has_transition = false;
    for (const auto& s : slides.suggestions) {
        if (s.label == "transition=") slides_has_transition = true;
    }
    assert(slides_has_transition);

    auto at_res = sedmat::get_completions("@", 1, slides_ctx);
    assert(at_res.ghost_suffix.has_value() && *at_res.ghost_suffix == "3:");

    auto colors = sedmat::get_completions("s/foo/{c=", 9);
    std::string green_swatch;
    std::string orange_swatch;
    for (const auto& s : colors.suggestions) {
        if (s.label == "green" && s.swatch) green_swatch = *s.swatch;
        if (s.label == "orange" && s.swatch) orange_swatch = *s.swatch;
    }
    assert(green_swatch == "#00FF00");
    assert(orange_swatch == "#FF8C00");

    auto chained = sedmat::get_completions("s/a/b/g; s/c/{c=re", 18);
    assert(!chained.suggestions.empty() && chained.suggestions[0].label == "red");
    assert(chained.ghost_suffix.has_value() && *chained.ghost_suffix == "d");

    sedmat::IntelliSenseExtensions ext;
    ext.flags.push_back({"brand=", "brand=", "Custom brand flag", {}});
    ext.values["brand"] = {{"acme", "Acme primary", "#123456"}};
    sedmat::ContextInfo ext_ctx;
    ext_ctx.extensions = ext;
    auto ext_val = sedmat::get_completions("s/x/{brand=", 11, ext_ctx);
    assert(!ext_val.suggestions.empty() && ext_val.suggestions[0].label == "acme");
    assert(ext_val.suggestions[0].swatch == "#123456");

    std::string expr = "@2:title:s/foo/{b c=red}bar/gi";
    auto tokens = sedmat::tokenize(expr);
    std::string rebuilt;
    bool has_scope = false, has_brace = false, has_text = false, has_flag = false;
    for (const auto& t : tokens) {
        rebuilt += t.text;
        if (t.kind == sedmat::TokenKind::Scope && t.text == "@2:title:") has_scope = true;
        if (t.kind == sedmat::TokenKind::Brace && t.text == "{b c=red}") has_brace = true;
        if (t.kind == sedmat::TokenKind::Text && t.text == "bar") has_text = true;
        if (t.kind == sedmat::TokenKind::Flag && t.text == "gi") has_flag = true;
    }
    assert(rebuilt == expr);
    assert(has_scope && has_brace && has_text && has_flag);

    std::string inc = "# comment\n@1:s/unclosed/{b c=";
    auto inc_tokens = sedmat::tokenize(inc);
    std::string inc_rebuilt;
    for (const auto& t : inc_tokens) inc_rebuilt += t.text;
    assert(inc_rebuilt == inc);
    assert(!inc_tokens.empty() && inc_tokens[0].kind == sedmat::TokenKind::Comment);

    std::cout << "PASS intellisense completions, diagnostics, and tokenizer\n";
}

} // namespace

int main() {
    test_conformance_v36_json();
    test_ported_sed_fixtures();
    test_v37_container_scopes_and_introspection();
    test_intellisense();
    return 0;
}
