## Build
See https://handbook.gnome.org/development/building.html

## Coding Style
Basic code formatting can be done by running the formatting script `build-aux/run-uncrustify.sh`.
It is also run via a merge request CI pipeline to catch any oversights.

Nautilus has a lot of legacy code that would not be written the same way today.
Some general guidelines for the current code style:

* Use [automatic cleanups](https://docs.gtk.org/glib/auto-cleanup.html) when possible
* Declare variables where they are needed and assign them directly if possible
* Explicitly compare pointers with `NULL` (`if (variable != NULL)` instead of `if (variable)`)
* Place empty lines between between blocks of declarations, expressions and return statements
* Keep line lengths below 100, put a line break after `=` or between function parameters if needed
* Prefer early returns over nested if cases

You can look at newer changes to see these guidelines in practice.
Example code block:

    g_autoptr (Type) autocleanup_variable = inline_initialization ();

    if (variable == NULL)
    {
        return early;
    }

    g_autofree char *define_variables_where_used =
        use_line_break_after_assignment_if_line_would_get_too_long ();

    function_call_separated_by_empty_line ();
    expressions += function_calls (are_grouped);

    return statement_separated_by_another_empty_line;


## Commit Messages
See https://handbook.gnome.org/development/commit-messages.html

## Unit Tests
Enable building unit tests from the `test/` directory by setting the respective
meson option (`-Dtests=all`).
You can then run them, e.g. with `meson test -C build`.
The tests are also run via a merge request CI pipeline to avoid regressions.

## Build With Address Sanitizer
* Install `libasan` in the build environment
* Reconfigure meson to use sanitizer, e.g. with:

    `meson setup --reconfigure --prefix=$PREFIX build -Db_sanitize=address`
* The sanitizer will inform you about leaked data on exiting.
  This is most useful in combination with unittests
