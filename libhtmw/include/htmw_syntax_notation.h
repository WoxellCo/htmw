#ifndef SYNTAX_NOTATION_H
#define SYNTAX_NOTATION_H

#include <stdint.h>
#include "str.h"
#include "syntaxer.h"

#include "htmw.h" // TEMPORARY

#define SYNTXN_ERROR        (1 << 0)
#define SYNTXN_AUTO         (1 << 1)

enum syntax_literal_error {
    syntxn_et_expected_opening,
    syntxn_et_expected_hash,
    syntxn_et_expected_identifier,
    syntxn_et_unclosed,
};

typedef struct {
    string explicit_syntax_id;
    size_t total_length;
    string content;
    uint8_t flags;
    uint8_t error;
} htmw_syntax_literal;

htmw_syntax_literal txt_parse_syntax_literal(const htmw_context *ctx, const char *s, size_t max_len, const char *default_syntax_id, size_t default_syntax_id_length);

#endif