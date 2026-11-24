#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace sedmat {

class SedmatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class Command {
    Substitute,
    Delete,
    Append,
    Insert,
    Transliterate,
    Brace
};

std::string command_name(Command cmd);

struct Address {
    int start = 0;       // 1-based, or -1 for '$'
    int end = 0;         // 1-based, -1 for '$', or 0 for single address
    bool has_range = false;
};

struct ContainerScope {
    std::optional<std::string> container_type;
    std::optional<Address> address;
    bool all = false;
    std::optional<std::string> name;
    std::optional<std::string> region;
    bool container_op = false;
};

struct InlineSpan {
    std::string flag;
    std::string text;
};

struct BraceExpression {
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<bool> underline;
    std::optional<bool> strike;
    std::optional<bool> code;
    std::optional<bool> sup;
    std::optional<bool> sub;
    std::optional<bool> small_caps;

    std::optional<std::string> text;
    std::optional<std::string> color;
    std::optional<std::string> bg;
    std::optional<std::string> font;
    std::optional<double> size;
    std::optional<std::string> url;
    std::optional<std::string> heading;
    std::optional<double> leading;
    std::optional<std::string> align;
    std::optional<double> opacity;
    std::optional<int> indent;
    std::optional<double> kerning;
    std::optional<double> width;
    std::optional<double> height;
    bool spacing_set = false;
    double spacing_above = 0.0;
    double spacing_below = 0.0;
    std::optional<std::string> effect;
    std::optional<int> cols;

    bool reset = false;
    bool no_reset = false;
    bool has_break = false;
    std::string break_kind;
    std::optional<std::string> comment;
    std::optional<std::string> bookmark;
    std::optional<bool> check;
    bool has_toc = false;
    int toc = 0;
    std::optional<std::string> img_ref;
    std::optional<std::string> table_ref;

    // v3.7 slide & shape flags
    std::optional<std::string> layout;
    std::optional<std::string> zone;
    std::optional<std::string> shape;
    std::optional<std::string> fill;
    std::optional<std::string> stroke;
    std::optional<std::string> notes;
    std::optional<std::string> transition;
    std::optional<int> move_to;
    bool dup = false;
    bool hide = false;

    std::vector<InlineSpan> inline_spans;
};

struct BraceSpan {
    std::size_t start = 0;
    std::size_t end = 0;
    bool is_global = false;
    BraceExpression expr;
    std::string raw;
};

struct TableCreateSpec {
    std::size_t rows = 0;
    std::size_t columns = 0;
    bool header = false;
};

struct Expression {
    Command command = Command::Substitute;
    std::string pattern;
    std::string replacement;
    bool global = false;
    std::size_t nth_match = 0;
    std::string flags;
    std::optional<Address> address;
    std::optional<ContainerScope> container;
    std::optional<BraceExpression> brace;
    std::vector<BraceSpan> brace_spans;
    std::optional<TableCreateSpec> table_create;
    bool enriched = false;
};

struct Program {
    std::vector<Expression> expressions;
};

// --- Document Projection ---

enum class BlockKind {
    Paragraph,
    Table,
    TableOfContents,
    SectionBreak
};

struct DocumentBlock {
    BlockKind kind = BlockKind::Paragraph;
    std::size_t start_index = 0;
    std::size_t end_index = 0;
    int item_index = -1;
};

struct DocumentParagraph {
    std::string text;
    std::size_t start_index = 0;
    std::size_t end_index = 0;
    std::string named_style = "NORMAL_TEXT";
};

struct DocumentSegment {
    std::string tab_id;
    std::string title;
    std::size_t body_start_index = 1;
    std::size_t body_end_index = 1;
    std::vector<DocumentBlock> blocks;
    std::vector<DocumentParagraph> paragraphs;
};

struct DocumentProjection {
    std::string document_id;
    std::string revision_id;
    std::optional<DocumentSegment> legacy;
    std::vector<DocumentSegment> tabs;
};

struct AddressElement {
    std::size_t number = 0;
    std::string kind;
    std::string text;
    std::size_t start_index = 0;
    std::size_t end_index = 0;
};

struct AddressMutation {
    std::size_t start_index = 0;
    std::size_t end_index = 0;
    std::optional<std::string> insert_text;
};

struct TextEdit {
    std::size_t start_index = 0;
    std::size_t end_index = 0;
    std::string insert_text;
};

struct FormatIntent {
    std::size_t start_index = 0;
    std::size_t end_index = 0;
    std::optional<BraceExpression> brace;
};

struct TextPlan {
    std::size_t match_count = 0;
    std::vector<TextEdit> text_edits;
    std::vector<FormatIntent> formatting;
};

struct ExecuteOptions {
    bool dry_run = false;
    std::string tab_id;
};

struct ExecutionReport {
    bool dry_run = false;
    std::size_t total_matches = 0;
};

// --- Core API ---

std::string resolve_color(const std::string& value);
BraceExpression parse_brace_expression(const std::string& raw);
std::pair<std::string, std::vector<BraceSpan>> parse_brace_replacement(const std::string& replacement);
bool has_brace_formatting(const std::string& replacement);

Expression parse_expression(const std::string& raw);
Program parse(const std::string& raw);
Program enrich(Program program);
Program parse_program(const std::string& script);

std::vector<AddressElement> address_elements(const DocumentSegment& seg);
const DocumentSegment& select_segment(const DocumentProjection& proj, const std::string& tab_id = "");
std::string format_structure(const DocumentSegment& seg);
std::string format_numbered(const DocumentSegment& seg);

std::vector<AddressElement> resolve_address(const Address& addr, const std::vector<AddressElement>& elements);
std::vector<AddressMutation> plan_addressed_delete(
    const std::vector<AddressElement>& elements,
    const std::vector<AddressElement>& targets);
std::vector<AddressMutation> plan_addressed_insert(
    const std::vector<AddressElement>& targets,
    const std::string& replacement);
std::vector<AddressMutation> plan_addressed_append(
    const std::vector<AddressElement>& targets,
    const std::string& replacement);
TextPlan plan_paragraph_substitutions(const std::vector<DocumentParagraph>& paragraphs, const Expression& expr);

// --- Backend & Memory Document ---

class Backend {
public:
    virtual ~Backend() = default;
    virtual DocumentProjection project() const = 0;
    virtual void apply_text_plan(const std::string& tab_id, const TextPlan& plan) = 0;
    virtual void apply_address_mutations(const std::string& tab_id, const std::vector<AddressMutation>& mutations) = 0;
    virtual std::size_t apply_transliterate(
        const std::string& tab_id,
        const std::string& source,
        const std::string& dest,
        const std::vector<AddressElement>& targets) = 0;
    virtual std::size_t apply_container_op(const ContainerScope&, const Expression&) { return 0; }
};

struct MemoryParagraph {
    std::string text;
    std::string named_style = "NORMAL_TEXT";
};

struct MemoryTab {
    std::string id;
    std::string title = "Default";
    std::optional<std::string> background;
    std::optional<std::string> notes;
    std::optional<std::string> transition;
    std::optional<std::string> layout;
    bool hidden = false;
    std::vector<MemoryParagraph> paragraphs;
    std::vector<FormatIntent> formatting;
};

class MemoryDocument final : public Backend {
public:
    std::string id = "mem-doc-1";
    std::string revision_id = "rev-1";
    std::vector<MemoryTab> tabs;

    explicit MemoryDocument(std::vector<std::string> paragraphs = {});
    void add_tab(const std::string& tab_id, const std::string& title, std::vector<std::string> paragraphs = {});
    std::vector<std::string> paragraph_texts(const std::string& tab_id = "") const;

    DocumentProjection project() const override;
    void apply_text_plan(const std::string& tab_id, const TextPlan& plan) override;
    void apply_address_mutations(const std::string& tab_id, const std::vector<AddressMutation>& mutations) override;
    std::size_t apply_transliterate(
        const std::string& tab_id,
        const std::string& source,
        const std::string& dest,
        const std::vector<AddressElement>& targets) override;
    std::size_t apply_container_op(const ContainerScope& scope, const Expression& expr) override;

private:
    const MemoryTab& find_tab(const std::string& tab_id) const;
    MemoryTab& find_tab_mut(const std::string& tab_id);
    std::vector<std::size_t> resolve_container_indices(const ContainerScope& scope) const;
};

class Executor {
public:
    explicit Executor(Backend& backend) : backend_(backend) {}
    ExecutionReport execute_string(const std::string& script, const ExecuteOptions& opts = {});
    ExecutionReport execute(Program program, const ExecuteOptions& opts = {});

private:
    Backend& backend_;
};

} // namespace sedmat
