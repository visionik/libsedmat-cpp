#include "sedmat/sedmat.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace sedmat {

namespace {

std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string to_upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool iequals(const std::string& a, const std::string& b) {
    return to_lower(a) == to_lower(b);
}

std::string unquote(const std::string& s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string replace_all(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string strip_trailing_newlines(std::string s) {
    while (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

std::optional<std::string> bool_flag_name(const std::string& flag) {
    if (flag == "b" || flag == "bold") return "bold";
    if (flag == "i" || flag == "italic") return "italic";
    if (flag == "_" || flag == "underline") return "underline";
    if (flag == "-" || flag == "strike") return "strike";
    if (flag == "#" || flag == "code") return "code";
    if (flag == "^" || flag == "sup") return "sup";
    if (flag == "," || flag == "sub") return "sub";
    if (flag == "w" || flag == "smallcaps") return "small_caps";
    return std::nullopt;
}

void set_bool_flag(BraceExpression& expr, const std::string& name, bool val) {
    if (name == "bold") expr.bold = val;
    else if (name == "italic") expr.italic = val;
    else if (name == "underline") expr.underline = val;
    else if (name == "strike") expr.strike = val;
    else if (name == "code") expr.code = val;
    else if (name == "sup") expr.sup = val;
    else if (name == "sub") expr.sub = val;
    else if (name == "small_caps") expr.small_caps = val;
}

std::vector<std::string> tokenize_brace_content(const std::string& s) {
    std::vector<std::string> tokens;
    std::string cur;
    bool in_quote = false;
    char quote_char = '\0';
    for (char ch : s) {
        if (in_quote) {
            cur.push_back(ch);
            if (ch == quote_char) in_quote = false;
        } else if (ch == '"' || ch == '\'') {
            in_quote = true;
            quote_char = ch;
            cur.push_back(ch);
        } else if (ch == ' ' || ch == '\t') {
            if (!cur.empty()) {
                tokens.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) tokens.push_back(std::move(cur));
    return tokens;
}

std::optional<std::size_t> find_matching_brace(const std::string& s, std::size_t open_idx) {
    if (open_idx + 3 <= s.size() && s.compare(open_idx, 3, "{\"=") == 0) {
        auto pos = s.find('}', open_idx + 3);
        return pos == std::string::npos ? std::nullopt : std::optional<std::size_t>(pos);
    }
    int depth = 0;
    bool in_quote = false;
    char quote_char = '\0';
    for (std::size_t i = open_idx; i < s.size(); ++i) {
        char ch = s[i];
        if (in_quote) {
            if (ch == quote_char) in_quote = false;
            continue;
        }
        if (ch == '"' || ch == '\'') {
            in_quote = true;
            quote_char = ch;
        } else if (ch == '{') {
            depth++;
        } else if (ch == '}') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return std::nullopt;
}

bool looks_like_brace_expression(const std::string& content) {
    std::string trimmed = trim(content);
    if (trimmed.empty()) return false;
    auto sp = trimmed.find_first_of(" \t");
    std::string first = sp == std::string::npos ? trimmed : trimmed.substr(0, sp);

    static const std::unordered_set<std::string> bare = {
        "0", "!0", "+", "check", "toc", "dup", "hide", "f", "s", "c", "z", "h", "p", "t"
    };
    if (bare.count(first) || bool_flag_name(first).has_value()) return true;
    if (first.size() > 1 && first[0] == '!' && bool_flag_name(first.substr(1)).has_value()) return true;
    if (trimmed.rfind("+=", 0) == 0 || trimmed.rfind("\"=", 0) == 0 || trimmed.rfind("@=", 0) == 0) return true;

    static const std::vector<std::string> keys = {
        "t=", "text=", "c=", "color=", "z=", "bg=", "f=", "font=", "s=", "size=",
        "u=", "url=", "h=", "heading=", "l=", "leading=", "a=", "align=", "o=", "opacity=",
        "n=", "indent=", "k=", "kerning=", "x=", "width=", "y=", "height=", "p=", "spacing=",
        "e=", "effect=", "cols=", "check=", "toc=", "img=", "T=", "@=", "\"=",
        "layout=", "zone=", "shape=", "fill=", "stroke=", "notes=", "transition=", "move=",
        "b=", "bold=", "i=", "italic=", "_=", "-=", "#=", "^=", ",=", "w="
    };
    for (const auto& k : keys) {
        if (trimmed.find(k) != std::string::npos) return true;
    }
    return false;
}

bool brace_has_any_format(const BraceExpression& e) {
    return e.bold.has_value() || e.italic.has_value() || e.underline.has_value() ||
           e.strike.has_value() || e.code.has_value() || e.sup.has_value() ||
           e.sub.has_value() || e.small_caps.has_value() || !e.inline_spans.empty() ||
           e.text.has_value() || e.color.has_value() || e.bg.has_value() ||
           e.font.has_value() || e.size.has_value() || e.url.has_value() ||
           e.heading.has_value() || e.leading.has_value() || e.align.has_value() ||
           e.opacity.has_value() || e.indent.has_value() || e.kerning.has_value() ||
           e.width.has_value() || e.height.has_value() || e.spacing_set ||
           e.effect.has_value() || e.cols.has_value() || e.reset || e.no_reset ||
           e.has_break || e.comment.has_value() || e.bookmark.has_value() ||
           e.check.has_value() || e.has_toc || e.img_ref.has_value() ||
           e.table_ref.has_value() || e.layout.has_value() || e.zone.has_value() ||
           e.shape.has_value() || e.fill.has_value() || e.stroke.has_value() ||
           e.notes.has_value() || e.transition.has_value() || e.move_to.has_value() ||
           e.dup || e.hide;
}

BraceExpression merge_brace_spans(const std::vector<BraceSpan>& spans) {
    BraceExpression m;
    for (const auto& span : spans) {
        if (!span.is_global) continue;
        const auto& s = span.expr;
        if (s.bold) m.bold = s.bold;
        if (s.italic) m.italic = s.italic;
        if (s.underline) m.underline = s.underline;
        if (s.strike) m.strike = s.strike;
        if (s.code) m.code = s.code;
        if (s.sup) m.sup = s.sup;
        if (s.sub) m.sub = s.sub;
        if (s.small_caps) m.small_caps = s.small_caps;
        if (s.color) m.color = s.color;
        if (s.bg) m.bg = s.bg;
        if (s.font) m.font = s.font;
        if (s.size) m.size = s.size;
        if (s.url) m.url = s.url;
        if (s.heading) m.heading = s.heading;
        if (s.align) m.align = s.align;
        if (s.leading) m.leading = s.leading;
        if (s.indent) m.indent = s.indent;
        if (s.cols) m.cols = s.cols;
        if (s.check) m.check = s.check;
        if (s.has_toc) { m.has_toc = true; m.toc = s.toc; }
        if (s.comment) m.comment = s.comment;
        if (s.bookmark) m.bookmark = s.bookmark;
        if (s.img_ref) m.img_ref = s.img_ref;
        if (s.table_ref) m.table_ref = s.table_ref;
        if (s.reset) m.reset = true;
        if (s.no_reset) m.no_reset = true;
        if (s.has_break) { m.has_break = true; m.break_kind = s.break_kind; }
    }
    return m;
}

std::pair<std::optional<Address>, std::string> parse_address_internal(const std::string& raw) {
    if (raw.empty() || (raw[0] != '$' && !std::isdigit(static_cast<unsigned char>(raw[0])))) {
        return {std::nullopt, raw};
    }
    std::size_t i = 0;
    int start = 0;
    if (raw[i] == '$') {
        start = -1;
        i++;
    } else {
        std::size_t s_idx = i;
        while (i < raw.size() && std::isdigit(static_cast<unsigned char>(raw[i]))) i++;
        start = std::stoi(raw.substr(s_idx, i - s_idx));
        if (start < 1) throw SedmatError("address must be >= 1");
    }

    bool has_range = false;
    int end = 0;
    if (i < raw.size() && raw[i] == ',') {
        if (start == -1) throw SedmatError("'$' cannot be used as range start");
        has_range = true;
        i++;
        if (i >= raw.size()) throw SedmatError("missing range end after ','");
        if (raw[i] == '$') {
            end = -1;
            i++;
        } else if (std::isdigit(static_cast<unsigned char>(raw[i]))) {
            std::size_t e_idx = i;
            while (i < raw.size() && std::isdigit(static_cast<unsigned char>(raw[i]))) i++;
            end = std::stoi(raw.substr(e_idx, i - e_idx));
            if (end < 1) throw SedmatError("address range end must be >= 1");
            if (end < start) throw SedmatError("address range is reversed");
        } else {
            throw SedmatError("invalid range end");
        }
    }

    std::string rest = raw.substr(i);
    if (rest.empty()) throw SedmatError("address without command");
    return {Address{start, end, has_range}, rest};
}

std::pair<std::optional<ContainerScope>, std::string> parse_container_scope_internal(const std::string& raw) {
    if (raw.empty() || raw[0] != '@') return {std::nullopt, raw};
    std::string rest = raw.substr(1);
    if (rest.empty()) throw SedmatError("empty container scope '@'");

    for (const std::string& reg : {"header:", "footer:"}) {
        if (rest.size() > reg.size() && to_lower(rest.substr(0, reg.size())) == reg) {
            ContainerScope scope;
            scope.region = reg.substr(0, reg.size() - 1);
            scope.container_op = false;
            return {scope, rest.substr(reg.size())};
        }
    }

    ContainerScope scope;
    for (const std::string& t : {"slide=", "sheet=", "tab=", "page=", "section="}) {
        if (rest.rfind(t, 0) == 0) {
            scope.container_type = t.substr(0, t.size() - 1);
            rest = rest.substr(t.size());
            break;
        }
    }

    if (!rest.empty() && rest[0] == '*') {
        scope.all = true;
        rest = rest.substr(1);
    } else if (!rest.empty() && rest[0] == '"') {
        auto end_q = rest.find('"', 1);
        if (end_q == std::string::npos) throw SedmatError("unterminated quoted container name");
        scope.name = rest.substr(1, end_q - 1);
        rest = rest.substr(end_q + 1);
    } else if (!rest.empty() && (rest[0] == '$' || std::isdigit(static_cast<unsigned char>(rest[0])))) {
        std::size_t idx = 0;
        if (rest[idx] == '$') idx++;
        else while (idx < rest.size() && std::isdigit(static_cast<unsigned char>(rest[idx]))) idx++;
        if (idx < rest.size() && rest[idx] == ',') {
            idx++;
            if (idx < rest.size() && rest[idx] == '$') idx++;
            else while (idx < rest.size() && std::isdigit(static_cast<unsigned char>(rest[idx]))) idx++;
        }
        auto [addr, _] = parse_address_internal(rest.substr(0, idx) + "d");
        scope.address = addr;
        rest = rest.substr(idx);
    } else {
        std::size_t end = 0;
        while (end < rest.size() && (std::isalnum(static_cast<unsigned char>(rest[end])) || rest[end] == '-' || rest[end] == '_')) end++;
        if (end == 0) throw SedmatError("invalid container target");
        scope.name = rest.substr(0, end);
        rest = rest.substr(end);
    }

    if (!rest.empty() && rest[0] == ':') {
        std::string body = rest.substr(1);
        auto c_idx = body.find(':');
        if (c_idx != std::string::npos) {
            std::string cand = to_lower(body.substr(0, c_idx));
            if (cand == "title" || cand == "sub" || cand == "subtitle" || cand == "body" ||
                cand == "notes" || cand == "header" || cand == "footer" ||
                cand.rfind("shape=", 0) == 0) {
                scope.region = cand;
                body = body.substr(c_idx + 1);
            }
        }
        if (body.empty()) throw SedmatError("container scope without command");
        scope.container_op = false;
        return {scope, body};
    }

    if (!rest.empty() && (rest[0] == 'd' || rest[0] == 'a' || rest[0] == 'i' || rest[0] == '{')) {
        scope.container_op = true;
        return {scope, rest};
    }

    throw SedmatError("invalid container scope");
}

std::vector<std::string> split_by_delimiter(const std::string& value, char delim) {
    std::vector<std::string> parts;
    std::string cur;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            if (value[i + 1] == delim) {
                cur.push_back(delim);
            } else {
                cur.push_back('\\');
                cur.push_back(value[i + 1]);
            }
            i++;
            continue;
        }
        if (value[i] == delim) {
            parts.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(value[i]);
        }
    }
    parts.push_back(std::move(cur));
    return parts;
}

std::string apply_regex_flags(const std::string& pattern, const std::string& flags) {
    std::string res = pattern;
    if (flags.find('i') != std::string::npos && res.rfind("(?i)", 0) != 0) res = "(?i)" + res;
    if (flags.find('m') != std::string::npos && res.rfind("(?m)", 0) != 0) res = "(?m)" + res;
    return res;
}

std::size_t nth_flag(const std::string& flags) {
    std::string digits;
    for (char c : flags) {
        if (std::isdigit(static_cast<unsigned char>(c))) digits.push_back(c);
    }
    return digits.empty() ? 0 : static_cast<std::size_t>(std::stoul(digits));
}

std::regex compile_sed_regex(const std::string& pattern) {
    auto opts = std::regex::ECMAScript;
    std::string body = pattern;
    while (true) {
        if (body.rfind("(?i)", 0) == 0) {
            opts |= std::regex::icase;
            body = body.substr(4);
        } else if (body.rfind("(?m)", 0) == 0) {
            opts |= std::regex::multiline;
            body = body.substr(4);
        } else if (body.rfind("(?im)", 0) == 0 || body.rfind("(?mi)", 0) == 0) {
            opts |= std::regex::icase | std::regex::multiline;
            body = body.substr(5);
        } else {
            break;
        }
    }
    return std::regex(body, opts);
}

std::string expand_replacement(const std::string& templ, const std::smatch& m) {
    std::string out;
    for (std::size_t i = 0; i < templ.size(); ++i) {
        char ch = templ[i];
        if (ch == '\\' && i + 1 < templ.size()) {
            char nxt = templ[i + 1];
            if (nxt == '&') { out.push_back('&'); i++; continue; }
            if (nxt == 'n') { out.push_back('\n'); i++; continue; }
            if (nxt >= '1' && nxt <= '9') {
                std::size_t idx = static_cast<std::size_t>(nxt - '0');
                if (idx < m.size()) out += m[idx].str();
                i++;
                continue;
            }
        }
        if (ch == '&') {
            out += m[0].str();
            continue;
        }
        if (ch == '$' && i + 1 < templ.size()) {
            char nxt = templ[i + 1];
            if (nxt == '$') { out.push_back('$'); i++; continue; }
            if (nxt >= '0' && nxt <= '9') {
                std::size_t idx = static_cast<std::size_t>(nxt - '0');
                if (idx < m.size()) out += m[idx].str();
                i++;
                continue;
            }
        }
        out.push_back(ch);
    }
    return out;
}

DocumentSegment project_tab(const MemoryTab& tab) {
    DocumentSegment seg;
    seg.tab_id = tab.id;
    seg.title = tab.title;
    seg.body_start_index = 1;
    std::size_t cursor = 1;
    for (std::size_t i = 0; i < tab.paragraphs.size(); ++i) {
        std::string full = tab.paragraphs[i].text + "\n";
        std::size_t start = cursor;
        std::size_t end = cursor + full.size();
        seg.paragraphs.push_back(DocumentParagraph{full, start, end, tab.paragraphs[i].named_style});
        seg.blocks.push_back(DocumentBlock{BlockKind::Paragraph, start, end, static_cast<int>(i)});
        cursor = end;
    }
    seg.body_end_index = cursor;
    return seg;
}

} // namespace

std::string command_name(Command cmd) {
    switch (cmd) {
        case Command::Substitute: return "s";
        case Command::Delete: return "d";
        case Command::Append: return "a";
        case Command::Insert: return "i";
        case Command::Transliterate: return "y";
        case Command::Brace: return "brace";
    }
    return "";
}

std::string resolve_color(const std::string& value) {
    static const std::unordered_map<std::string, std::string> colors = {
        {"red", "#FF0000"}, {"green", "#008000"}, {"blue", "#0000FF"},
        {"yellow", "#FFFF00"}, {"orange", "#FFA500"}, {"purple", "#800080"},
        {"pink", "#FFC0CB"}, {"cyan", "#00FFFF"}, {"aqua", "#00FFFF"},
        {"magenta", "#FF00FF"}, {"white", "#FFFFFF"}, {"black", "#000000"},
        {"gray", "#808080"}, {"grey", "#808080"}, {"navy", "#000080"},
        {"teal", "#008080"}, {"maroon", "#800000"}, {"olive", "#808000"},
        {"silver", "#C0C0C0"}, {"lime", "#00FF00"}, {"brown", "#A52A2A"},
        {"gold", "#FFD700"}, {"indigo", "#4B0082"}, {"violet", "#EE82EE"},
        {"coral", "#FF7F50"}, {"crimson", "#DC143C"}, {"transparent", "transparent"},
    };
    std::string low = to_lower(trim(value));
    if (auto it = colors.find(low); it != colors.end()) return it->second;
    if (low.size() == 4 && low[0] == '#') {
        std::string u = to_upper(low.substr(1));
        return std::string("#") + u[0] + u[0] + u[1] + u[1] + u[2] + u[2];
    }
    if (low.size() == 7 && low[0] == '#') return to_upper(low);
    return value;
}

BraceExpression parse_brace_expression(const std::string& raw) {
    std::string s = trim(raw);
    BraceExpression expr;
    if (s.empty()) return expr;

    if (auto c_idx = s.find("\"="); c_idx != std::string::npos) {
        expr.comment = unquote(trim(s.substr(c_idx + 2)));
        s = trim(s.substr(0, c_idx));
    }
    if (s == "0" || s.rfind("0 ", 0) == 0 || s.rfind("0\t", 0) == 0) {
        expr.reset = true;
        s = trim(s.substr(1));
    }

    for (const auto& tok : tokenize_brace_content(s)) {
        if (tok == "+" || tok.rfind("+=", 0) == 0) {
            expr.has_break = true;
            expr.break_kind = tok.rfind("+=", 0) == 0 ? tok.substr(2) : "";
            continue;
        }
        if (tok.rfind("@=", 0) == 0) {
            expr.bookmark = unquote(tok.substr(2));
            continue;
        }
        if (tok == "0") { expr.reset = true; continue; }
        if (tok == "!0") { expr.no_reset = true; continue; }
        if (tok.size() > 1 && tok[0] == '!') {
            auto canon = bool_flag_name(tok.substr(1));
            if (!canon) throw SedmatError("unknown negated flag: " + tok);
            set_bool_flag(expr, *canon, false);
            continue;
        }
        if (auto eq = tok.find('='); eq != std::string::npos) {
            std::string key = tok.substr(0, eq);
            std::string val = unquote(tok.substr(eq + 1));
            if (auto canon = bool_flag_name(key)) {
                expr.inline_spans.push_back(InlineSpan{*canon, val});
                continue;
            }
            if (key == "t" || key == "text") expr.text = val;
            else if (key == "c" || key == "color") expr.color = resolve_color(val);
            else if (key == "z" || key == "bg") expr.bg = resolve_color(val);
            else if (key == "f" || key == "font") expr.font = val;
            else if (key == "s" || key == "size") expr.size = std::stod(val);
            else if (key == "u" || key == "url") expr.url = val;
            else if (key == "h" || key == "heading") expr.heading = val;
            else if (key == "l" || key == "leading") expr.leading = std::stod(val);
            else if (key == "a" || key == "align") expr.align = val;
            else if (key == "o" || key == "opacity") expr.opacity = std::stod(val);
            else if (key == "n" || key == "indent") expr.indent = std::stoi(val);
            else if (key == "k" || key == "kerning") expr.kerning = std::stod(val);
            else if (key == "x" || key == "width") expr.width = std::stod(val);
            else if (key == "y" || key == "height") expr.height = std::stod(val);
            else if (key == "p" || key == "spacing") {
                expr.spacing_set = true;
                auto comma = val.find(',');
                if (comma != std::string::npos) {
                    expr.spacing_above = std::stod(val.substr(0, comma));
                    expr.spacing_below = std::stod(val.substr(comma + 1));
                } else {
                    expr.spacing_above = expr.spacing_below = std::stod(val);
                }
            }
            else if (key == "e" || key == "effect") expr.effect = val;
            else if (key == "cols") expr.cols = std::stoi(val);
            else if (key == "check") expr.check = (val == "y" || val == "true" || val == "1");
            else if (key == "toc") { expr.has_toc = true; expr.toc = std::stoi(val); }
            else if (key == "img") expr.img_ref = val;
            else if (key == "T") expr.table_ref = val;
            else if (key == "layout") expr.layout = val;
            else if (key == "zone") expr.zone = val;
            else if (key == "shape") expr.shape = val;
            else if (key == "fill") expr.fill = resolve_color(val);
            else if (key == "stroke") expr.stroke = resolve_color(val);
            else if (key == "notes") expr.notes = val;
            else if (key == "transition") expr.transition = val;
            else if (key == "move") expr.move_to = std::stoi(val);
            else throw SedmatError("unknown brace key: " + key);
            continue;
        }
        if (auto canon = bool_flag_name(tok)) {
            set_bool_flag(expr, *canon, true);
            continue;
        }
        if (tok == "check") expr.check = false;
        else if (tok == "toc") { expr.has_toc = true; expr.toc = -1; }
        else if (tok == "h") expr.heading = "1";
        else if (tok == "p") { expr.spacing_set = true; expr.spacing_above = 0.0; expr.spacing_below = 0.0; }
        else if (tok == "dup") expr.dup = true;
        else if (tok == "hide") expr.hide = true;
        else if (tok == "t" || tok == "f" || tok == "s" || tok == "c" || tok == "z") {}
        else throw SedmatError("unknown brace flag: " + tok);
    }
    return expr;
}

bool has_brace_formatting(const std::string& replacement) {
    for (std::size_t i = 0; i < replacement.size(); ++i) {
        if (replacement[i] == '\\' && i + 1 < replacement.size()) { i++; continue; }
        if (replacement[i] == '{' && (i == 0 || replacement[i - 1] != '$')) {
            if (auto close = find_matching_brace(replacement, i)) {
                if (looks_like_brace_expression(replacement.substr(i + 1, *close - i - 1))) return true;
            }
        }
    }
    return false;
}

std::pair<std::string, std::vector<BraceSpan>> parse_brace_replacement(const std::string& replacement) {
    if (replacement.find('{') == std::string::npos) return {replacement, {}};
    std::vector<BraceSpan> spans;
    std::string cleaned;
    for (std::size_t i = 0; i < replacement.size(); ) {
        if (replacement[i] == '\\' && i + 1 < replacement.size() &&
            (replacement[i + 1] == '{' || replacement[i + 1] == '}')) {
            cleaned.push_back(replacement[i + 1]);
            i += 2;
            continue;
        }
        if (replacement[i] == '{' && (i == 0 || replacement[i - 1] != '$')) {
            if (auto close = find_matching_brace(replacement, i)) {
                std::string inner = replacement.substr(i + 1, *close - i - 1);
                if (looks_like_brace_expression(inner)) {
                    try {
                        BraceExpression parsed = parse_brace_expression(inner);
                        if (brace_has_any_format(parsed)) {
                            std::string raw = replacement.substr(i, *close - i + 1);
                            if (!parsed.inline_spans.empty()) {
                                for (const auto& inl : parsed.inline_spans) {
                                    std::size_t start = cleaned.size();
                                    cleaned += inl.text;
                                    BraceExpression se = parsed;
                                    se.inline_spans.clear();
                                    set_bool_flag(se, inl.flag, true);
                                    spans.push_back(BraceSpan{start, cleaned.size(), false, se, raw});
                                }
                            } else {
                                std::size_t start = cleaned.size();
                                if (parsed.text) cleaned += *parsed.text;
                                bool is_global = (i == 0 || *close == replacement.size() - 1);
                                spans.push_back(BraceSpan{start, cleaned.size(), is_global, parsed, raw});
                            }
                            i = *close + 1;
                            continue;
                        }
                    } catch (const SedmatError&) {
                        // Fall through as literal
                    }
                }
            }
        }
        cleaned.push_back(replacement[i++]);
    }
    return {cleaned, spans};
}

Expression parse_expression(const std::string& raw) {
    std::string trimmed = trim(raw);
    if (trimmed.empty()) throw SedmatError("empty expression");

    auto [container, after_container] = parse_container_scope_internal(trimmed);
    if (container && !after_container.empty() && after_container[0] == '{') {
        auto close = find_matching_brace(after_container, 0);
        if (!close || *close != after_container.size() - 1) {
            throw SedmatError("malformed container brace expression");
        }
        Expression expr;
        expr.command = Command::Brace;
        expr.replacement = after_container;
        expr.container = container;
        expr.brace = parse_brace_expression(after_container.substr(1, *close - 1));
        expr.enriched = true;
        return expr;
    }

    std::optional<Address> addr;
    std::string body = after_container;
    if (!container || !container->container_op) {
        auto [a, rest] = parse_address_internal(after_container);
        addr = a;
        body = rest;
    }
    bool has_addr = addr.has_value() || container.has_value();

    Expression expr;
    char cmd = body[0];
    if (cmd == 's') {
        if (body.size() < 4) throw SedmatError("invalid substitution");
        char delim = body[1];
        if (std::isalnum(static_cast<unsigned char>(delim)) || delim == '\\') throw SedmatError("invalid delimiter");
        auto parts = split_by_delimiter(body.substr(2), delim);
        if (parts.size() < 3 || parts.size() > 4) throw SedmatError("malformed substitution");
        expr.command = Command::Substitute;
        expr.flags = parts[2];
        expr.pattern = apply_regex_flags(parts[0], expr.flags);
        expr.replacement = parts[1];
        expr.global = expr.flags.find('g') != std::string::npos;
        expr.nth_match = nth_flag(expr.flags);
    } else if (cmd == 'd') {
        expr.command = Command::Delete;
        if (body == "d") {
            if (!has_addr) throw SedmatError("delete requires pattern or address");
        } else {
            char delim = body[1];
            auto parts = split_by_delimiter(body.substr(2), delim);
            if (parts.size() < 2) throw SedmatError("malformed delete");
            expr.flags = parts[1];
            expr.pattern = apply_regex_flags(parts[0], expr.flags);
            expr.global = expr.flags.find('g') != std::string::npos;
        }
    } else if (cmd == 'a' || cmd == 'i') {
        expr.command = (cmd == 'a') ? Command::Append : Command::Insert;
        if (body.size() < 3) throw SedmatError("invalid insert/append");
        char delim = body[1];
        auto parts = split_by_delimiter(body.substr(2), delim);
        if (has_addr) {
            if (parts.size() < 2) throw SedmatError("malformed addressed insert/append");
            expr.replacement = parts[0];
            expr.flags = parts[1];
        } else {
            if (parts.size() < 3) throw SedmatError("malformed insert/append");
            expr.flags = parts[2];
            expr.pattern = apply_regex_flags(parts[0], expr.flags);
            expr.replacement = parts[1];
        }
    } else if (cmd == 'y') {
        expr.command = Command::Transliterate;
        if (body.size() < 4) throw SedmatError("invalid transliterate");
        char delim = body[1];
        auto parts = split_by_delimiter(body.substr(2), delim);
        if (parts.size() < 3) throw SedmatError("malformed transliterate");
        if (parts[0].size() != parts[1].size()) throw SedmatError("transliterate lengths differ");
        expr.pattern = parts[0];
        expr.replacement = parts[1];
        expr.flags = parts[2];
    } else {
        throw SedmatError("unsupported command: " + raw);
    }

    expr.address = addr;
    expr.container = container;
    return expr;
}

Program parse(const std::string& raw) {
    return Program{{parse_expression(raw)}};
}

Program enrich(Program program) {
    for (auto& expr : program.expressions) {
        if (expr.enriched || expr.command != Command::Substitute) continue;
        expr.enriched = true;
        if (has_brace_formatting(expr.replacement)) {
            auto [cleaned, spans] = parse_brace_replacement(expr.replacement);
            if (!spans.empty()) {
                expr.replacement = cleaned;
                expr.brace_spans = spans;
                if (spans.size() == 1 && spans[0].is_global) {
                    expr.brace = spans[0].expr;
                } else {
                    expr.brace = merge_brace_spans(spans);
                }
                if (expr.brace && expr.brace->table_ref) {
                    std::smatch m;
                    static const std::regex re(R"(^(\d+)[xX](\d+)(:header)?$)");
                    if (std::regex_match(*expr.brace->table_ref, m, re)) {
                        expr.table_create = TableCreateSpec{
                            static_cast<std::size_t>(std::stoul(m[1].str())),
                            static_cast<std::size_t>(std::stoul(m[2].str())),
                            m[3].matched
                        };
                        expr.brace.reset();
                        expr.brace_spans.clear();
                    }
                }
            }
        }
    }
    return program;
}

Program parse_program(const std::string& script) {
    Program prog;
    std::istringstream iss(script);
    std::string line;
    std::size_t line_num = 0;
    while (std::getline(iss, line)) {
        line_num++;
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        try {
            prog.expressions.push_back(parse_expression(t));
        } catch (const SedmatError& e) {
            throw SedmatError("line " + std::to_string(line_num) + ": " + e.what());
        }
    }
    return enrich(std::move(prog));
}

std::vector<AddressElement> address_elements(const DocumentSegment& seg) {
    std::vector<AddressElement> out;
    std::size_t num = 0;
    for (const auto& b : seg.blocks) {
        if (b.kind == BlockKind::SectionBreak) continue;
        num++;
        if (b.kind == BlockKind::Paragraph) {
            std::string txt = (b.item_index >= 0 && static_cast<std::size_t>(b.item_index) < seg.paragraphs.size())
                ? strip_trailing_newlines(seg.paragraphs[static_cast<std::size_t>(b.item_index)].text)
                : "";
            out.push_back(AddressElement{num, "paragraph", txt, b.start_index, b.end_index});
        } else if (b.kind == BlockKind::Table) {
            out.push_back(AddressElement{num, "table", "[table]", b.start_index, b.end_index});
        } else if (b.kind == BlockKind::TableOfContents) {
            out.push_back(AddressElement{num, "toc", "[table of contents]", b.start_index, b.end_index});
        }
    }
    return out;
}

const DocumentSegment& select_segment(const DocumentProjection& proj, const std::string& tab_id) {
    if (!tab_id.empty()) {
        for (const auto& t : proj.tabs) {
            if (t.tab_id == tab_id || iequals(t.title, tab_id)) return t;
        }
        throw SedmatError("tab not found: " + tab_id);
    }
    if (proj.legacy) return *proj.legacy;
    if (!proj.tabs.empty()) return proj.tabs[0];
    throw SedmatError("document has no content");
}

std::string format_structure(const DocumentSegment& seg) {
    std::ostringstream oss;
    std::size_t num = 0;
    for (const auto& b : seg.blocks) {
        if (b.kind == BlockKind::SectionBreak) continue;
        num++;
        if (b.kind == BlockKind::Paragraph) {
            const auto& p = seg.paragraphs[static_cast<std::size_t>(b.item_index)];
            oss << "[" << num << "] paragraph (" << p.named_style << "): \""
                << strip_trailing_newlines(p.text) << "\"\n";
        } else if (b.kind == BlockKind::TableOfContents) {
            oss << "[" << num << "] toc: \"[table of contents]\"\n";
        }
    }
    return oss.str();
}

std::string format_numbered(const DocumentSegment& seg) {
    std::ostringstream oss;
    for (const auto& el : address_elements(seg)) {
        oss << "[" << el.number << "] " << el.text << "\n";
    }
    return oss.str();
}

std::vector<AddressElement> resolve_address(const Address& addr, const std::vector<AddressElement>& elements) {
    if (elements.empty()) throw SedmatError("document has no paragraphs");
    int last = static_cast<int>(elements.size());
    int start = (addr.start == -1) ? last : addr.start;
    if (start < 1 || start > last) throw SedmatError("address out of range");
    if (!addr.has_range) return {elements[static_cast<std::size_t>(start - 1)]};
    int end = (addr.end == 0) ? start : ((addr.end == -1) ? last : addr.end);
    if (end < 1 || end > last) throw SedmatError("address end out of range");
    if (end < start) throw SedmatError("address range is reversed");
    return std::vector<AddressElement>(
        elements.begin() + (start - 1),
        elements.begin() + end);
}

std::vector<AddressMutation> plan_addressed_delete(
    const std::vector<AddressElement>& elements,
    const std::vector<AddressElement>& targets) {
    if (targets.empty()) return {};
    bool contiguous_terminal = targets.size() >= 2 && targets.back().number == elements.size();
    for (std::size_t i = 1; contiguous_terminal && i < targets.size(); ++i) {
        if (targets[i].number != targets[i - 1].number + 1) contiguous_terminal = false;
    }
    if (contiguous_terminal) {
        return {AddressMutation{targets.front().start_index, targets.back().end_index - 1, std::nullopt}};
    }
    std::vector<AddressMutation> muts;
    for (const auto& t : targets) {
        std::size_t s = t.start_index;
        std::size_t e = t.end_index;
        bool is_last = (t.number == elements.size());
        if (is_last && t.number > 1) {
            s = elements[t.number - 2].end_index - 1;
            e = t.end_index - 1;
        } else if (is_last && t.number == 1) {
            if (t.start_index >= t.end_index - 1) continue;
            e = t.end_index - 1;
        }
        muts.push_back(AddressMutation{s, e, std::nullopt});
    }
    return muts;
}

std::vector<AddressMutation> plan_addressed_insert(
    const std::vector<AddressElement>& targets,
    const std::string& replacement) {
    std::string text = replace_all(replacement, "\\n", "\n");
    if (text.empty() || text.back() != '\n') text.push_back('\n');
    std::vector<AddressMutation> out;
    for (const auto& t : targets) out.push_back(AddressMutation{t.start_index, t.start_index, text});
    return out;
}

std::vector<AddressMutation> plan_addressed_append(
    const std::vector<AddressElement>& targets,
    const std::string& replacement) {
    std::string text = replace_all(replacement, "\\n", "\n");
    if (!text.empty() && text.back() == '\n') text.pop_back();
    text = "\n" + text;
    std::vector<AddressMutation> out;
    for (const auto& t : targets) out.push_back(AddressMutation{t.end_index - 1, t.end_index - 1, text});
    return out;
}

TextPlan plan_paragraph_substitutions(const std::vector<DocumentParagraph>& paragraphs, const Expression& expr) {
    std::regex rx = compile_sed_regex(expr.pattern);
    TextPlan plan;
    long long delta = 0;

    for (const auto& p : paragraphs) {
        if (p.text.empty()) continue;
        std::vector<std::smatch> matches;
        auto it = std::sregex_iterator(p.text.begin(), p.text.end(), rx);
        auto end_it = std::sregex_iterator();
        for (; it != end_it; ++it) {
            matches.push_back(*it);
            if (!expr.global && expr.nth_match == 0) break;
            if (expr.nth_match > 0 && matches.size() >= expr.nth_match) break;
        }
        if (expr.nth_match > 0) {
            if (matches.size() < expr.nth_match) continue;
            matches = {matches[expr.nth_match - 1]};
        }
        for (const auto& m : matches) {
            std::string old_text = m[0].str();
            std::string expanded = expand_replacement(expr.replacement, m);
            if (expanded.empty() && expr.replacement.empty() && expr.brace &&
                !expr.brace->has_break && !expr.brace->has_toc &&
                !expr.brace->img_ref && !expr.brace->table_ref) {
                expanded = old_text;
            }
            std::size_t start = p.start_index + static_cast<std::size_t>(m.position(0));
            std::size_t end = start + old_text.size();
            plan.match_count++;
            plan.text_edits.push_back(TextEdit{start, end, expanded});
            if (expr.brace && !expanded.empty()) {
                std::size_t fs = static_cast<std::size_t>(static_cast<long long>(start) + delta);
                plan.formatting.push_back(FormatIntent{fs, fs + expanded.size(), expr.brace});
            }
            delta += static_cast<long long>(expanded.size()) - static_cast<long long>(old_text.size());
        }
    }
    return plan;
}

// --- MemoryDocument ---

MemoryDocument::MemoryDocument(std::vector<std::string> paragraphs) {
    MemoryTab tab;
    for (auto& p : paragraphs) tab.paragraphs.push_back(MemoryParagraph{std::move(p), "NORMAL_TEXT"});
    tabs.push_back(std::move(tab));
}

void MemoryDocument::add_tab(const std::string& tab_id, const std::string& title, std::vector<std::string> paragraphs) {
    MemoryTab tab;
    tab.id = tab_id;
    tab.title = title;
    for (auto& p : paragraphs) tab.paragraphs.push_back(MemoryParagraph{std::move(p), "NORMAL_TEXT"});
    tabs.push_back(std::move(tab));
}

std::vector<std::string> MemoryDocument::paragraph_texts(const std::string& tab_id) const {
    std::vector<std::string> out;
    for (const auto& p : find_tab(tab_id).paragraphs) out.push_back(p.text);
    return out;
}

const MemoryTab& MemoryDocument::find_tab(const std::string& tab_id) const {
    if (tab_id.empty()) {
        if (tabs.empty()) throw SedmatError("document has no tabs");
        return tabs[0];
    }
    for (const auto& t : tabs) {
        if (t.id == tab_id || iequals(t.title, tab_id)) return t;
    }
    throw SedmatError("tab not found: " + tab_id);
}

MemoryTab& MemoryDocument::find_tab_mut(const std::string& tab_id) {
    if (tab_id.empty()) {
        if (tabs.empty()) tabs.push_back(MemoryTab{});
        return tabs[0];
    }
    for (auto& t : tabs) {
        if (t.id == tab_id || iequals(t.title, tab_id)) return t;
    }
    throw SedmatError("tab not found: " + tab_id);
}

std::vector<std::size_t> MemoryDocument::resolve_container_indices(const ContainerScope& scope) const {
    if (scope.all) {
        std::vector<std::size_t> all(tabs.size());
        for (std::size_t i = 0; i < tabs.size(); ++i) all[i] = i;
        return all;
    }
    if (scope.name) {
        for (std::size_t i = 0; i < tabs.size(); ++i) {
            if (tabs[i].id == *scope.name || iequals(tabs[i].title, *scope.name)) return {i};
        }
        throw SedmatError("container not found: " + *scope.name);
    }
    if (scope.address) {
        int total = static_cast<int>(tabs.size());
        if (total == 0) throw SedmatError("document has no containers");
        int s = (scope.address->start == -1) ? total : scope.address->start;
        int e = !scope.address->has_range ? s : ((scope.address->end == -1) ? total : scope.address->end);
        if (s < 1 || s > total || e < 1 || e > total || e < s) throw SedmatError("container address out of range");
        std::vector<std::size_t> out;
        for (int i = s - 1; i < e; ++i) out.push_back(static_cast<std::size_t>(i));
        return out;
    }
    return tabs.empty() ? std::vector<std::size_t>{} : std::vector<std::size_t>{0};
}

DocumentProjection MemoryDocument::project() const {
    DocumentProjection proj;
    proj.document_id = id;
    proj.revision_id = revision_id;
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        auto seg = project_tab(tabs[i]);
        if (i == 0 && tabs[i].id.empty()) proj.legacy = seg;
        proj.tabs.push_back(std::move(seg));
    }
    return proj;
}

void MemoryDocument::apply_text_plan(const std::string& tab_id, const TextPlan& plan) {
    auto& tab = find_tab_mut(tab_id);
    auto edits = plan.text_edits;
    std::stable_sort(edits.begin(), edits.end(), [](const TextEdit& a, const TextEdit& b) {
        return a.start_index > b.start_index;
    });
    for (const auto& edit : edits) {
        auto seg = project_tab(tab);
        for (std::size_t i = 0; i < seg.paragraphs.size(); ++i) {
            const auto& p = seg.paragraphs[i];
            if (edit.start_index >= p.start_index && edit.end_index <= p.end_index) {
                std::size_t rs = edit.start_index - p.start_index;
                std::size_t re = edit.end_index - p.start_index;
                std::string updated = p.text.substr(0, rs) + edit.insert_text + p.text.substr(re);
                if (!updated.empty() && updated.back() == '\n') updated.pop_back();
                tab.paragraphs[i].text = updated;
                break;
            }
        }
    }
    tab.formatting.insert(tab.formatting.end(), plan.formatting.begin(), plan.formatting.end());
}

void MemoryDocument::apply_address_mutations(const std::string& tab_id, const std::vector<AddressMutation>& mutations) {
    auto& tab = find_tab_mut(tab_id);
    auto ordered = mutations;
    std::stable_sort(ordered.begin(), ordered.end(), [](const AddressMutation& a, const AddressMutation& b) {
        if (a.start_index != b.start_index) return a.start_index > b.start_index;
        return a.end_index > b.end_index;
    });
    for (const auto& m : ordered) {
        auto seg = project_tab(tab);
        if (m.insert_text && (m.end_index == 0 || m.end_index == m.start_index)) {
            const std::string& ins = *m.insert_text;
            bool is_append = !ins.empty() && ins.front() == '\n';
            std::string cleaned = is_append ? ins.substr(1) : ins;
            if (!cleaned.empty() && cleaned.back() == '\n') cleaned.pop_back();
            std::vector<MemoryParagraph> added;
            std::istringstream iss(cleaned);
            std::string ln;
            while (std::getline(iss, ln)) added.push_back(MemoryParagraph{ln, "NORMAL_TEXT"});
            if (added.empty()) added.push_back(MemoryParagraph{"", "NORMAL_TEXT"});
            bool placed = false;
            for (std::size_t i = 0; i < seg.blocks.size(); ++i) {
                const auto& b = seg.blocks[i];
                if (is_append && (m.start_index == b.end_index - 1 || m.start_index == b.end_index)) {
                    tab.paragraphs.insert(tab.paragraphs.begin() + static_cast<long>(i + 1), added.begin(), added.end());
                    placed = true;
                    break;
                }
                if (!is_append && m.start_index == b.start_index) {
                    tab.paragraphs.insert(tab.paragraphs.begin() + static_cast<long>(i), added.begin(), added.end());
                    placed = true;
                    break;
                }
            }
            if (!placed) tab.paragraphs.insert(tab.paragraphs.end(), added.begin(), added.end());
            continue;
        }
        if (seg.blocks.size() == 1 && m.start_index >= seg.blocks[0].start_index && m.end_index <= seg.blocks[0].end_index) {
            tab.paragraphs[0].text.clear();
            continue;
        }
        std::vector<MemoryParagraph> kept;
        for (std::size_t i = 0; i < seg.blocks.size(); ++i) {
            const auto& b = seg.blocks[i];
            if (m.start_index == b.end_index - 1 && m.end_index > b.end_index) {
                kept.push_back(tab.paragraphs[i]);
                continue;
            }
            if ((b.start_index >= m.start_index && b.end_index <= m.end_index) ||
                (b.start_index >= m.start_index && b.end_index - 1 <= m.end_index && i + 1 == seg.blocks.size())) {
                continue;
            }
            kept.push_back(tab.paragraphs[i]);
        }
        if (kept.empty()) kept.push_back(MemoryParagraph{"", "NORMAL_TEXT"});
        tab.paragraphs = std::move(kept);
    }
}

std::size_t MemoryDocument::apply_transliterate(
    const std::string& tab_id,
    const std::string& source,
    const std::string& dest,
    const std::vector<AddressElement>& targets) {
    auto& tab = find_tab_mut(tab_id);
    std::unordered_set<std::size_t> tnums;
    for (const auto& t : targets) tnums.insert(t.number);
    std::size_t changed = 0;
    for (std::size_t i = 0; i < tab.paragraphs.size(); ++i) {
        if (!tnums.empty() && !tnums.count(i + 1)) continue;
        for (char& ch : tab.paragraphs[i].text) {
            if (auto pos = source.find(ch); pos != std::string::npos) {
                ch = dest[pos];
                changed++;
            }
        }
    }
    return changed;
}

std::size_t MemoryDocument::apply_container_op(const ContainerScope& scope, const Expression& expr) {
    auto indices = resolve_container_indices(scope);
    if (expr.command == Command::Delete) {
        for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
            tabs.erase(tabs.begin() + static_cast<long>(*it));
        }
        return indices.size();
    }
    if (expr.command == Command::Append || expr.command == Command::Insert) {
        MemoryTab nt;
        nt.id = "tab-" + std::to_string(tabs.size() + 1);
        nt.title = trim(expr.replacement);
        nt.paragraphs.push_back(MemoryParagraph{nt.title, "NORMAL_TEXT"});
        std::size_t anchor = indices.empty() ? 0 : indices.back();
        std::size_t pos = (expr.command == Command::Append) ? std::min(anchor + 1, tabs.size()) : anchor;
        tabs.insert(tabs.begin() + static_cast<long>(pos), std::move(nt));
        return 1;
    }
    if (expr.command == Command::Brace && expr.brace) {
        for (auto idx : indices) {
            auto& t = tabs[idx];
            if (expr.brace->bg) t.background = expr.brace->bg;
            if (expr.brace->notes) t.notes = expr.brace->notes;
            if (expr.brace->transition) t.transition = expr.brace->transition;
            if (expr.brace->layout) t.layout = expr.brace->layout;
            if (expr.brace->hide) t.hidden = true;
        }
        return indices.size();
    }
    return 0;
}

// --- Executor ---

ExecutionReport Executor::execute_string(const std::string& script, const ExecuteOptions& opts) {
    return execute(parse_program(script), opts);
}

ExecutionReport Executor::execute(Program program, const ExecuteOptions& opts) {
    auto enriched = enrich(std::move(program));
    ExecutionReport report{opts.dry_run, 0};

    for (const auto& expr : enriched.expressions) {
        if (expr.container && (expr.container->container_op || expr.command == Command::Brace)) {
            report.total_matches += opts.dry_run ? 1 : backend_.apply_container_op(*expr.container, expr);
            continue;
        }
        auto proj = backend_.project();
        std::string tab_id = opts.tab_id;
        if (expr.container) {
            if (expr.container->name) {
                tab_id = *expr.container->name;
            } else if (expr.container->address) {
                int total = static_cast<int>(proj.tabs.size());
                int idx = (expr.container->address->start == -1) ? total : expr.container->address->start;
                if (idx < 1 || idx > total) throw SedmatError("container address out of range");
                const auto& t = proj.tabs[static_cast<std::size_t>(idx - 1)];
                tab_id = !t.tab_id.empty() ? t.tab_id : t.title;
            }
        }
        const auto& seg = select_segment(proj, tab_id);

        if (expr.command == Command::Substitute) {
            std::vector<DocumentParagraph> paras;
            if (expr.address) {
                for (const auto& t : resolve_address(*expr.address, address_elements(seg))) {
                    paras.push_back(DocumentParagraph{t.text + "\n", t.start_index, t.end_index, "NORMAL_TEXT"});
                }
            } else {
                paras = seg.paragraphs;
            }
            auto plan = plan_paragraph_substitutions(paras, expr);
            report.total_matches += plan.match_count;
            if (!opts.dry_run && plan.match_count > 0) backend_.apply_text_plan(tab_id, plan);
        } else if (expr.command == Command::Delete) {
            auto elems = address_elements(seg);
            auto targets = expr.address ? resolve_address(*expr.address, elems) : elems;
            if (!expr.pattern.empty()) {
                std::regex rx = compile_sed_regex(expr.pattern);
                targets.erase(std::remove_if(targets.begin(), targets.end(), [&](const AddressElement& e) {
                    return !std::regex_search(e.text, rx);
                }), targets.end());
            }
            auto muts = plan_addressed_delete(elems, targets);
            report.total_matches += targets.size();
            if (!opts.dry_run && !muts.empty()) backend_.apply_address_mutations(tab_id, muts);
        } else if (expr.command == Command::Append || expr.command == Command::Insert) {
            auto elems = address_elements(seg);
            std::vector<AddressElement> targets;
            if (expr.address) {
                targets = resolve_address(*expr.address, elems);
            } else {
                std::regex rx = compile_sed_regex(expr.pattern);
                for (const auto& e : elems) {
                    if (std::regex_search(e.text, rx)) targets.push_back(e);
                }
            }
            auto muts = (expr.command == Command::Insert)
                ? plan_addressed_insert(targets, expr.replacement)
                : plan_addressed_append(targets, expr.replacement);
            report.total_matches += targets.size();
            if (!opts.dry_run && !muts.empty()) backend_.apply_address_mutations(tab_id, muts);
        } else if (expr.command == Command::Transliterate) {
            auto targets = expr.address ? resolve_address(*expr.address, address_elements(seg)) : std::vector<AddressElement>{};
            if (!opts.dry_run) {
                report.total_matches += backend_.apply_transliterate(tab_id, expr.pattern, expr.replacement, targets);
            }
        }
    }
    return report;
}

} // namespace sedmat
