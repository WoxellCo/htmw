#include <htmw.h>

// starting point

int main(int argc, char** argv) {
    setlocale(LC_ALL, "en_US.UTF-8");
#if DEBUG_MODE
#ifdef _WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif
#endif
    config_t config = (config_t){
        .origin_name = NULL,
        .output_name = NULL,
        .format = NULL,
        .replica_mode = 0,
        .enable_network = 0,
        .unsafe_mode = 0
    };

    int show_ver = 0;   // flag: show version if 1

    list* global_include_paths = list_new();

#ifdef __unix__
    string global_include_buffer = str_from(GLOBAL_INCLUDE_PATH);
    list_insert(global_include_paths, 0, (void*)&global_include_buffer);
#endif

    for (int i = 1; i < argc; i++) {
        //printf("%d\t\"%s\"\n", i, argv[i]);
        //puts("-------");
        if (argv[i][0] == '-') {
            switch (argv[i][1])
            {
            case '-':
                if (!strcmp(argv[i] + 2, "format")) {
                    if (i + 1 >= argc) {
                        throw_error("Format config error", E_EXIT);
                    }
                    if (!strcmp(argv[i + 1], "auto")) {
                        config.format = NULL;
                        i++;
                    } else {
                        config.format = argv[++i];
                    }
                } else if (!strcmp(argv[i] + 2, "recursive")) {throw_error("Recursive/Replica mode is not supported yet", E_EXIT);
                    if (config.output_name) {
                        throw_error("Output mode already configured", E_EXIT);
                    }
                    if (i + 1 >= argc) {
                        throw_error("Output mode config error", E_EXIT);
                    }
                    config.output_name = argv[++i];
                } else if (!strcmp(argv[i] + 2, "replica")) {throw_error("Recursive/Replica mode is not supported yet", E_EXIT);
                    config.replica_mode = 1;
                    if (config.output_name) {
                        throw_error("Output mode already configured", E_EXIT);
                    }
                    if (i + 1 >= argc) {
                        throw_error("Output mode config error", E_EXIT);
                    }
                    config.output_name = argv[++i];
                } else if (!strcmp(argv[i] + 2, "unsafe")) {
                    config.unsafe_mode = 1;
                }
                break;
            case 'f':
                if (i + 1 >= argc) {
                    throw_error("Format config error", E_EXIT);
                }
                if (!strcmp(argv[i + 1], "auto")) {
                    config.format = NULL;
                    i++;
                } else {
                    config.format = argv[++i];
                }
                break;
            case 'R':
                config.replica_mode = 1;
            case 'r':throw_error("Recursive/Replica mode is not supported yet", E_EXIT);
                if (config.output_name) {
                    throw_error("Output mode already configured", E_EXIT);
                }
                if (i + 1 >= argc) {
                    throw_error("Output mode config error", E_EXIT);
                }
                config.output_name = argv[++i];
                break;
            case 'v':
                printf("HTMW vT.7\nWoxell\n\n");
                show_ver = 1;
                break;
            case 'I': {
                    string *s = (string*)malloc(sizeof(string));
                    *s = str_from(argv[i] + 2);
                    list_insert(global_include_paths, 0, (void*)s);
                } break;
            case 'N':
                config.enable_network = 1;
                break;
            default:
                break;
            }
        } else {
            if (config.origin_name == NULL) {
                config.origin_name = argv[i];
            } else {
                throw_error("Input file(s) already specified", E_EXIT);
            }
        }
    }

    if (config.origin_name == NULL) {
        if (show_ver) return 0;
        throw_error("No input file(s)", E_EXIT);
    }

    char* in_name = (char*)malloc((strlen(config.origin_name) + 6) * sizeof(char));
    sprintf(in_name, "%s.%s", config.origin_name, "htmw");

    char* out_name = (char*)malloc((strlen(config.origin_name) + (config.format ? strlen(config.format) : 4) + 2) * sizeof(char));
    sprintf(out_name, "%s.%s", config.origin_name, (config.format ? config.format : "html"));

    string in = read_file(in_name, 0);
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    if (config.enable_network && !config.unsafe_mode) {
        htmw_remove_unsafe(L);
        printf("[HTMW Warning]: Some features have been disabled to prevent unsafe execution from HTTP sources, use the `--unsafe` flag to enable all the features\n");
    }

    /// create stack allocated context ///

    luax_context luax_ctx;
    syntaxer_list syntax_routers;
    compiled_regex_vector crv;
    size_t crv_size;

    htmw_context ctx = nullctx;
    ctx.luax_ctx = &luax_ctx;
    ctx.syntax_routers = &syntax_routers;
    ctx.crv = &crv;
    ctx.crv_size = &crv_size;

    htmw_preprocess(L, in, &ctx, global_include_paths, config.enable_network);
    stack* ctx_stack = stack_new();

    stack_push(ctx_stack, (void*)&ctx);

    //ctx.global_paths = global_include_paths;
    luaw_ctx = &ctx;
    htmw_prefetch_syntax_drectives(ctx.syntax_routers);
    //luax_test(&ctx);
    //debug_templates(&ctx);

    txt_template_fill_data tfd = htmw_process_txt(&ctx);
    //str_debug(tfd.ground);
    //printf("_%zu\n", tfd.fields_count);

    //strplex_print(&tfd.args);
    string preout = htmw_compile_txt(&ctx, &tfd);

    string out = htmw_postprocess(preout);
    //str_delete(preout);

    //struct regex_analyze_literal_s rl = regex_analyze_literal("/hello [[/]] world/gsx", 0);
    lua_close(L);
    FILE* out_stream = fopen(out_name, "w");
#if WCHAR_ENABLED
    fprintf(out_stream, "%ls", out.c_str);
#else
    fprintf(out_stream, "%s", out.c_str);
#endif
    fclose(out_stream);
    free(in_name);
    free(out_name);
#if DEBUG_MODE
    //printf("%ls\n", ctx.input.c_str);
#endif
//printf("NEXTGEN: %d\n", NEW_TXT_PROCESSING);
}