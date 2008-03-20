/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2007  Kouhei Sutou <kou@cozmixng.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "cut-report-factory-builder.h"
#include "cut-module-factory.h"

static const gchar *xml_report;
static CutReportFactoryBuilder *the_builder = NULL;

static GObject *constructor  (GType                  type,
                              guint                  n_props,
                              GObjectConstructParam *props);

static void         set_option_context (CutFactoryBuilder *builder,
                                        GOptionContext    *context);
static GList       *build              (CutFactoryBuilder *builder);
static const gchar *get_type_name      (CutFactoryBuilder *builder);

G_DEFINE_TYPE(CutReportFactoryBuilder, cut_report_factory_builder, CUT_TYPE_FACTORY_BUILDER)

static void
cut_report_factory_builder_class_init (CutReportFactoryBuilderClass *klass)
{
    GObjectClass *gobject_class;
    CutFactoryBuilderClass *builder_class;

    gobject_class = G_OBJECT_CLASS(klass);
    builder_class = CUT_FACTORY_BUILDER_CLASS(klass);

    gobject_class->constructor = constructor;

    builder_class->set_option_context = set_option_context;
    builder_class->build              = build;
    builder_class->get_type_name      = get_type_name;
}

static GObject *
constructor (GType type, guint n_props, GObjectConstructParam *props)
{
    GObject *object;

    if (!the_builder) {
        GObjectClass *klass = G_OBJECT_CLASS(cut_report_factory_builder_parent_class);
        object = klass->constructor(type, n_props, props);
    } else {
        object = g_object_ref(G_OBJECT(the_builder));
    }

    return object;
}

static void
cut_report_factory_builder_init (CutReportFactoryBuilder *builder)
{
    const gchar *dir;

    dir = g_getenv("CUT_REPORT_FACTORY_MODULE_DIR");
    if (!dir)
        dir = REPORT_FACTORY_MODULEDIR;

    g_object_set(G_OBJECT(builder),
                 "module-dir", dir,
                 NULL);
}

static void
set_option_context (CutFactoryBuilder *builder, GOptionContext *context)
{
    GOptionEntry entries[] = {
        {"xml-report", 0, 0, G_OPTION_ARG_STRING, &xml_report,
         N_("Set filename of XML report"), "FILE"},
        {NULL}
    };

    g_option_context_add_main_entries(context, entries, NULL);
}

static GList *
build (CutFactoryBuilder *builder)
{
    const gchar *report_type;
    GList *factories = NULL;

    if (!xml_report)
        return NULL;

    report_type = "xml";
    if (cut_module_factory_exist_module("report", report_type)) {
        CutModuleFactory *module_factory;
        GOptionContext *option_context;
        module_factory = cut_module_factory_new("report", report_type,
                                                "filename", xml_report,
                                                NULL);
        g_object_get(builder, "option-context", &option_context, NULL);
        cut_module_factory_set_option_group(module_factory, option_context);
        factories = g_list_prepend(factories, module_factory);
    }

    return factories;
}

const gchar *
get_type_name (CutFactoryBuilder *builder)
{
    return "report";
}

/*
vi:ts=4:nowrap:ai:expandtab:sw=4
*/
