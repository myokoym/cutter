#include "cutter.h"
#include "cut-module-factory.h"

#include "cuttest-utils.h"

void test_module_names (void);
void test_module_arguments (void);

static CutModuleFactory *factory;
static GOptionContext *option_context;
static GObject *object;

void
initialize (void)
{
    cut_module_factory_init();
    cut_module_factory_load("./factory_test_dir");
}

void
finalize (void)
{
    cut_module_factory_unload();
    cut_module_factory_quit();
}

void
setup (void)
{
    factory = NULL;
    option_context = NULL;
    object = NULL;
}

void
teardown (void)
{
    if (factory)
        g_object_unref(factory);
    if (option_context)
        g_option_context_free(option_context);
    if (object)
        g_object_unref(object);
}

void
test_module_names (void)
{
    factory = cut_module_factory_new("type1", "factory_module1", NULL);
    cut_assert(factory);
    g_object_unref(factory);

    factory = cut_module_factory_new("type1", "factory_module2", NULL);
    cut_assert(factory);
    g_object_unref(factory);

    factory = cut_module_factory_new("type1", "factory_module3", NULL);
    cut_assert(factory);
    g_object_unref(factory);
    factory = NULL;

    factory = cut_module_factory_new("type1", "factory_module4", NULL);
    cut_assert(!factory);
}

void
test_module_arguments (void)
{
    gchar *function_name = NULL, *description = NULL;
    gint argc = 5;
    gchar *argv[] = {
        "test program",
        "-f", "dummy_function",
        "-d", "This test is a dummy",
        NULL,
    };
    gchar **copy_argv;
    copy_argv = g_strdupv(argv);

    factory = cut_module_factory_new("type1", "factory_module1", NULL);
    cut_assert(factory);

    option_context = g_option_context_new(NULL);
    cut_module_factory_set_option_group(factory, option_context);
    cut_assert(g_option_context_parse(option_context, &argc, &copy_argv, NULL));
    g_strfreev(copy_argv);

    object = cut_module_factory_create(factory);
    cut_assert(object);

    g_object_get(object,
                 "function-name", &function_name,
                 "description", &description,
                 NULL);
    cut_assert_equal_string_with_free("dummy_function", function_name);
    cut_assert_equal_string_with_free("This test is a dummy", description);

    g_object_unref(factory);
    factory = NULL;
}

/*
vi:ts=4:nowrap:ai:expandtab:sw=4
*/
