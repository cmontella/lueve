#include <runtime.h>
#include <http/http.h>
#include <bswap.h>
#include <luanne.h>

static boolean enable_tracing = false;
static buffer loadedParse;
static int port = 8080;
#define register(__h, __url, __content, __name)\
 {\
    extern unsigned char __name##_start, __name##_end;\
    unsigned char *s = &__name##_start, *e = &__name##_end;\
    register_static_content(__h, __url, __content, wrap_buffer(init, s, e-s), dynamicReload?(char *)e:0); \
 }

int atoi( const char *str );

station create_station(unsigned int address, unsigned short port) {
    void *a = allocate(init,6);
    unsigned short p = htons(port);
    memset(a, 0, 6);
    memcpy (a+4, &p, 2);
    return(a);
}


extern void init_json_service(http_server, uuid, boolean, buffer);
extern int strcmp(const char *, const char *);
static buffer read_file_or_exit(heap, char *);

extern void *ignore;

static CONTINUATION_1_2(test_result, heap, table, table);
static void test_result(heap h, table s, table c)
{
    table_foreach(s, n, v) {
        prf("%v %b\n", n, bag_dump(h, v));
    }
    destroy(h);
}

static void run_test(bag root, buffer b, boolean tracing)
{
    heap h = allocate_rolling(pages, sstring("command line"));
    bag event = create_bag(h, generate_uuid());
    table scopes = create_value_table(h);
    table results = create_value_vector_table(h);
    table_set(scopes, intern_cstring("all"), root);
    table_set(scopes, intern_cstring("session"), event);
    table_set(scopes, intern_cstring("transient"), event);

    buffer desc;
    vector n = compile_eve(h, b, tracing, &desc);
    vector_foreach(n, i)
        edb_register_implication(event, i);
    table persisted = create_value_table(h);
    evaluation ev = build_evaluation(scopes, persisted, cont(h, test_result, h));
    run_solver(ev);
    destroy(h);
}



typedef struct command {
    char *single, *extended, *help;
    boolean argument;
    void (*f)(interpreter, char *, bag);
} *command;

static void do_port(interpreter c, char *x, bag b)
{
    port = atoi(x);
}

static void do_tracing(interpreter c, char *x, bag b)
{
    enable_tracing = true;
}

static void do_parse(interpreter c, char *x, bag b)
{
    lua_run_module_func(c, read_file_or_exit(init, x), "parser", "printParse");
}

static void do_analyze(interpreter c, char *x, bag b)
{
    lua_run_module_func(c, read_file_or_exit(init, x), "compiler", "analyzeQuiet");
}

static void do_run_test(interpreter c, char *x, bag b)
{
    buffer f = read_file_or_exit(init, x);
    run_test(b, f, enable_tracing);
}

static void do_exec(interpreter c, char *x, bag b)
{
    buffer desc;
    buffer f = read_file_or_exit(init, x);
    vector v = compile_eve(init, f, enable_tracing, &loadedParse);
    vector_foreach(v, i) {
        edb_register_implication(b, i);
    }
}

static command commands;

static void print_help(interpreter c, char *x, bag b)
{
    for (command c = commands; *c->single; c++) {
        prf("-%s --%s %s\n", c->single, c->extended, c->help);
    }
}

static struct command command_body[] = {
    {"p", "parse", "parse and print structure", true, do_parse},
    {"a", "analyze", "parse order print structure", true, do_analyze},
    //    {"r", "run", "execute eve", true, do_run_test},
    //    {"s", "serve", "serve urls from the given root path", true, 0},
    {"e", "exec", "use eve as default path", true, do_exec},
    {"P", "port", "serve http on passed port", true, do_port},
    {"h", "help", "print help", false, print_help},
    {"t", "tracing", "enable per-statement tracing", false, do_tracing},
    //    {"R", "resolve", "implication resolver", false, 0},
};

int main(int argc, char **argv)
{
    init_runtime();
    bag root = create_bag(init, generate_uuid());
    boolean enable_tracing = false;
    interpreter interp = build_lua();
    commands = command_body;
    boolean dynamicReload = true;
    
    char * file = "";
    for (int i = 1; i < argc ; i++) {
        command c = 0;
        for (int j = 0; !c &&(j < sizeof(command_body)/sizeof(struct command)); j++) {
            command d = &commands[j];
            if (argv[i][0] == '-') {
                if (argv[i][1] == '-') {
                    if (!strcmp(argv[i]+2, d->extended)) c = d;
                } else {
                    if (!strcmp(argv[i]+1, d->single)) c = d;
                }
            }
        }
        if (c) {
            c->f(interp, argv[i+1], root);
            if (c->argument) i++;
        } else {
            do_exec(interp, argv[i], root);
            // prf("\nUnknown flag %s, aborting\n", argv[i]);
            // exit(-1);
        }
    }
    
    http_server h = create_http_server(create_station(0, port));
    register(h, "/", "text/html", index);
    register(h, "/jssrc/renderer.js", "application/javascript", renderer);
    register(h, "/jssrc/microReact.js", "application/javascript", microReact);
    register(h, "/jssrc/codemirror.js", "application/javascript", codemirror);
    register(h, "/jssrc/codemirror.css", "text/css", codemirrorCss);
    register(h, "/examples/todomvc.css", "text/css", exampleTodomvcCss);

    // TODO: figure out a better way to manage multiple graphs
    init_json_service(h, root, enable_tracing, loadedParse);

    init_request_service(root);

    prf("\n----------------------------------------------\n\nEve started. Running at http://localhost:%d\n\n",port);
    unix_wait();
}

buffer read_file_or_exit(heap h, char *path)
{
    buffer b = read_file(h, path);

    if (b) {
        return b;
    } else {
        printf("can't read a file: %s\n", path);
        exit(1);
    }
}
