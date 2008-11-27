/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <gcutter.h>

void test_direct (void);
void test_int (void);
void test_string (void);

static GString *string;

void
setup (void)
{
    string = g_string_new(NULL);
}

void
teardown (void)
{
    if (string)
        g_string_free(string, TRUE);
}

void
test_direct (void)
{
    gcut_direct_inspect(string, GUINT_TO_POINTER(100), NULL);
    cut_assert_equal_string("100", string->str);
}

void
test_int (void)
{
    gint value = 99;

    gcut_int_inspect(string, &value, NULL);
    cut_assert_equal_string("99", string->str);
}

void
test_string (void)
{
    gcut_string_inspect(string, "XXX", NULL);
    cut_assert_equal_string("\"XXX\"", string->str);
}

/*
vi:nowrap:ai:expandtab:sw=4:ts=4
*/
