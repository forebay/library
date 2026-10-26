#include "fixtures.h"

#define ENTRY(key, label) "\"" key "\":{\"label\":\"" label "\",\"home\":{\"candidates\":[\"/x\"]}}"

static void expect_empty(support_answer answer, const char *label) {
    expect_json(&answer, 0, "{\"apps\":[]}", label);
}

static void fails_the_parse_on_a_bom_as_json_parse_does(void) {
    expect_empty(list_text("\xEF\xBB\xBF{" ENTRY("k", "K") "}"), "bom before the object");
}

static void strips_a_bom_that_leads_a_comment_line(void) {
    support_answer answer = list_text("\xEF\xBB\xBF// registry\n{" ENTRY("k", "K") "}");
    expect_ids(&answer, "k", "bom eaten by the comment's whitespace");
}

static void strips_whole_line_comments(void) {
    support_answer closed = list_text("// head\n{\n    // indented\n" ENTRY("a", "A") ",\n\t// tabbed\n" ENTRY("b", "B")
                                      "\n}\n// tail");
    expect_ids(&closed, "a,b", "whole-line comments stripped");
}

static void keeps_a_comment_that_follows_content_on_its_line(void) {
    expect_empty(list_text("{" ENTRY("k", "K") "} // trailing"), "a trailing comment is not stripped");
}

static void keeps_slashes_inside_a_string(void) {
    support_answer answer = list_text("{\"k\":{\"label\":\"http://x\",\"home\":{\"candidates\":[\"//share\"]}}}");
    expect_at(&answer, "apps.0.label", "\"http://x\"", "slashes in a label");
    support_discard(&answer);
}

static void strips_comments_in_a_crlf_registry(void) {
    support_answer answer = list_text("{\r\n  // one\r\n  " ENTRY("k", "K") ",\r\n  // two\r\n  " ENTRY("j", "J") "\r\n}\r\n");
    expect_ids(&answer, "k,j", "crlf comments");
}

static void starts_a_comment_line_after_a_lone_cr(void) {
    support_answer answer = list_text("{" ENTRY("k", "K") ",\r// gone\n" ENTRY("j", "J") "}");
    expect_ids(&answer, "k,j", "a cr starts a line");
}

static void starts_a_comment_line_after_a_line_separator_inside_a_string(void) {
    expect_empty(list_text("{\"k\":{\"label\":\"a\xE2\x80\xA8// cut\",\"home\":{\"candidates\":[\"/x\"]}}}"),
                 "u+2028 then // cuts the string short");
}

static void keeps_a_line_separator_inside_a_string(void) {
    support_answer answer = list_text("{\"k\":{\"label\":\"a\xE2\x80\xA8z\",\"home\":{\"candidates\":[\"/x\"]}}}");
    expect_at(&answer, "apps.0.label", "\"a\\u2028z\"", "raw u+2028 in a string");
    support_discard(&answer);
}

static void strips_a_comment_led_by_javascript_whitespace(void) {
    support_answer answer = list_text("{\n\xC2\xA0\xE3\x80\x80// wide\n" ENTRY("k", "K") "}");
    expect_ids(&answer, "k", "no-break and ideographic spaces before a comment");
    support_answer ranged = list_text("{\n\xE2\x80\x83\xEF\xBB\xBF// ranged\n" ENTRY("k", "K") "}");
    expect_ids(&ranged, "k", "a space from U+2000 to U+200A and a BOM before a comment");
}

static void refuses_whitespace_json_parse_does_not_know(void) {
    expect_empty(list_text("\v{" ENTRY("k", "K") "}"), "vertical tab");
    expect_empty(list_text("{\f" ENTRY("k", "K") "}"), "form feed");
    expect_empty(list_text("{\xC2\xA0" ENTRY("k", "K") "}"), "no-break space");
    expect_empty(list_text("{\x01" ENTRY("k", "K") "}"), "a control character");
}

static void reads_a_comment_holding_invalid_utf8_or_nul(void) {
    static const char registry[] = "// \xFF\xC3 and a \0 nul\n{" ENTRY("k", "K") "}";
    support_answer answer = list_bytes(registry, sizeof(registry) - 1);
    expect_ids(&answer, "k", "comment bytes are stripped first");
}

static void refuses_a_raw_nul_outside_a_comment(void) {
    static const char registry[] = "{" ENTRY("k", "K") "}\0";
    expect_empty(list_bytes(registry, sizeof(registry) - 1), "a trailing raw nul");
}

static void replaces_invalid_utf8_as_node_decodes_it(void) {
    support_answer answer = list_text(
        "{\"a\":{\"label\":\"a\xC3(b\",\"home\":{\"candidates\":[\"/x\"]}},"
        "\"b\":{\"label\":\"\xE0\x80z\",\"home\":{\"candidates\":[\"/x\"]}},"
        "\"c\":{\"label\":\"\xED\xA0\x80\",\"home\":{\"candidates\":[\"/x\"]}},"
        "\"d\":{\"label\":\"\xF0\x9F\x98\",\"home\":{\"candidates\":[\"/x\"]}},"
        "\"e\":{\"label\":\"\xFF\xFE\",\"home\":{\"candidates\":[\"/x\"]}},"
        "\"f\":{\"label\":\"\xE2\x82\",\"home\":{\"candidates\":[\"/x\"]}}}");
    expect_at(&answer, "apps.0.label", "\"a\\ufffd(b\"", "a truncated two-byte sequence");
    expect_at(&answer, "apps.1.label", "\"\\ufffd\\ufffdz\"", "an overlong lead and its continuation");
    expect_at(&answer, "apps.2.label", "\"\\ufffd\\ufffd\\ufffd\"", "an encoded surrogate");
    expect_at(&answer, "apps.3.label", "\"\\ufffd\"", "a truncated four-byte sequence");
    expect_at(&answer, "apps.4.label", "\"\\ufffd\\ufffd\"", "bytes that never lead");
    expect_at(&answer, "apps.5.label", "\"\\ufffd\"", "a truncated three-byte sequence");
    support_discard(&answer);
}

static void refuses_numbers_json_parse_refuses(void) {
    expect_empty(list_text("{\"k\":{\"label\":\"K\",\"home\":{\"candidates\":[\"/x\"]},\"proxyPort\":01}}"), "leading zero");
    expect_empty(list_text("{\"k\":{\"label\":\"K\",\"home\":{\"candidates\":[\"/x\"]},\"proxyPort\":1.}}"), "bare point");
    expect_empty(list_text("{\"k\":{\"label\":\"K\",\"home\":{\"candidates\":[\"/x\"]},\"proxyPort\":-}}"), "bare minus");
    expect_empty(list_text("{\"k\":{\"label\":\"K\",\"home\":{\"candidates\":[\"/x\"]},\"proxyPort\":1e}}"), "bare exponent");
}

static void reads_numbers_json_parse_reads(void) {
    support_answer answer = list_text("{\"k\":{\"label\":\"K\",\"home\":{\"candidates\":[\"/x\"]},\"proxyPort\":1.5E+3,"
                                      "\"usage\":[-0,0.25,1e-2]}}");
    expect_at(&answer, "apps.0.proxyPort", "1500", "an exponent");
    expect_at(&answer, "apps.0.usage", "[0,0.25,0.01]", "fractions");
    support_discard(&answer);
}

static void refuses_a_raw_control_character_inside_a_string(void) {
    expect_empty(list_text("{\"k\":{\"label\":\"a\tb\",\"home\":{\"candidates\":[\"/x\"]}}}"), "raw tab");
    expect_empty(list_text("{\"k\":{\"label\":\"a\x1f" "b\",\"home\":{\"candidates\":[\"/x\"]}}}"), "raw unit separator");
}

static void refuses_an_escape_json_parse_refuses(void) {
    expect_empty(list_text("{\"k\":{\"label\":\"a\\'b\",\"home\":{\"candidates\":[\"/x\"]}}}"), "escaped quote");
    expect_empty(list_text("{\"k\":{\"label\":\"\\u12\",\"home\":{\"candidates\":[\"/x\"]}}}"), "short unicode escape");
}

static void reads_an_unpaired_surrogate_escape_as_a_replacement_character(void) {
    support_answer answer = list_text("{\"k\":{\"label\":\"\\ud800\",\"home\":{\"candidates\":[\"/x\"]}}}");
    expect_at(&answer, "apps.0.label", "\"\\ufffd\"", "unpaired surrogate");
    support_discard(&answer);
}

/** A divergence: JSON.parse reads an escaped NUL, but no C string can carry one, so the registry reads as empty. */
static void answers_an_empty_list_for_an_escaped_nul_basekit_would_read(void) {
    expect_empty(list_text("{" ENTRY("k", "K") ",\"j\":{\"label\":\"a\\u0000b\",\"home\":{\"candidates\":[\"/x\"]}}}"),
                 "escaped nul");
}

static void refuses_anything_after_the_registry(void) {
    expect_empty(list_text("{" ENTRY("k", "K") "} {}"), "a second value");
    expect_empty(list_text("{" ENTRY("k", "K") ",}"), "a trailing comma");
}

int main(void) {
    fails_the_parse_on_a_bom_as_json_parse_does();
    strips_a_bom_that_leads_a_comment_line();
    strips_whole_line_comments();
    keeps_a_comment_that_follows_content_on_its_line();
    keeps_slashes_inside_a_string();
    strips_comments_in_a_crlf_registry();
    starts_a_comment_line_after_a_lone_cr();
    starts_a_comment_line_after_a_line_separator_inside_a_string();
    keeps_a_line_separator_inside_a_string();
    strips_a_comment_led_by_javascript_whitespace();
    refuses_whitespace_json_parse_does_not_know();
    reads_a_comment_holding_invalid_utf8_or_nul();
    refuses_a_raw_nul_outside_a_comment();
    replaces_invalid_utf8_as_node_decodes_it();
    refuses_numbers_json_parse_refuses();
    reads_numbers_json_parse_reads();
    refuses_a_raw_control_character_inside_a_string();
    refuses_an_escape_json_parse_refuses();
    reads_an_unpaired_surrogate_escape_as_a_replacement_character();
    answers_an_empty_list_for_an_escaped_nul_basekit_would_read();
    refuses_anything_after_the_registry();
    return support_summary("text_test");
}
