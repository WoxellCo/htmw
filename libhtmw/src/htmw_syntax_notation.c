#include "htmw_syntax_notation.h"

htmw_syntax_literal txt_parse_syntax_literal(const htmw_context *ctx, const char *s, size_t max_len, const char *default_syntax_id, size_t default_syntax_id_length) {
    //debug_syntax_directives(ctx->syntax_routers);
    htmw_syntax_literal out = (htmw_syntax_literal){
        .content = nullstr,
        .explicit_syntax_id = nullstr,
        .total_length = 0,
        .flags = 0,
        .error = 0
    };

    if (s[0] != '#') {
        out.flags |= SYNTXN_ERROR;

        return out;
    }

    size_t i = 1;
    size_t esid_beginx_idx = 0;
    char c;

    uint8_t flags;
    syntaxer_result r;

    //string out_content = nullstr;

    while ((c = s[i]) != '\0' && i < max_len) {
        switch (c)
        {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
        case '{':
            if (/*!str_isnull(out.explicit_syntax_id)*/esid_beginx_idx > 0 && out.explicit_syntax_id.length == 0) {
                //out.explicit_syntax_id.c_str = s + i;
                out.explicit_syntax_id.length = i - esid_beginx_idx;
            } else {
                // `# js` (it must be rigt after the # or else it should be in auto check)
                out.flags |= SYNTXN_AUTO;
            }

            if (c != '{')
                break;

            if (out.flags & SYNTXN_ERROR)
                return out;

            if (str_isnull(out.explicit_syntax_id))
                out.flags |= SYNTXN_AUTO;

            r = syntaxer_analyze(
                ctx,
                s + i,
                SIZE_MAX,
                !(out.flags & SYNTXN_AUTO) ? syntaxer_list_find_by_id(
                    ctx->syntax_routers,
                    out.explicit_syntax_id.c_str,
                    out.explicit_syntax_id.length
                ) : syntaxer_list_find_by_id(
                    ctx->syntax_routers,
                    default_syntax_id,
                    default_syntax_id_length
                ),
                (syntaxer_analyze_input_data){
                    .shift = 1,
                    .in_token = {
                        .c_str = "{",
                        .length = 1,
                        .size = 0
                    },
                    .out_token_length = 1,
                    .flags = SYNTXF_NEWLINE
                }
            );

            if (r.error.type) { // TODO: implement a better error handler
                char *msg = (char *)malloc(256 * sizeof(char)); // TODO: handle allocation error
                sprintf(msg, "there was an error in the syntaxer (entry syntax: `%.*s`):\n\ttype: %u,\n\t%s,\n\t%s", out.flags & SYNTXN_AUTO ? default_syntax_id_length : out.explicit_syntax_id.length, out.flags & SYNTXN_AUTO ? default_syntax_id : out.explicit_syntax_id.c_str, r.error.type, r.error.message, r.error.ext_message);
                throw_error(msg, E_EXIT);
            }

            out.total_length = i + r.n;

            out.content.c_str = s + i + 1;
            out.content.length = r.n - 2;

            goto txt_parse_syntax_literal_exit;
        default:
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                if (str_isnull(out.explicit_syntax_id)) {
                    if (out.flags & SYNTXN_AUTO) {
                        out.flags |= SYNTXN_ERROR;
                        out.error = syntxn_et_expected_opening;

                        out.total_length = i;
                    }
                    out.explicit_syntax_id.c_str = s + i;
                    esid_beginx_idx = i;
                } else if (out.explicit_syntax_id.length > 0) {
                    // that means it already counted the length: meaning it already reached a white space (would be a syntax error)
                    out.flags |= SYNTXN_ERROR;
                    out.error = syntxn_et_expected_opening;

                    out.total_length = i;

                    return out;
                }
            }
            break;
        }

        ++i;
    }

    txt_parse_syntax_literal_exit:

    //str_debug(out.content);
    //printf("TOTAL_LENGTH: %zu\n", out.total_length);

    return out;
}