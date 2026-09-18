Title: Example extension in C

Example extension in C
======================
In this page, we'll create a simple example of how to write a simple extension
using `libnautilus-extension-4`.

## Project structure
Let's create a new directory for your extension. We will then create 3 files to
start:

- `meson.build`: A simple build definition file using the [Meson] build system
- `demo-extension.h`: Contains declarations in a separate header file (optional)
- `demo-extension.c`: Contains the source code of our extension


## meson.build
This file can be relatively straightforward. We declare a loadable module, and
make sure it is installed in the nautilus extensions directory.

```meson
project('demo-extension', 'c')

# All the necessary symbols for extensions can be found and linked using
# the 'libnautilus-extension-4' pkg-config file
libnautilus_extensions_dep = dependency('libnautilus-extension-4')

demo_extension = shared_library('demo-extension',
  'demo-extension.c',
  dependencies: libnautilus_extensions_dep,
  install: true,
  # By default, Nautilus expects files to be installed at this directory
  install_dir: get_option('libdir') / 'nautilus' / 'extensions-4',
)

```

## demo-extension.h

Next, we intrdouce our header file. Note that using a separate header file is
really optional in a small example like this, but it helps our to split things
up a bit.

If you've written C code with GObject before, this will look pretty familiar.
For the people who don't have a lot of experience, this header file declares a
object class `DemoExtension` which derives from the base object class,
[class@GObject.Object].

The actual definition of the object comes later in the `.c` file.

```c
#ifndef __DEMO_EXTENSION_H__
#define __DEMO_EXTENSION_H__

#include <glib-object.h>

#define DEMO_TYPE_EXTENSION (demo_extension_get_type())
G_DECLARE_FINAL_TYPE (DemoExtension, demo_extension,
                      DEMO, EXTENSION,
                      GObject)

#endif /* __DEMO_EXTENSION_H__ */
```

## demo-extension.c
We'll go over the source code file now in pieces and explain them one by one.
If you want to see the full file, you can skip to the bottom of this section.

### Includes

Let's start with the `#include`s at the top of the file. We'll include a comment
for each line why it's there.

```c
/* Our header file from earlier */
#include "nautilus-extension-demo.h"

/* This is the header file that contains the API for nautilus extensions */
#include <nautilus-extension.h>

/* This isn't strictly necessary, but is probably a good idea if you want to
 * make sure your UI is localized */
#define GETTEXT_PACKAGE "demo-extension"
#include <glib/gi18n-lib.h>
```

### Entry points for a Nautilus extension
From an extension point of view, there are 3 functions that Nautilus will try to
load, which are the following:

```c
/* This is where you intialize your module. This can be several things, but an
 * important part is to register the object types you defined earlier */
void
nautilus_module_initialize (GTypeModule *module)
{
    /* We register our DemoExtension object */
    demo_extension_register_type (module);
}

/* This is where you do any necessary cleanup */
void
nautilus_module_shutdown (void)
{
}

/* This is where you list the types that should be loaded */
void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
    /* We only have our single "DemoExtension" object type */
    static GType type_list[1] = { 0 };

    type_list[0] = DEMO_TYPE_EXTENSION;

    *types = type_list;
    *num_types = G_N_ELEMENTS (type_list);
}
```

### Defining our object
In our header file, we've already set up the declarations for our
`DemoExtension` object type, and now we're ready for actually defining it.

For someone who has experience writing GObject-based code, this will again all
look pretty familiar. Note however that we're explictily using
[func@GObject.DEFINE_DYNAMIC_TYPE] rather than using [func@GObject.DEFINE_TYPE],
due to us being in a shared module.

Finally, this macro defines a function we'll need at module initialization,
`demo_extension_register_type()`, which let's Nautilus know about our object
type.

```c
/* This defines the actual data struct with the object's fields */
typedef struct _DemoExtension {
    /* This is a required field, and *must* be at the top of the struct */
    GObject parent_instance;

    /* This is where you can store extra fields in your object */
} DemoExtension;

/* The macro defines a bunch of GObject boilerplate code for our DemoExtension
 * object. Part of that code are a couple of function declarations which we'll
 * have to implement */
G_DEFINE_DYNAMIC_TYPE (DemoExtension, demo_extension, G_TYPE_OBJECT)

static void
demo_extension_init (DemoExtension *self)
{
    /* You can initialize your object's fields here */
}

static void
demo_extension_class_finalize (DemoExtensionClass *klass)
{
    /* You can clean up your class's fields here */
}

static void
demo_extension_class_init (DemoExtensionClass *klass)
{
    /* This is where you can initialize class variables, or do further GObject
     * subclassing (for example overriding a vfunc of a parent class) */
}
```

### Full .c file

To finish up things, let's put the full source code of the .c file here:


```c
#include "nautilus-extension-demo.h"

#include <nautilus-extension.h>

#define GETTEXT_PACKAGE "demo-extension"
#include <glib/gi18n-lib.h>

typedef struct _DemoExtension {
    GObject parent_instance;
} DemoExtension;

static void
nautilus_menu_provider_interface_init (NautilusMenuProviderInterface *interface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (DemoExtension, demo_extension, G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (NAUTILUS_TYPE_MENU_PROVIDER,
                                                               nautilus_menu_provider_interface_init))

static void
on_menu_item_activate (NautilusMenuItem *menu_item,
                       gpointer          user_data)
{
    g_warning ("Activate clicked!");
}

static GList *
demo_extension_get_file_items (NautilusMenuProvider *provider,
                               GList                *files)
{
    /* If no files are selected, do nothing */
    guint n_files = g_list_length (files);
    if (n_files == 0)
        return NULL;

    NautilusMenuItem *menu_item = nautilus_menu_item_new (
        "demo-ext",                    /* Identifier */
        _("Demo Extension"),           /* Label      */
        _("Demo extension test!"),     /* Tooltip    */
        "x-office-document-symbolic"   /* Icon       */
    );
    g_signal_connect (menu_item, "activate", G_CALLBACK (on_menu_item_activate), NULL);

    GList *items = NULL;
    items = g_list_prepend (items, menu_item);

    return items;
}

static void
nautilus_menu_provider_interface_init (NautilusMenuProviderInterface *interface)
{
  interface->get_file_items = demo_extension_get_file_items;
}

static void
demo_extension_init (DemoExtension *self)
{
}

static void
demo_extension_class_finalize (DemoExtensionClass *klass)
{
}

static void
demo_extension_class_init (DemoExtensionClass *klass)
{
}

/** NAUTILUS HOOKS **/

void
nautilus_module_initialize (GTypeModule *module)
{
    demo_extension_register_type (module);
}

void
nautilus_module_shutdown (void)
{
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
    static GType type_list[1] = { 0 };

    type_list[0] = DEMO_TYPE_EXTENSION;

    *types = type_list;
    *num_types = G_N_ELEMENTS (type_list);
}
```


[Meson]: https://mesonbuild.com/
