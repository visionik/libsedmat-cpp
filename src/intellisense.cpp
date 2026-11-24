#include "sedmat/sedmat.hpp"

#include <algorithm>
#include <cctype>

namespace sedmat {
namespace {

std::string to_lower_ascii(const std::string& s) {
    std::string out = s;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool starts_with_ci(const std::string& s, const std::string& prefix) {
    if (prefix.size() > s.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

bool contains_ci(const std::string& s, const std::string& needle) {
    if (needle.empty()) return true;
    return to_lower_ascii(s).find(to_lower_ascii(needle)) != std::string::npos;
}

std::string trim_ascii(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

Profile normalize_profile(Profile p) {
    return p == Profile::Word ? Profile::Document : p;
}

bool matches_profile(const FlagDefinition& def, Profile profile) {
    if (profile == Profile::All || def.profiles.empty()) return true;
    for (Profile p : def.profiles) {
        if (normalize_profile(p) == profile) return true;
    }
    return false;
}

const std::vector<FlagDefinition>& builtin_flags() {
    static const std::vector<FlagDefinition> flags = {
        // Timeline directives
        {"+2d", "+2d", "Extend task duration by +2 working days", {Profile::Timeline}},
        {"-1d", "-1d", "Shorten task duration by -1 working day", {Profile::Timeline}},
        {"5d", "5d", "Set exact task duration to 5 working days", {Profile::Timeline}},
        {"+4h", "+4h", "Extend task duration by +4 working hours", {Profile::Timeline}},
        {"50%", "50%", "Set task completion to 50%", {Profile::Timeline}},
        {"100%", "100%", "Mark task 100% complete", {Profile::Timeline}},
        {"milestone", "milestone", "Convert task into a 0-day milestone", {Profile::Timeline}},
        // Core inline formatting
        {"b", "b", "Bold", {Profile::Document, Profile::Slides, Profile::Spreadsheet, Profile::Vector}},
        {"i", "i", "Italic", {Profile::Document, Profile::Slides, Profile::Spreadsheet, Profile::Vector}},
        {"_", "_", "Underline", {Profile::Document, Profile::Slides, Profile::Spreadsheet}},
        {"-", "-", "Strikethrough", {Profile::Document, Profile::Slides, Profile::Spreadsheet}},
        {"c=", "c=", "Text / foreground color (red, blue, #FF0000…)", {}},
        {"z=", "z=", "Background / highlight / fill color", {}},
        {"s=", "s=", "Font size in pt (e.g. s=14)", {Profile::Document, Profile::Slides, Profile::Spreadsheet, Profile::Vector}},
        {"f=", "f=", "Font family (e.g. f=Inter)", {Profile::Document, Profile::Slides, Profile::Spreadsheet, Profile::Vector}},
        {"a=", "a=", "Alignment (left, center, right, justify)", {Profile::Document, Profile::Slides, Profile::Spreadsheet, Profile::Vector}},
        {"t=", "t=", "Replacement text while styling match", {Profile::Document, Profile::Slides, Profile::Vector}},
        {"u=", "u=", "Hyperlink URL, #bookmark, or chip:// URI", {Profile::Document, Profile::Slides, Profile::Spreadsheet}},
        {"#", "#", "Inline monospace code", {Profile::Document, Profile::Slides}},
        {"w", "w", "Small caps", {Profile::Document, Profile::Slides}},
        {"^", "^", "Superscript", {Profile::Document, Profile::Slides}},
        {",", ",", "Subscript", {Profile::Document, Profile::Slides}},
        {"!b", "!b", "Remove bold (negate any boolean flag with !)", {Profile::Document, Profile::Slides, Profile::Vector}},
        {"!0", "!0", "Additive mode (opt out of implicit reset)", {Profile::Document, Profile::Slides, Profile::Spreadsheet, Profile::Vector}},
        {"0", "0", "Reset all formatting", {Profile::Document, Profile::Slides, Profile::Spreadsheet, Profile::Vector}},
        // Slides, Vector & Shape flags (v3.7)
        {"shape=", "shape=", "Shape geometry (roundRect, rect, diamond, ellipse…)", {Profile::Slides, Profile::Vector}},
        {"fill=", "fill=", "Shape fill hex or named color", {Profile::Slides, Profile::Vector, Profile::Timeline}},
        {"stroke=", "stroke=", "Shape border hex or named color", {Profile::Slides, Profile::Vector}},
        {"layout=", "layout=", "Slide layout (title, content, two-col, section, blank)", {Profile::Slides}},
        {"zone=", "zone=", "Placement zone (left, right, top, bottom, center, footer)", {Profile::Slides}},
        {"transition=", "transition=", "Slide transition (fade, push, wipe, none)", {Profile::Slides}},
        {"move=", "move=", "Move container to 1-based position (e.g. @3{move=1})", {Profile::Slides, Profile::Spreadsheet}},
        {"dup", "dup", "Duplicate container (e.g. @2{dup})", {Profile::Slides, Profile::Vector, Profile::Spreadsheet}},
        {"hide", "hide", "Hide container", {Profile::Slides, Profile::Spreadsheet}},
        // Document-specific structural flags
        {"h=", "h=", "Heading level (t=Title, s=Subtitle, 1..6, 0=Normal)", {Profile::Document}},
        {"p=", "p=", "Paragraph spacing before,after pt (e.g. p=12,6)", {Profile::Document}},
        {"n=", "n=", "Paragraph left indent level", {Profile::Document}},
        {"cols=", "cols=", "Section columns (e.g. cols=2)", {Profile::Document}},
        {"\"=", "\"=", "Attach review comment (e.g. \"=Check figure)", {Profile::Document}},
        {"@=", "@=", "Insert bookmark anchor (e.g. @=summary)", {Profile::Document}},
        {"check", "check", "Insert checkbox (check or check=y)", {Profile::Document}},
        {"toc", "toc", "Insert Table of Contents", {Profile::Document}},
        {"+=p", "+=p", "Insert page break (+, +=p, +=c, +=s)", {Profile::Document}},
        // Tables & images
        {"T=", "T=", "Table create/cell ref (T=3x4:header or T=1!A1)", {Profile::Document, Profile::Slides}},
        {"img=", "img=", "Target image by 1-based index (img=1, img=-1)", {Profile::Document, Profile::Slides}},
        {"x=", "x=", "Image width in px (e.g. x=420)", {Profile::Document, Profile::Slides}},
    };
    return flags;
}

const std::vector<ValueOption>& color_values() {
    // Canonical sedmat.org 18-color palette swatches
    static const std::vector<ValueOption> colors = {
        {"red", "Canonical red", "#FF0000"},
        {"blue", "Canonical blue", "#0000FF"},
        {"green", "Canonical green", "#00FF00"},
        {"orange", "Canonical orange", "#FF8C00"},
        {"purple", "Canonical purple", "#800080"},
        {"navy", "Canonical navy", "#000080"},
        {"teal", "Canonical teal", "#008080"},
        {"gray", "Canonical gray", "#808080"},
        {"yellow", "Highlight yellow", "#FFFF00"},
        {"black", "Canonical black", "#000000"},
        {"white", "Canonical white", "#FFFFFF"},
    };
    return colors;
}

const std::vector<ValueOption>* builtin_values(const std::string& key) {
    if (key == "c" || key == "z" || key == "fill" || key == "stroke") {
        return &color_values();
    }
    static const std::unordered_map<std::string, std::vector<ValueOption>> map = {
        {"a", {
            {"left", "Align left", std::nullopt},
            {"center", "Align center", std::nullopt},
            {"right", "Align right", std::nullopt},
            {"justify", "Justify paragraph", std::nullopt},
        }},
        {"h", {
            {"t", "Document Title", std::nullopt},
            {"s", "Subtitle", std::nullopt},
            {"1", "Heading 1", std::nullopt},
            {"2", "Heading 2", std::nullopt},
            {"3", "Heading 3", std::nullopt},
            {"0", "Normal paragraph", std::nullopt},
        }},
        {"layout", {
            {"content", "Title + bullet body", std::nullopt},
            {"title", "Centered cover title", std::nullopt},
            {"two-col", "Two-column comparison", std::nullopt},
            {"section", "Section divider header", std::nullopt},
            {"comparison", "Side-by-side comparison", std::nullopt},
            {"blank", "Blank canvas", std::nullopt},
        }},
        {"zone", {
            {"center", "Center stage", std::nullopt},
            {"left", "Left column", std::nullopt},
            {"right", "Right column", std::nullopt},
            {"top", "Upper band", std::nullopt},
            {"bottom", "Lower band", std::nullopt},
            {"footer", "Bottom footer", std::nullopt},
        }},
        {"shape", {
            {"roundRect", "Rounded rectangle", std::nullopt},
            {"rect", "Rectangle", std::nullopt},
            {"ellipse", "Circle / oval", std::nullopt},
            {"diamond", "Decision diamond", std::nullopt},
            {"rightArrow", "Right arrow", std::nullopt},
            {"chevron", "Process chevron", std::nullopt},
        }},
        {"transition", {
            {"fade", "Smooth crossfade", std::nullopt},
            {"push", "Slide push", std::nullopt},
            {"wipe", "Directional wipe", std::nullopt},
            {"none", "No transition", std::nullopt},
        }},
        {"f", {
            {"Arial", "Classic sans-serif", std::nullopt},
            {"Calibri", "Office sans-serif", std::nullopt},
            {"Georgia", "Editorial serif", std::nullopt},
            {"Inter", "Modern UI sans", std::nullopt},
            {"Consolas", "Monospace code", std::nullopt},
        }},
        {"s", {
            {"11", "11 pt default", std::nullopt},
            {"12", "12 pt body", std::nullopt},
            {"16", "16 pt lead", std::nullopt},
            {"24", "24 pt heading", std::nullopt},
            {"36", "36 pt title", std::nullopt},
        }},
    };
    auto it = map.find(key);
    return it != map.end() ? &it->second : nullptr;
}

std::optional<std::size_t> find_open_brace(const std::string& before) {
    std::size_t depth = 0;
    std::optional<std::size_t> last_open;
    for (std::size_t i = 0; i < before.size(); ++i) {
        if (before[i] == '\\' && i + 1 < before.size()) {
            ++i;
            continue;
        }
        if (before[i] == '{') {
            ++depth;
            last_open = i;
        } else if (before[i] == '}') {
            if (depth > 0) --depth;
            if (depth == 0) last_open = std::nullopt;
        }
    }
    return depth > 0 ? last_open : std::nullopt;
}

std::size_t count_unescaped_slashes(const std::string& s) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            continue;
        }
        if (s[i] == '/') ++count;
    }
    return count;
}

std::optional<std::string> compute_ghost_suffix(
    const std::string& input,
    std::size_t cursor,
    const std::vector<Suggestion>& suggestions) {
    if (cursor != input.size() || suggestions.empty()) return std::nullopt;
    const Suggestion& top = suggestions.front();
    if (top.replace_from > top.replace_to || top.replace_to > input.size()) return std::nullopt;
    std::string typed = input.substr(top.replace_from, top.replace_to - top.replace_from);
    if (top.insert_text.size() > typed.size() && starts_with_ci(top.insert_text, typed)) {
        return top.insert_text.substr(typed.size());
    }
    return std::nullopt;
}

std::optional<std::size_t> find_sub_cmd_end(const std::string& seg) {
    for (std::size_t i = 0; i + 1 < seg.size(); ++i) {
        if (seg[i] == 's' && seg[i + 1] == '/') {
            bool valid_prev = (i == 0);
            if (!valid_prev) {
                unsigned char p = static_cast<unsigned char>(seg[i - 1]);
                valid_prev = (p == ':' || std::isdigit(p) || p == '$' || std::isspace(p));
            }
            if (valid_prev) return i + 2;
        }
    }
    return std::nullopt;
}

std::size_t match_region_prefix(const std::string& rem) {
    static const char* const regions[] = {
        "title:", "subtitle:", "sub:", "body:", "notes:", "header:", "footer:"
    };
    for (const char* reg : regions) {
        std::string r(reg);
        if (starts_with_ci(rem, r)) return r.size();
    }
    if (starts_with_ci(rem, "shape=")) {
        std::size_t colon = rem.find(':', 6);
        if (colon != std::string::npos && colon > 6 && rem[6] != '0') {
            bool all_digits = true;
            for (std::size_t k = 6; k < colon; ++k) {
                if (!std::isdigit(static_cast<unsigned char>(rem[k]))) {
                    all_digits = false;
                    break;
                }
            }
            if (all_digits) return colon + 1;
        }
    }
    return 0;
}

} // namespace

Diagnostic extract_diagnostic(const std::string& error_message) {
    std::string lower = to_lower_ascii(error_message);
    const std::string marker = "at offset ";
    std::size_t pos = lower.find(marker);
    if (pos != std::string::npos) {
        std::size_t num_start = pos + marker.size();
        std::size_t colon = error_message.find(':', num_start);
        if (colon != std::string::npos) {
            std::string num_str = trim_ascii(error_message.substr(num_start, colon - num_start));
            if (!num_str.empty() && std::all_of(num_str.begin(), num_str.end(), [](unsigned char c) { return std::isdigit(c); })) {
                std::size_t offset = static_cast<std::size_t>(std::stoull(num_str));
                std::string rest = trim_ascii(error_message.substr(colon + 1));
                const std::string em_dash = " \xE2\x80\x94 "; // " — "
                std::size_t dash = rest.find(em_dash);
                if (dash != std::string::npos) {
                    return Diagnostic{
                        false,
                        trim_ascii(rest.substr(0, dash)),
                        offset,
                        1,
                        trim_ascii(rest.substr(dash + em_dash.size())),
                    };
                }
                return Diagnostic{false, rest, offset, 1, std::nullopt};
            }
        }
    }
    return Diagnostic{false, error_message, std::nullopt, 0, std::nullopt};
}

IntelliSenseResult get_completions(const std::string& input, std::size_t cursor, const ContextInfo& ctx) {
    Profile profile = normalize_profile(ctx.mode);
    std::size_t safe_cursor = std::min(cursor, input.size());
    std::string before = input.substr(0, safe_cursor);
    std::size_t container_num = ctx.active_container.value_or(ctx.active_slide.value_or(1));
    std::string cnum = std::to_string(container_num);

    std::size_t last_semi = before.rfind(';');
    std::size_t seg_start = (last_semi != std::string::npos) ? last_semi + 1 : 0;
    std::size_t cmd_start = seg_start;
    while (cmd_start < before.size() && std::isspace(static_cast<unsigned char>(before[cmd_start]))) {
        ++cmd_start;
    }
    std::string seg = before.substr(cmd_start);

    // 1. Inside `{...}` formatting / operation braces
    auto brace_idx = find_open_brace(before);
    if (brace_idx.has_value() && *brace_idx >= cmd_start) {
        std::string inside = before.substr(*brace_idx + 1);
        std::size_t last_space = inside.find_last_of(" \t");
        std::size_t token_start = (last_space != std::string::npos) ? (*brace_idx + 1 + last_space + 1) : (*brace_idx + 1);
        std::string token = before.substr(token_start);

        std::size_t eq_idx = token.find('=');
        if (eq_idx != std::string::npos && eq_idx > 0) {
            std::string key = token.substr(0, eq_idx);
            std::string val_typed = token.substr(eq_idx + 1);
            std::size_t val_start = token_start + eq_idx + 1;

            const std::vector<ValueOption>* value_list = nullptr;
            if (ctx.extensions.has_value()) {
                auto it = ctx.extensions->values.find(key);
                if (it != ctx.extensions->values.end()) {
                    value_list = &it->second;
                }
            }
            if (!value_list) {
                value_list = builtin_values(key);
            }
            if (value_list) {
                std::vector<Suggestion> filtered;
                for (const auto& v : *value_list) {
                    if (val_typed.empty() || contains_ci(v.value, val_typed) || contains_ci(v.detail, val_typed)) {
                        filtered.push_back(Suggestion{
                            v.value, v.value, val_start, safe_cursor, v.detail, SuggestionKind::Value, v.swatch
                        });
                    }
                }
                return IntelliSenseResult{
                    "Value for {" + key + "=\xE2\x80\xA6}",
                    {{"{ ", false}, {key + "=", false}, {"value", true}, {" }", false}},
                    filtered,
                    compute_ghost_suffix(input, safe_cursor, filtered),
                };
            }
        }

        std::vector<FlagDefinition> all_flags;
        if (ctx.extensions.has_value()) {
            all_flags.insert(all_flags.end(), ctx.extensions->flags.begin(), ctx.extensions->flags.end());
        }
        const auto& base_flags = builtin_flags();
        all_flags.insert(all_flags.end(), base_flags.begin(), base_flags.end());

        std::vector<Suggestion> suggestions;
        for (const auto& f : all_flags) {
            if (!matches_profile(f, profile)) continue;
            if (!token.empty() && !starts_with_ci(f.key, token) && !contains_ci(f.detail, token)) continue;
            suggestions.push_back(Suggestion{
                f.key, f.insert, token_start, safe_cursor, f.detail, SuggestionKind::Flag, std::nullopt
            });
            if (suggestions.size() >= 12) break;
        }
        return IntelliSenseResult{
            "SEDMAT Brace Flags ({\xE2\x80\xA6})",
            {{"{", false}, {"b i _ - c= z= s= f= a= !0 \xE2\x80\xA6", true}, {"}", false}},
            suggestions,
            compute_ghost_suffix(input, safe_cursor, suggestions),
        };
    }

    // 2. Typing a `@` container scope prefix
    if (!seg.empty() && seg[0] == '@') {
        std::size_t first_colon = seg.find(':');
        if (first_colon == std::string::npos) {
            std::vector<SnippetOption> base_scopes;
            if (profile == Profile::Slides) {
                base_scopes = {
                    {"@" + cnum + ":", "@" + cnum + ":", "Target slide " + cnum, SuggestionKind::Scope, std::nullopt},
                    {"@" + cnum + ":title:", "@" + cnum + ":title:", "Target slide " + cnum + " title", SuggestionKind::Scope, std::nullopt},
                    {"@" + cnum + ":body:", "@" + cnum + ":body:", "Target slide " + cnum + " body", SuggestionKind::Scope, std::nullopt},
                    {"@" + cnum + ":notes:", "@" + cnum + ":notes:", "Target slide " + cnum + " speaker notes", SuggestionKind::Scope, std::nullopt},
                    {"@*:", "@*:", "Target ALL slides", SuggestionKind::Scope, std::nullopt},
                    {"@" + cnum + ":{z=#0F172A}", "@" + cnum + ":{z=#0F172A}", "Set slide background", SuggestionKind::Snippet, std::nullopt},
                    {"@$a/{layout=two-col}New Slide/", "@$a/{layout=two-col}New Slide/", "Append new slide", SuggestionKind::Snippet, std::nullopt},
                    {"@" + cnum + "{dup}", "@" + cnum + "{dup}", "Duplicate slide " + cnum, SuggestionKind::Command, std::nullopt},
                    {"@" + cnum + "{move=1}", "@" + cnum + "{move=1}", "Move slide " + cnum + " to position 1", SuggestionKind::Command, std::nullopt},
                    {"@" + cnum + "d", "@" + cnum + "d", "Delete slide " + cnum, SuggestionKind::Command, std::nullopt},
                };
            } else if (profile == Profile::Spreadsheet) {
                base_scopes = {
                    {"@" + cnum + ":", "@" + cnum + ":", "Target sheet " + cnum, SuggestionKind::Scope, std::nullopt},
                    {"@sheet=", "@sheet=", "Target named worksheet", SuggestionKind::Scope, std::nullopt},
                    {"@*:", "@*:", "Target ALL sheets", SuggestionKind::Scope, std::nullopt},
                };
            } else {
                base_scopes = {
                    {"@header:", "@header:", "Target document header", SuggestionKind::Scope, std::nullopt},
                    {"@footer:", "@footer:", "Target document footer", SuggestionKind::Scope, std::nullopt},
                    {"@" + cnum + ":", "@" + cnum + ":", "Target container " + cnum, SuggestionKind::Scope, std::nullopt},
                    {"@*:", "@*:", "Target all containers", SuggestionKind::Scope, std::nullopt},
                };
            }

            std::vector<SnippetOption> all_scopes;
            if (ctx.extensions.has_value()) {
                all_scopes.insert(all_scopes.end(), ctx.extensions->scope_snippets.begin(), ctx.extensions->scope_snippets.end());
            }
            all_scopes.insert(all_scopes.end(), base_scopes.begin(), base_scopes.end());

            std::vector<Suggestion> filtered;
            for (const auto& s : all_scopes) {
                if (contains_ci(s.label, seg) || contains_ci(s.detail, seg)) {
                    filtered.push_back(Suggestion{
                        s.label, s.insert, cmd_start, safe_cursor, s.detail, s.kind, s.swatch
                    });
                }
            }
            return IntelliSenseResult{
                "Container Scope (@)",
                {{"@container", true}, {"[:region]:command", false}},
                filtered,
                compute_ghost_suffix(input, safe_cursor, filtered),
            };
        }

        std::string after_first = seg.substr(first_colon + 1);
        if (after_first.find(':') == std::string::npos &&
            after_first.find('/') == std::string::npos &&
            (after_first.empty() || after_first[0] != '{')) {
            std::size_t token_start = cmd_start + first_colon + 1;
            std::vector<SnippetOption> post_scope;
            if (profile == Profile::Slides) {
                post_scope = {
                    {"title:", "title:", "Slide title placeholder", SuggestionKind::Scope, std::nullopt},
                    {"sub:", "sub:", "Slide subtitle placeholder", SuggestionKind::Scope, std::nullopt},
                    {"body:", "body:", "Slide body / bullets", SuggestionKind::Scope, std::nullopt},
                    {"notes:", "notes:", "Speaker notes", SuggestionKind::Scope, std::nullopt},
                    {"s/old/new/g", "s/", "Search & replace text or style", SuggestionKind::Command, std::nullopt},
                    {"a/text/", "a/", "Append bullet / paragraph", SuggestionKind::Command, std::nullopt},
                    {"d/pattern/", "d/", "Delete matching lines", SuggestionKind::Command, std::nullopt},
                    {"{z=#0F172A}", "{z=", "Set slide background color", SuggestionKind::Flag, std::nullopt},
                    {"{transition=fade}", "{transition=", "Set slide transition", SuggestionKind::Flag, std::nullopt},
                };
            } else {
                post_scope = {
                    {"s/old/new/g", "s/", "Search & replace in container", SuggestionKind::Command, std::nullopt},
                    {"a/text/", "a/", "Append paragraph", SuggestionKind::Command, std::nullopt},
                    {"d/pattern/", "d/", "Delete matching paragraphs", SuggestionKind::Command, std::nullopt},
                };
            }

            std::vector<Suggestion> filtered;
            for (const auto& item : post_scope) {
                if (after_first.empty() || starts_with_ci(item.label, after_first) || contains_ci(item.detail, after_first)) {
                    filtered.push_back(Suggestion{
                        item.label, item.insert, token_start, safe_cursor, item.detail, item.kind, item.swatch
                    });
                }
            }
            std::string prefix = seg.substr(0, first_colon + 1);
            return IntelliSenseResult{
                "Inside " + prefix,
                {{prefix, false}, {"region: | command | {flags}", true}},
                filtered,
                compute_ghost_suffix(input, safe_cursor, filtered),
            };
        }
    }

    // 3. Inside an active `s/.../.../...` substitution command
    auto sub_end = find_sub_cmd_end(seg);
    if (sub_end.has_value()) {
        std::string after_s = seg.substr(*sub_end);
        std::size_t slashes = count_unescaped_slashes(after_s);
        if (slashes == 0) {
            return IntelliSenseResult{
                "Substitution \xE2\x80\x94 Pattern (RegExp)",
                {{"s/", false}, {"pattern", true}, {"/replacement/[g i m N]", false}},
                {Suggestion{".*/{b}/", ".*/{b}/", safe_cursor, safe_cursor, "Match entire element and apply formatting", SuggestionKind::Snippet, std::nullopt}},
                std::nullopt,
            };
        }
        if (slashes == 1) {
            return IntelliSenseResult{
                "Substitution \xE2\x80\x94 Replacement (type { for formatting flags)",
                {{"s/pattern/", false}, {"{flags}replacement", true}, {"/[g i m N]", false}},
                {
                    Suggestion{"{b c=red}", "{b c=red}", safe_cursor, safe_cursor, "Bold red highlight (keeps matched text)", SuggestionKind::Snippet, "#FF0000"},
                    Suggestion{"{b}", "{b}", safe_cursor, safe_cursor, "Make match bold", SuggestionKind::Flag, std::nullopt},
                    Suggestion{"{z=yellow}", "{z=yellow}", safe_cursor, safe_cursor, "Highlight match yellow", SuggestionKind::Flag, "#FFFF00"},
                    Suggestion{"{!0 b c=blue}", "{!0 b c=blue}", safe_cursor, safe_cursor, "Add bold blue without resetting existing style", SuggestionKind::Snippet, "#0000FF"},
                    Suggestion{"$0", "$0", safe_cursor, safe_cursor, "Insert original matched text", SuggestionKind::Value, std::nullopt},
                },
                std::nullopt,
            };
        }

        std::size_t last_slash = after_s.rfind('/');
        std::string trailing = (last_slash != std::string::npos) ? after_s.substr(last_slash + 1) : "";
        struct TrailingOpt { const char* flag; const char* detail; };
        static const TrailingOpt opts[] = {
            {"g", "Global \xE2\x80\x94 replace every match"},
            {"i", "Case-insensitive regex match"},
            {"m", "Multiline ^ and $ anchors"},
            {"2", "Replace only the 2nd match"},
        };
        std::vector<Suggestion> trailing_opts;
        for (const auto& o : opts) {
            if (trailing.find(o.flag) == std::string::npos) {
                trailing_opts.push_back(Suggestion{
                    o.flag, o.flag, safe_cursor, safe_cursor, o.detail, SuggestionKind::Flag, std::nullopt
                });
            }
        }
        return IntelliSenseResult{
            "Substitution \xE2\x80\x94 Trailing Flags",
            {{"s/pattern/replacement/", false}, {"[g i m N]", true}},
            trailing_opts,
            compute_ghost_suffix(input, safe_cursor, trailing_opts),
        };
    }

    // 4. Default / Start of expression
    std::vector<SnippetOption> base_starters;
    if (profile == Profile::Slides) {
        base_starters = {
            {"@" + cnum + ":title:s/.*/New Title/", "@" + cnum + ":title:s/.*/", "Edit slide " + cnum + " title", SuggestionKind::Snippet, std::nullopt},
            {"s/old/{b c=blue t=new}/g", "s/", "Search & replace on slide " + cnum, SuggestionKind::Command, std::nullopt},
            {"@*:s/old/new/gi", "@*:s/", "Search & replace across ALL slides", SuggestionKind::Snippet, std::nullopt},
            {"@" + cnum + ":notes:a/Talking point/", "@" + cnum + ":notes:a/", "Append speaker note on slide " + cnum, SuggestionKind::Snippet, std::nullopt},
            {"@$a/{layout=two-col}New Slide/", "@$a/{layout=two-col}", "Append a new slide", SuggestionKind::Snippet, std::nullopt},
        };
    } else {
        base_starters = {
            {"s/old/new/g", "s/", "Search & replace across document", SuggestionKind::Command, std::nullopt},
            {"s/TODO/{b c=red}/g", "s/TODO/{b c=red}/g", "Highlight every TODO in bold red", SuggestionKind::Snippet, std::nullopt},
            {"1s/.*/{h=t}/", "1s/.*/{h=t}/", "Format element 1 as Title", SuggestionKind::Snippet, std::nullopt},
            {"$a/{h=2}Next Steps/", "$a/{h=2}", "Append a new Heading 2 at end", SuggestionKind::Snippet, std::nullopt},
            {"d/DRAFT/i", "d/", "Delete elements matching pattern", SuggestionKind::Command, std::nullopt},
            {"s/{T=1!A1}/{b}Header/", "s/{T=1!A1}/", "Edit cell A1 of table 1", SuggestionKind::Snippet, std::nullopt},
            {"@header:s/.*/CONFIDENTIAL/", "@header:s/", "Edit document header", SuggestionKind::Scope, std::nullopt},
        };
    }

    std::vector<SnippetOption> starters;
    if (ctx.extensions.has_value()) {
        starters.insert(starters.end(), ctx.extensions->starters.begin(), ctx.extensions->starters.end());
    }
    starters.insert(starters.end(), base_starters.begin(), base_starters.end());

    std::vector<Suggestion> filtered;
    for (const auto& s : starters) {
        if (seg.empty() || contains_ci(s.label, seg) || contains_ci(s.detail, seg)) {
            filtered.push_back(Suggestion{
                s.label, s.insert, cmd_start, safe_cursor, s.detail, s.kind, s.swatch
            });
        }
    }
    return IntelliSenseResult{
        "SEDMAT v3.7",
        {{"[@scope:][addr]", seg.empty()}, {"cmd/pattern/{flags}replacement/[g i m N]", false}},
        filtered,
        compute_ghost_suffix(input, safe_cursor, filtered),
    };
}

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    std::size_t i = 0;
    const std::size_t n = input.size();

    auto push = [&](TokenKind kind, std::size_t start, std::size_t end) {
        if (end > start) {
            tokens.push_back(Token{kind, start, end, input.substr(start, end - start)});
        }
    };

    while (i < n) {
        unsigned char uch = static_cast<unsigned char>(input[i]);
        if (std::isspace(uch)) {
            std::size_t s = i;
            while (i < n && std::isspace(static_cast<unsigned char>(input[i]))) ++i;
            push(TokenKind::Whitespace, s, i);
            continue;
        }
        if (input[i] == ';') {
            push(TokenKind::Delimiter, i, i + 1);
            ++i;
            continue;
        }
        if (input[i] == '#') {
            std::size_t s = i;
            std::size_t nl = input.find('\n', i);
            i = (nl != std::string::npos) ? nl : n;
            push(TokenKind::Comment, s, i);
            continue;
        }
        if (input[i] == '@') {
            std::size_t s = i;
            ++i;
            while (i < n && !std::isspace(static_cast<unsigned char>(input[i])) &&
                   input[i] != ';' && input[i] != '{' && input[i] != '/') {
                if (input[i] == '"') {
                    ++i;
                    while (i < n && input[i] != '"') ++i;
                    if (i < n) ++i;
                    continue;
                }
                if (input[i] == ':') {
                    ++i;
                    i += match_region_prefix(input.substr(i));
                    break;
                }
                if ((input[i] == 'd' || input[i] == 'a' || input[i] == 'i') &&
                    (i + 1 >= n || std::isspace(static_cast<unsigned char>(input[i + 1])) ||
                     input[i + 1] == ';' || input[i + 1] == '/')) {
                    break;
                }
                ++i;
            }
            push(TokenKind::Scope, s, i);
            continue;
        }
        if (input[i] == '$' || std::isdigit(uch)) {
            std::size_t s = i;
            while (i < n && (std::isdigit(static_cast<unsigned char>(input[i])) ||
                             input[i] == ',' || input[i] == '$' || input[i] == '!')) {
                ++i;
            }
            push(TokenKind::Address, s, i);
            continue;
        }
        if (input[i] == '{') {
            std::size_t s = i;
            std::size_t depth = 0;
            while (i < n) {
                if (input[i] == '\\' && i + 1 < n) {
                    i += 2;
                    continue;
                }
                if (input[i] == '{') {
                    ++depth;
                } else if (input[i] == '}') {
                    if (depth > 0) --depth;
                    ++i;
                    if (depth == 0) break;
                    continue;
                }
                ++i;
            }
            push(TokenKind::Brace, s, i);
            continue;
        }
        if (input[i] == 's' || input[i] == 'd' || input[i] == 'a' ||
            input[i] == 'c' || input[i] == 'i' || input[i] == 'y') {
            char cmd = input[i];
            push(TokenKind::Command, i, i + 1);
            ++i;
            if (i < n && !std::isspace(static_cast<unsigned char>(input[i])) && input[i] != ';') {
                char delim = input[i];
                push(TokenKind::Delimiter, i, i + 1);
                ++i;
                int expected_parts = (cmd == 's' || cmd == 'y') ? 2 : 1;
                for (int part = 0; part < expected_parts && i < n; ++part) {
                    std::size_t p_start = i;
                    while (i < n && input[i] != '\n') {
                        if (input[i] == '\\' && i + 1 < n) {
                            i += 2;
                            continue;
                        }
                        if (input[i] == delim) break;
                        if (part == 1 && input[i] == '{') {
                            push(TokenKind::Text, p_start, i);
                            std::size_t b_start = i;
                            std::size_t depth = 0;
                            while (i < n && input[i] != '\n') {
                                if (input[i] == '\\' && i + 1 < n) {
                                    i += 2;
                                    continue;
                                }
                                if (input[i] == '{') {
                                    ++depth;
                                } else if (input[i] == '}') {
                                    if (depth > 0) --depth;
                                    ++i;
                                    if (depth == 0) break;
                                    continue;
                                }
                                ++i;
                            }
                            push(TokenKind::Brace, b_start, i);
                            p_start = i;
                            continue;
                        }
                        ++i;
                    }
                    if (i > p_start) {
                        push(part == 0 ? TokenKind::Pattern : TokenKind::Text, p_start, i);
                    }
                    if (i < n && input[i] == delim) {
                        push(TokenKind::Delimiter, i, i + 1);
                        ++i;
                    }
                }
                std::size_t f_start = i;
                while (i < n && (input[i] == 'g' || input[i] == 'i' || input[i] == 'm' ||
                                 std::isdigit(static_cast<unsigned char>(input[i])))) {
                    ++i;
                }
                push(TokenKind::Flag, f_start, i);
            }
            continue;
        }

        push(TokenKind::Text, i, i + 1);
        ++i;
    }

    return tokens;
}

} // namespace sedmat
