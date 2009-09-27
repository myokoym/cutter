/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2007-2009  Kouhei Sutou <kou@clear-code.com>
 *
 *  This library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include <cutter/cut-module-impl.h>
#include <cutter/cut-listener.h>
#include <cutter/cut-ui.h>
#include <cutter/cut-test-runner.h>
#include <cutter/cut-test-result.h>
#include <cutter/cut-test.h>
#include <cutter/cut-test-suite.h>
#include <cutter/cut-test-context.h>
#include <cutter/cut-backtrace-entry.h>
#include <cutter/cut-verbose-level.h>
#include <cutter/cut-enum-types.h>

#define CUT_TYPE_CONSOLE_UI            cut_type_console_ui
#define CUT_CONSOLE_UI(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CUT_TYPE_CONSOLE_UI, CutConsoleUI))
#define CUT_CONSOLE_UI_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CUT_TYPE_CONSOLE_UI, CutConsoleUIClass))
#define CUT_IS_CONSOLE_UI(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CUT_TYPE_CONSOLE_UI))
#define CUT_IS_CONSOLE_UI_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CUT_TYPE_CONSOLE_UI))
#define CUT_CONSOLE_UI_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CUT_TYPE_CONSOLE_UI, CutConsoleUIClass))

#define RED_COLOR "\033[01;31m"
#define RED_BACK_COLOR "\033[41m"
#define GREEN_COLOR "\033[01;32m"
#define GREEN_BACK_COLOR "\033[01;42m"
#define YELLOW_COLOR "\033[01;33m"
#define BLUE_COLOR "\033[01;34m"
#define BLUE_BACK_COLOR "\033[01;44m"
#define MAGENTA_COLOR "\033[01;35m"
#define CYAN_COLOR "\033[01;36m"
#define CYAN_BACK_COLOR "\033[01;46m"
#define WHITE_COLOR "\033[01;37m"
#define WHITE_BACK_COLOR "\033[01;47m"
#define BLACK_BACK_COLOR "\033[01;40m"
#define NORMAL_COLOR "\033[00m"

#define CRASH_COLOR RED_BACK_COLOR WHITE_COLOR

#define DIFF_LINE_EXPECTED_COLOR GREEN_COLOR
#define DIFF_LINE_ACTUAL_COLOR RED_COLOR
#define DIFF_LINE_CHANGE_COLOR CYAN_COLOR
#define DIFF_IN_LINE_EXPECTED_COLOR GREEN_BACK_COLOR WHITE_COLOR
#define DIFF_IN_LINE_ACTUAL_COLOR RED_BACK_COLOR WHITE_COLOR
#define DIFF_IN_LINE_CHANGE_COLOR CYAN_BACK_COLOR WHITE_COLOR

typedef struct _CutConsoleUI CutConsoleUI;
typedef struct _CutConsoleUIClass CutConsoleUIClass;

struct _CutConsoleUI
{
    GObject       object;
    gchar        *name;
    gboolean      use_color;
    CutVerboseLevel verbose_level;
    GList        *errors;
    gint          progress_row;
    gint          progress_row_max;
};

struct _CutConsoleUIClass
{
    GObjectClass parent_class;
};

enum
{
    PROP_0,
    PROP_USE_COLOR,
    PROP_VERBOSE_LEVEL,
    PROP_PROGRESS_ROW_MAX
};

static GType cut_type_console_ui = 0;
static GObjectClass *parent_class;

static void dispose        (GObject         *object);
static void set_property   (GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec);
static void get_property   (GObject         *object,
                            guint            prop_id,
                            GValue          *value,
                            GParamSpec      *pspec);

static void     attach_to_run_context   (CutListener *listener,
                                         CutRunContext   *run_context);
static void     detach_from_run_context (CutListener *listener,
                                         CutRunContext   *run_context);
static gboolean run                     (CutUI       *ui,
                                         CutRunContext   *run_context);

static void
class_init (CutConsoleUIClass *klass)
{
    GObjectClass *gobject_class;
    GParamSpec *spec;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose      = dispose;
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    spec = g_param_spec_boolean("use-color",
                                "Use color",
                                "Whether use color",
                                FALSE,
                                G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_USE_COLOR, spec);

    spec = g_param_spec_enum("verbose-level",
                             "Verbose Level",
                             "The number of representing verbosity level",
                             CUT_TYPE_VERBOSE_LEVEL,
                             CUT_VERBOSE_LEVEL_NORMAL,
                             G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_VERBOSE_LEVEL, spec);

    spec = g_param_spec_int("progress-row-max",
                            "Progress Row Max",
                            "The max number of progress row",
                            -1, G_MAXINT16, -1,
                            G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_PROGRESS_ROW_MAX, spec);
}

static void
init (CutConsoleUI *console)
{
    console->use_color = FALSE;
    console->verbose_level = CUT_VERBOSE_LEVEL_NORMAL;
    console->errors = NULL;
    console->progress_row = 0;
    console->progress_row_max = -1;
}

static void
listener_init (CutListenerClass *listener)
{
    listener->attach_to_run_context   = attach_to_run_context;
    listener->detach_from_run_context = detach_from_run_context;
}

static void
ui_init (CutUIClass *ui)
{
    ui->run = run;
}

static void
register_type (GTypeModule *type_module)
{
    static const GTypeInfo info =
        {
            sizeof (CutConsoleUIClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof(CutConsoleUI),
            0,
            (GInstanceInitFunc) init,
        };

	static const GInterfaceInfo ui_info =
	    {
            (GInterfaceInitFunc) ui_init,
            NULL,
            NULL
        };

	static const GInterfaceInfo listener_info =
	    {
            (GInterfaceInitFunc) listener_init,
            NULL,
            NULL
        };

    cut_type_console_ui = g_type_module_register_type(type_module,
                                                      G_TYPE_OBJECT,
                                                      "CutConsoleUI",
                                                      &info, 0);

    g_type_module_add_interface(type_module,
                                cut_type_console_ui,
                                CUT_TYPE_UI,
                                &ui_info);

    g_type_module_add_interface(type_module,
                                cut_type_console_ui,
                                CUT_TYPE_LISTENER,
                                &listener_info);
}

G_MODULE_EXPORT GList *
CUT_MODULE_IMPL_INIT (GTypeModule *type_module)
{
    GList *registered_types = NULL;

    register_type(type_module);
    if (cut_type_console_ui)
        registered_types =
            g_list_prepend(registered_types,
                           (gchar *)g_type_name(cut_type_console_ui));

    return registered_types;
}

G_MODULE_EXPORT void
CUT_MODULE_IMPL_EXIT (void)
{
}

G_MODULE_EXPORT GObject *
CUT_MODULE_IMPL_INSTANTIATE (const gchar *first_property, va_list var_args)
{
    return g_object_new_valist(CUT_TYPE_CONSOLE_UI, first_property, var_args);
}

static void
dispose (GObject *object)
{
    CutConsoleUI *console;

    console = CUT_CONSOLE_UI(object);

    if (console->errors) {
        g_list_foreach(console->errors, (GFunc)g_error_free, NULL);
        g_list_free(console->errors);
        console->errors = NULL;
    }

    G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    CutConsoleUI *console = CUT_CONSOLE_UI(object);

    switch (prop_id) {
      case PROP_USE_COLOR:
        console->use_color = g_value_get_boolean(value);
        break;
      case PROP_VERBOSE_LEVEL:
        console->verbose_level = g_value_get_enum(value);
        break;
      case PROP_PROGRESS_ROW_MAX:
        console->progress_row_max = g_value_get_int(value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    CutConsoleUI *console = CUT_CONSOLE_UI(object);

    switch (prop_id) {
      case PROP_USE_COLOR:
        g_value_set_boolean(value, console->use_color);
        break;
      case PROP_VERBOSE_LEVEL:
        g_value_set_enum(value, console->verbose_level);
        break;
      case PROP_PROGRESS_ROW_MAX:
        g_value_set_int(value, console->progress_row_max);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static const gchar *
status_to_name(CutTestResultStatus status)
{
    const gchar *name;

    switch (status) {
    case CUT_TEST_RESULT_SUCCESS:
        name = "Success";
        break;
    case CUT_TEST_RESULT_NOTIFICATION:
        name = "Notification";
        break;
    case CUT_TEST_RESULT_OMISSION:
        name = "Omission";
        break;
    case CUT_TEST_RESULT_PENDING:
        name = "Pending";
        break;
    case CUT_TEST_RESULT_FAILURE:
        name = "Failure";
        break;
    case CUT_TEST_RESULT_ERROR:
        name = "Error";
        break;
    case CUT_TEST_RESULT_CRASH:
        name = "Crash";
        break;
    default:
        name = "MUST NOT HAPPEN!!!";
        break;
    }

    return name;
}

static const gchar *
status_to_color(CutTestResultStatus status)
{
    const gchar *color;

    switch (status) {
    case CUT_TEST_RESULT_SUCCESS:
        color = GREEN_COLOR;
        break;
    case CUT_TEST_RESULT_NOTIFICATION:
        color = CYAN_COLOR;
        break;
    case CUT_TEST_RESULT_OMISSION:
        color = BLUE_COLOR;
        break;
    case CUT_TEST_RESULT_PENDING:
        color = MAGENTA_COLOR;
        break;
    case CUT_TEST_RESULT_FAILURE:
        color = RED_COLOR;
        break;
    case CUT_TEST_RESULT_ERROR:
        color = YELLOW_COLOR;
        break;
    case CUT_TEST_RESULT_CRASH:
        color = CRASH_COLOR;
        break;
    default:
        color = "";
        break;
    }

    return color;
}

static void
print_with_colorv (CutConsoleUI *console, const gchar *color,
                   gchar const *format, va_list args)
{
   if (console->use_color) {
        gchar *message;
        message = g_strdup_vprintf(format, args);
        g_print("%s%s%s", color, message, NORMAL_COLOR);
        g_free(message);
    } else {
        g_vprintf(format, args);
    }
}

static void
print_with_color (CutConsoleUI *console, const gchar *color,
                  gchar const *format, ...)
{
    va_list args;

    va_start(args, format);
    print_with_colorv(console, color, format, args);
    va_end(args);
}

static void
print_for_status (CutConsoleUI *console, CutTestResultStatus status,
                  gchar const *format, ...)
{
    va_list args;

    va_start(args, format);
    print_with_colorv(console, status_to_color(status), format, args);
    va_end(args);
}

static void
print_progress (CutConsoleUI *console, CutTestResultStatus status,
                gchar const *mark)
{
    print_with_color(console, status_to_color(status), "%s", mark);

    console->progress_row += strlen(mark);
    if (console->progress_row >= console->progress_row_max) {
        if (console->progress_row_max != -1 &&
            console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
            g_print("\n");
        console->progress_row = 0;
    }
}

static void
cb_start_test_suite (CutRunContext *run_context, CutTestSuite *test_suite,
                     CutConsoleUI *console)
{
}

static void
cb_start_test_case (CutRunContext *run_context, CutTestCase *test_case,
                    CutConsoleUI *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
        return;

    print_with_color(console, GREEN_BACK_COLOR,
                     "%s", cut_test_get_name(CUT_TEST(test_case)));
    g_print(":\n");
}

static void
cb_start_test_iterator (CutRunContext *run_context,
                        CutTestIterator *test_iterator,
                        CutConsoleUI *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
        return;

    g_print("  ");
    print_with_color(console, BLUE_BACK_COLOR,
                     "%s", cut_test_get_name(CUT_TEST(test_iterator)));
    g_print(":\n");
}

static void
print_test_on_start (CutConsoleUI *console, const gchar *name,
                     CutTest *test, const gchar *indent)
{
    GString *tab_stop;
    gint name_length;
    const gchar *description;

    description = cut_test_get_description(test);
    if (description)
        g_print("  %s%s\n", indent, description);

    name_length = strlen(indent) + strlen(name) + 2;
    tab_stop = g_string_new("");
    while (name_length < (8 * 7 - 1)) {
        g_string_append_c(tab_stop, '\t');
        name_length += 8;
    }
    g_print("  %s%s:%s", indent, name, tab_stop->str);
    g_string_free(tab_stop, TRUE);
    fflush(stdout);
}

static void
cb_start_test (CutRunContext *run_context, CutTest *test,
               CutTestContext *test_context, CutConsoleUI *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
        return;

    print_test_on_start(console, cut_test_get_name(test), test, "");
}

static void
cb_start_iterated_test (CutRunContext *run_context,
                        CutIteratedTest *iterated_test,
                        CutTestContext *test_context, CutConsoleUI *console)
{
    const gchar *name = NULL;

    if (console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
        return;

    if (cut_test_context_have_data(test_context)) {
        CutTestData *data;

        data = cut_test_context_get_current_data(test_context);
        name = cut_test_data_get_name(data);
    }

    if (!name)
        name = cut_test_get_name(CUT_TEST(iterated_test));

    print_test_on_start(console, name, CUT_TEST(iterated_test), "  ");
}

static void
cb_success_test (CutRunContext  *run_context,
                 CutTest        *test,
                 CutTestContext *context,
                 CutTestResult  *result,
                 CutConsoleUI   *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_NORMAL)
        return;
    print_progress(console, CUT_TEST_RESULT_SUCCESS, ".");
    fflush(stdout);
}

static void
cb_notification_test (CutRunContext  *run_context,
                      CutTest        *test,
                      CutTestContext *test_context,
                      CutTestResult  *result,
                      CutConsoleUI   *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_NORMAL)
        return;
    print_progress(console, CUT_TEST_RESULT_NOTIFICATION, "N");
    fflush(stdout);
}

static void
cb_omission_test (CutRunContext  *run_context,
                  CutTest        *test,
                  CutTestContext *test_context,
                  CutTestResult  *result,
                  CutConsoleUI   *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_NORMAL)
        return;
    print_progress(console, CUT_TEST_RESULT_OMISSION, "O");
    fflush(stdout);
}

static void
cb_pending_test (CutRunContext  *run_context,
                 CutTest        *test,
                 CutTestContext *test_context,
                 CutTestResult  *result,
                 CutConsoleUI   *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_NORMAL)
        return;
    print_progress(console, CUT_TEST_RESULT_PENDING, "P");
    fflush(stdout);
}

static void
cb_failure_test (CutRunContext  *run_context,
                 CutTest        *test,
                 CutTestContext *test_context,
                 CutTestResult  *result,
                 CutConsoleUI   *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_NORMAL)
        return;
    print_progress(console, CUT_TEST_RESULT_FAILURE, "F");
    fflush(stdout);
}

static void
cb_error_test (CutRunContext  *run_context,
               CutTest        *test,
               CutTestContext *test_context,
               CutTestResult  *result,
               CutConsoleUI   *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_NORMAL)
        return;
    print_progress(console, CUT_TEST_RESULT_ERROR, "E");
    fflush(stdout);
}

static void
handle_crash (CutRunContext  *run_context,
              CutTestResult  *result,
              CutConsoleUI   *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_NORMAL)
        return;
    print_progress(console, CUT_TEST_RESULT_CRASH, "!");
    fflush(stdout);
}

static void
cb_crash_test (CutRunContext  *run_context,
               CutTest        *test,
               CutTestContext *test_context,
               CutTestResult  *result,
               CutConsoleUI   *console)
{
    handle_crash(run_context, result, console);
}

static void
cb_crash_test_suite (CutRunContext  *run_context,
                     CutTestSuite   *test_suite,
                     CutTestResult  *result,
                     CutConsoleUI   *console)
{
    handle_crash(run_context, result, console);
}

static void
cb_complete_test (CutRunContext *run_context, CutTest *test,
                  CutTestContext *test_context, gboolean success,
                  CutConsoleUI *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
        return;

    g_print(": (%f)\n", cut_test_get_elapsed(test));
    fflush(stdout);
}

static void
cb_complete_iterated_test (CutRunContext *run_context,
                           CutIteratedTest *iterated_test,
                           CutTestContext *test_context, gboolean success,
                           CutConsoleUI *console)
{
    cb_complete_test(run_context, CUT_TEST(iterated_test),
                     test_context, success, console);
}

static void
cb_complete_test_iterator (CutRunContext *run_context,
                           CutTestIterator *test_iterator,
                           gboolean success, CutConsoleUI *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
        return;
}

static void
cb_complete_test_case (CutRunContext *run_context, CutTestCase *test_case,
                       gboolean success, CutConsoleUI *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
        return;
}

static void
cb_complete_test_suite (CutRunContext *run_context, CutTestSuite *test_suite,
                        gboolean success, CutConsoleUI *console)
{
    if (console->verbose_level < CUT_VERBOSE_LEVEL_VERBOSE)
        return;
}

typedef struct _ConsoleAndStatus {
    CutConsoleUI *console;
    CutTestResultStatus status;
} ConsoleAndStatus;

static void
print_each_attribute (gpointer key, gpointer value, gpointer data)
{
    ConsoleAndStatus *info = (ConsoleAndStatus*)data;
    g_print("\n");
    print_with_color(info->console, status_to_color(info->status),
                     "  %s: %s",
                     (const gchar*)key, (const gchar*)value,
                     NULL);
}

static void
print_test_attributes (CutConsoleUI *console, CutTestResultStatus status,
                       CutTest *test)
{
    ConsoleAndStatus info;
    info.console = console;
    info.status = status;
    g_hash_table_foreach(cut_test_get_attributes(test),
                         (GHFunc)print_each_attribute, &info);
}

static void
print_colorize_diff_with_one_side_change (CutConsoleUI *console,
                                          const gchar *line,
                                          const gchar *char_diff_line)
{
    print_with_color(console, DIFF_IN_LINE_CHANGE_COLOR, "*");
    g_print(" ");

    for (; char_diff_line[0]; char_diff_line++) {
        switch (char_diff_line[0]) {
        case '-':
            print_with_color(console,
                             DIFF_IN_LINE_EXPECTED_COLOR,
                             "%c", line[0]);
            break;
        case '+':
            print_with_color(console,
                             DIFF_IN_LINE_ACTUAL_COLOR,
                             "%c", line[0]);
            break;
        default:
            g_print("%c", line[0]);
            break;
        }
        line++;
    }

    g_print("%s", line);
}

static void
print_colorize_diff_with_both_change (CutConsoleUI *console,
                                      const gchar *expected_line,
                                      const gchar *expected_char_diff_line,
                                      const gchar *actual_line,
                                      const gchar *actual_char_diff_line)
{
    const gchar *original_actual_line = actual_line;
    const gchar *original_actual_char_diff_line = actual_char_diff_line;
    gboolean have_diff_char = FALSE;
    gint i;

    for (i = 0; expected_char_diff_line[i]; i++) {
        if (expected_char_diff_line[i] == '^') {
            have_diff_char = TRUE;
            break;
        }
    }

    if (have_diff_char)
        print_with_color(console, DIFF_IN_LINE_EXPECTED_COLOR, "-");
    else
        print_with_color(console, DIFF_IN_LINE_CHANGE_COLOR, "*");
    g_print(" ");

    while (expected_char_diff_line[0] || actual_char_diff_line[0]) {
        gboolean check_actual = TRUE;

        if (expected_char_diff_line[0]) {
            switch (expected_char_diff_line[0]) {
            case '^':
                print_with_color(console, DIFF_IN_LINE_EXPECTED_COLOR,
                                 "%c", expected_line[0]);
                actual_line++;
                actual_char_diff_line++;
                check_actual = FALSE;
                break;
            case '-':
                print_with_color(console, DIFF_IN_LINE_EXPECTED_COLOR,
                                 "%c", expected_line[0]);
                check_actual = FALSE;
                break;
            default:
                if (!actual_char_diff_line[0]) {
                    g_print("%c", expected_line[0]);
                    actual_line++;
                    check_actual = FALSE;
                }
                break;
            }
        }

        if (check_actual && actual_char_diff_line[0]) {
            switch (actual_char_diff_line[0]) {
            case '^':
                break;
            case '+':
                print_with_color(console, DIFF_IN_LINE_ACTUAL_COLOR,
                                 "%c", actual_line[0]);
                break;
            default:
                g_print("%c", actual_line[0]);
                if (expected_line[0])
                    expected_line++;
                if (expected_char_diff_line[0])
                    expected_char_diff_line++;
                break;
            }
            actual_line++;
            actual_char_diff_line++;
        } else {
            expected_line++;
            expected_char_diff_line++;
        }
    }

    if (have_diff_char) {
        g_print("%s\n", expected_line);
        actual_line = original_actual_line;
        actual_char_diff_line = original_actual_char_diff_line;
        print_with_color(console, DIFF_IN_LINE_ACTUAL_COLOR, "+");
        g_print(" ");

        while (actual_char_diff_line[0]) {
            switch (actual_char_diff_line[0]) {
            case '^':
                print_with_color(console, DIFF_IN_LINE_ACTUAL_COLOR,
                                 "%c", actual_line[0]);
                break;
            default:
                g_print(" ");
            break;
            }
            actual_line++;
            actual_char_diff_line++;
      }
    } else {
        g_print("%s", actual_line);
    }
}

static void
print_colorize_diff (CutConsoleUI *console, const gchar *diff)
{
    gchar **lines, **_lines;

    lines = g_regex_split_simple("\r?\n", diff, 0, 0);
    for (_lines = lines; *_lines; _lines++) {
        const gchar *line, *line2, *line3 = NULL, *line4 = NULL;

        line = *_lines;
        line2 = *(_lines + 1);
        if (line2)
            line3 = *(_lines + 2);
        if (line3)
            line4 = *(_lines + 3);
        switch (line[0]) {
        case '-':
            if ((line2 && line2[0] == '+') &&
                (line3 && line3[0] == '?')) {
                const gchar *actual_line = line2;
                const gchar *actual_char_diff_line = line3;

                print_colorize_diff_with_one_side_change(
                    console,
                    actual_line + 2,
                    actual_char_diff_line + 2);
                _lines += 2;
            } else if ((line2 && line2[0] == '?') &&
                       (line3 && line3[0] == '+')) {
                const gchar *expected_char_diff_line = line2;
                const gchar *actual_line = line3;

                if (line4 && line4[0] == '?') {
                    const gchar *actual_char_diff_line = line4;

                    print_colorize_diff_with_both_change(
                        console,
                        line + 2,
                        expected_char_diff_line + 2,
                        actual_line + 2,
                        actual_char_diff_line + 2);
                    _lines++;
                } else {
                    print_colorize_diff_with_one_side_change(
                        console,
                        line + 2,
                        expected_char_diff_line + 2);
                }
                _lines += 2;
            } else {
                print_with_color(console, DIFF_LINE_EXPECTED_COLOR,
                                 "%s", line);
            }
            break;
        case '+':
            print_with_color(console, DIFF_LINE_ACTUAL_COLOR,
                             "%s", line);
            break;
        case '?':
            print_with_color(console, DIFF_LINE_CHANGE_COLOR,
                             "%s", line);
            break;
        default:
            g_print("%s", line);
            break;
        }
        g_print("\n");
    }
    g_strfreev(lines);
}

static void
print_result_detail (CutConsoleUI *console, CutTestResultStatus status,
                     CutTestResult *result)
{
    const gchar *expected, *actual;

    expected = cut_test_result_get_expected(result);
    actual = cut_test_result_get_actual(result);
    if (expected && actual) {
        const gchar *user_message, *system_message;
        const gchar *diff;

        user_message = cut_test_result_get_user_message(result);
        system_message = cut_test_result_get_system_message(result);
        diff = cut_test_result_get_diff(result);
        if (user_message) {
            g_print("\n");
            print_for_status(console, status, "%s", user_message);
        }

        if (system_message) {
            g_print("\n");
            print_for_status(console, status, "%s", system_message);
        }

        g_print("\n");
        g_print("expected: <");
        print_for_status(console, CUT_TEST_RESULT_SUCCESS, "%s", expected);
        g_print(">\n");
        g_print("  actual: <");
        print_for_status(console, CUT_TEST_RESULT_FAILURE, "%s", actual);
        g_print(">");

        if (diff) {
            g_print("\n\n");
            g_print("diff:\n");
            if (console->use_color)
                print_colorize_diff(console, diff);
            else
                g_print(diff);
        }

        if (!console->use_color) {
            const gchar *folded_diff;

            folded_diff = cut_test_result_get_folded_diff(result);
            if (folded_diff) {
                g_print("\n\n");
                g_print("folded diff:\n%s", folded_diff);
            }
        }
    } else {
        const gchar *message;

        message = cut_test_result_get_message(result);
        if (message) {
            g_print("\n");
            print_for_status(console, status, "%s", message);
        }
    }
}

static void
print_results (CutConsoleUI *console, CutRunContext *run_context)
{
    gint i;
    const GList *node;

    i = 1;
    for (node = console->errors; node; node = g_list_next(node)) {
        GError *error = node->data;

        g_print("\n%d) ", i);
        print_for_status(console, CUT_TEST_RESULT_ERROR,
                         "SystemError: %s:%d",
                         g_quark_to_string(error->domain),
                         error->code);
        if (error->message) {
            g_print("\n");
            print_for_status(console, CUT_TEST_RESULT_ERROR,
                             "%s", error->message);
        }
        g_print("\n");
        i++;
    }

    for (node = cut_run_context_get_results(run_context);
         node;
         node = g_list_next(node)) {
        CutTestResult *result = node->data;
        CutTestResultStatus status;
        CutTest *test;
        const GList *node;
        const gchar *name;

        status = cut_test_result_get_status(result);
        if (status == CUT_TEST_RESULT_SUCCESS)
            continue;

        name = cut_test_result_get_test_name(result);
        if (!name)
            name = cut_test_result_get_test_case_name(result);
        if (!name)
            name = cut_test_result_get_test_suite_name(result);

        g_print("\n%d) ", i);
        print_for_status(console, status, "%s", status_to_name(status));
        g_print(": %s", name);

        test = cut_test_result_get_test(result);
        if (test)
            print_test_attributes(console, status, test);

        print_result_detail(console, status, result);

        g_print("\n");
        for (node = cut_test_result_get_backtrace(result);
             node;
             node = g_list_next(node)) {
            CutBacktraceEntry *entry = node->data;
            gchar *formatted_entry;

            formatted_entry = cut_backtrace_entry_format(entry);
            g_print("%s\n", formatted_entry);
            g_free(formatted_entry);
        }
        i++;
    }
}

static void
print_summary (CutConsoleUI *console, CutRunContext *run_context)
{
    guint n_tests, n_assertions, n_successes, n_failures, n_errors;
    guint n_pendings, n_notifications, n_omissions;
    gdouble pass_percentage;
    const gchar *color;

    n_tests = cut_run_context_get_n_tests(run_context);
    n_assertions = cut_run_context_get_n_assertions(run_context);
    n_successes = cut_run_context_get_n_successes(run_context);
    n_failures = cut_run_context_get_n_failures(run_context);
    n_errors = cut_run_context_get_n_errors(run_context);
    n_pendings = cut_run_context_get_n_pendings(run_context);
    n_notifications = cut_run_context_get_n_notifications(run_context);
    n_omissions = cut_run_context_get_n_omissions(run_context);

    color = status_to_color(cut_run_context_get_status(run_context));
    print_with_color(console, color,
                     "%d test(s), %d assertion(s), %d failure(s), "
                     "%d error(s), %d pending(s), %d omission(s), "
                     "%d notification(s)",
                     n_tests, n_assertions, n_failures, n_errors,
                     n_pendings, n_omissions, n_notifications);
    g_print("\n");

    if (n_tests == 0) {
        pass_percentage = 0;
    } else {
        pass_percentage = 100.0 * ((gdouble)n_successes / (gdouble)n_tests);
    }
    print_with_color(console, color, "%g%% passed", pass_percentage);
    g_print("\n");
}

static void
cb_complete_run (CutRunContext *run_context, gboolean success,
                 CutConsoleUI *console)
{
    CutVerboseLevel verbose_level;

    verbose_level = console->verbose_level;
    if (verbose_level < CUT_VERBOSE_LEVEL_NORMAL)
        return;

    if (verbose_level == CUT_VERBOSE_LEVEL_NORMAL)
        g_print("\n");

    print_results(console, run_context);

    g_print("\n");
    g_print("Finished in %f seconds (total: %f seconds)",
            cut_run_context_get_elapsed(run_context),
            cut_run_context_get_total_elapsed(run_context));
    g_print("\n\n");

    print_summary(console, run_context);
}

static void
cb_error (CutRunContext *run_context, GError *error, CutConsoleUI *console)
{
    if (console->verbose_level >= CUT_VERBOSE_LEVEL_NORMAL) {
        print_with_color(console, status_to_color(CUT_TEST_RESULT_ERROR), "E");
        fflush(stdout);
    }

    console->errors = g_list_append(console->errors, g_error_copy(error));
}

static void
connect_to_run_context (CutConsoleUI *console, CutRunContext *run_context)
{
#define CONNECT(name) \
    g_signal_connect(run_context, #name, G_CALLBACK(cb_ ## name), console)

    CONNECT(start_test_suite);
    CONNECT(start_test_case);
    CONNECT(start_test_iterator);
    CONNECT(start_test);
    CONNECT(start_iterated_test);

    CONNECT(success_test);
    CONNECT(notification_test);
    CONNECT(omission_test);
    CONNECT(pending_test);
    CONNECT(failure_test);
    CONNECT(error_test);
    CONNECT(crash_test);

    CONNECT(crash_test_suite);

    CONNECT(complete_test);
    CONNECT(complete_iterated_test);
    CONNECT(complete_test_iterator);
    CONNECT(complete_test_case);
    CONNECT(complete_test_suite);
    CONNECT(complete_run);

    CONNECT(error);

#undef CONNECT
}

static void
disconnect_from_run_context (CutConsoleUI *console, CutRunContext *run_context)
{
#define DISCONNECT(name)                                                \
    g_signal_handlers_disconnect_by_func(run_context,                   \
                                         G_CALLBACK(cb_ ## name),       \
                                         console)

    DISCONNECT(start_test_suite);
    DISCONNECT(start_test_case);
    DISCONNECT(start_test_iterator);
    DISCONNECT(start_test);
    DISCONNECT(start_iterated_test);

    DISCONNECT(success_test);
    DISCONNECT(notification_test);
    DISCONNECT(omission_test);
    DISCONNECT(pending_test);
    DISCONNECT(failure_test);
    DISCONNECT(error_test);
    DISCONNECT(crash_test);

    DISCONNECT(crash_test_suite);

    DISCONNECT(complete_iterated_test);
    DISCONNECT(complete_test);
    DISCONNECT(complete_test_iterator);
    DISCONNECT(complete_test_case);
    DISCONNECT(complete_test_suite);
    DISCONNECT(complete_run);

    DISCONNECT(error);

#undef DISCONNECT
}

static void
attach_to_run_context (CutListener *listener,
                       CutRunContext   *run_context)
{
    connect_to_run_context(CUT_CONSOLE_UI(listener), run_context);
}

static void
detach_from_run_context (CutListener *listener,
                         CutRunContext   *run_context)
{
    disconnect_from_run_context(CUT_CONSOLE_UI(listener), run_context);
}

static gboolean
run (CutUI *ui, CutRunContext *run_context)
{
    return cut_run_context_start(run_context);
}

/*
vi:ts=4:nowrap:ai:expandtab:sw=4
*/
