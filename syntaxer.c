#include "syntaxer.h"

static int syntaxer_sort_cmp(const void* x, const void* y) {
    size_t lx = strlenw(((syntaxer_neutralizer*)x)->begin);
    size_t ly = strlenw(((syntaxer_neutralizer*)y)->begin);

    return lx > ly ? 1 : lx < ly ? -1 : 0;
}

size_t syntaxer_select_until(syntaxer_neutralizer* neutralizers, size_t neutralizers_count, const charw* s, charw* begin_sequence, charw* end_sequence, size_t max_len) {
    size_t begin_sequence_len = strlenw(begin_sequence);
    size_t end_sequence_len = strlenw(end_sequence);
    charw c;
    size_t i;
    size_t neutral = 0;
    flag_t f = 0;
    size_t nest = 0;

    size_t* n_begin_lens = (size_t*)malloc(neutralizers_count * sizeof(size_t));
    size_t* n_end_lens = (size_t*)malloc(neutralizers_count * sizeof(size_t));
    size_t* n_esc_char_lens = (size_t*)malloc(neutralizers_count * sizeof(size_t));
    size_t* n_esc_raw_lens = (size_t*)malloc(neutralizers_count * sizeof(size_t));

    for (i = 0; i < neutralizers_count; i++) {
        if ((neutralizers + i) == NULL) {
            neutralizers_count = i;
            break;
        }

        n_begin_lens[i] = strlenw(neutralizers->begin);
        n_end_lens[i] = strlenw(neutralizers->end);
        n_esc_char_lens[i] = strlenw(neutralizers->esc_char);
        n_esc_raw_lens[i] = strlenw(neutralizers->esc_raw);
    }
    for (i = 0; (c = s[i]) != L'\0' && i < max_len; i++) {
        //printf("(%zu; %zu; '%lc')\n", i, neutral, c);
        if (!neutral) {
            if (!strncmpw(begin_sequence, s + i, begin_sequence_len)) {
                nest++;
                i += begin_sequence_len - 1;
            } else if (!strncmpw(end_sequence, s + i, end_sequence_len)) {
                if (nest > 0) {
                    nest--;
                    i += end_sequence_len - 1;
                } else {
                    //printf("(%zu; %zu)\n", i, 99);
                    return i;
                }
            } else for (size_t j = 0; j < neutralizers_count; j++) {
                if (!strncmpw(neutralizers[j].begin, s + i, n_begin_lens[j])) {
                    i += n_begin_lens[j] - 1;
                    neutral = j + 1;
                    //break;
                    continue;
                }
            }
        } else {
            unsigned char esc_mode_enabled = 0;
            size_t old_i = i;
            size_t old_n = neutral;
            if (flag_excludes(f, SYNTXF_EXE_ESC_CHAR)) {
                // if escape char is disabled
                if (neutralizers[neutral - 1].end != NULL && !strncmpw(neutralizers[neutral - 1].end, s + i, n_end_lens[neutral - 1])) {
                    // ending string or comment
                    i += n_end_lens[neutral - 1] - 1;
                    neutral = 0;
                    continue;//break;
                } else if (neutralizers[neutral - 1].esc_char != NULL && !strncmpw(neutralizers[neutral - 1].esc_char, s + i, n_esc_char_lens[neutral - 1])) {
                    // escape char placed \ <--
                    f |= SYNTXF_EXE_ESC_CHAR;
                    i += n_esc_char_lens[neutral - 1] - 1;
                } if (flag_includes(f, SYNTXF_EXE_ESC_RAW)) {
                    // if the escape raw char flag is enabled
                    // x\
                        a
                    f ^= SYNTXF_EXE_ESC_RAW;
                } else if (c == L'\n' && flag_excludes(neutralizers[neutral - 1].flags, SYNTXF_DEF_N_ML)) {
                    // if the escape raw char is disabled and the block is not multiline
                    neutral = 0;
                } else if (neutralizers[neutral - 1].esc_raw != NULL && !strncmpw(neutralizers[old_n - 1].esc_raw, s + old_i, n_esc_raw_lens[old_n - 1])) {
                    // if the raw escape char is placed => enable flag
                    f |= SYNTXF_EXE_ESC_CHAR;
                    i += n_esc_char_lens[old_n - 1] - 1;
                }// else if (flag_includes(f, SYNTXF_DEF_NB_ESC_IMMUNE) && !strncmpw(neutralizers[neutral - 1]., ,))
            } else {
                // if escape char is enabled
                if (flag_includes(f, SYNTXF_EXE_ESC_RAW)) {
                    // disable escape after 1 char: if escape raw char is enabled => disable it
                    f ^= SYNTXF_EXE_ESC_RAW;
                } else if (c == L'\n' && flag_excludes(neutralizers[neutral - 1].flags, SYNTXF_DEF_N_ML)) {
                    // else if the line breaks and it's not multline => disable neutral block
                    neutral = 0;
                }// else if ()

                f ^= SYNTXF_EXE_ESC_CHAR;
            }
        }
    }
    return i;
}

// macro?
void syntaxer_sort(syntaxer_neutralizer* neutralizers, size_t neutralizers_count) {
    qsort(neutralizers, neutralizers_count, sizeof(syntaxer_neutralizer), syntaxer_sort_cmp);
}