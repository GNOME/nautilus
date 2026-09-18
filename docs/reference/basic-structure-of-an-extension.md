Title: Basic structure of an extension

Basic structure of an extension
===============================
The architecture of GNOME Files (Nautilus) allows external developers to easily
extend the core functionality by writing extensions. In this page, we'll
summarize what the expected structure of such an extension should look like.

## Location
Nautilus expects that each extension is installed as a loadable module in a well
known location. Usually that location is going to be the
`/usr/lib/nautilus/extensions-4/` folder.

(Note that the `/usr/lib/` folder might be different though depending on the
configuration of the Nautilus install on your system.)

## Extension entry points
As each extension is a loadable module, Nautilus will need to have a
well-defined set of hooks so it knows how to load/initialize/… the extension.

For that purpose, each extension should provide the following functions:

- [func@Nautilus.module_list_types]
- [func@Nautilus.module_initialize]
- [func@Nautilus.module_shutdown]

## Extending functionality
Depending on the functionality you want to extend, Nautilus provides several
interfaces that you can implement within your extension. Examples of such
interfaces are [iface@Nautilus.MenuProvider], [iface@Nautilus.ColumnProvider],
or [iface@Nautilus.InfoProvider].

When you've implemented the interface(s) you wanted, you can then let Nautilus
know about them using the entry points from the previous section. When
appropriate (for example, creating a `MenuProvider` when a menu is created),
Nautilus will construct an instance of that type.
