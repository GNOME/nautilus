# Nautilus Coding Style

This document describes the recommended coding style for the Nautilus project. These guidelines are based by the [GNOME C Coding Style](https://developer.gnome.org/programming-guidelines/stable/c-coding-style.html.en).

## Line width

A width between 80 and 120 characters long fits easily in most monitors. The readability of the code should be always the priority. Sometimes having a long line width could be a sign that the code should be restructured, also, having many indentation levels certainly means that the code needs refactoring.

## Indentation

Each indentation level is set to 4 spaces.

```
if (codition)
{
    statement ();
    if (another_condition)
    {
        another_statement ();
    }
}
```

## Tab Characters

The tab character should be expanded to spaces, this can be done in your editor's settings. Configuring a tab size of 4 characters might be helpful for consistency along the Nautilus source code.

## Braces

The Nautilus project uses Allman style for indentation and curly braces placement. The curly braces should go underneath the opening block statement.

```
if (a & b)
{
    foo ();
    bar ();
}
else
{
    baz ();
}
```

Even one line blocks should have their proper curly braces.

```
for (i = 0; i < max_lenght; i++)
{
    some_func (i);
}
```

## Conditions

Following the same rules with braces, condtions will always include braces below the opening block. This is true one line and multline blocks.

```
/* valid */
if (condition)
{
    foo ();
}
else
{
    bar ();
}
```

```
/* valid */
if (condition)
{
    foo ();
    bar ();
}
else
{
    baz ();
}
```

```
/* invalid */
if (condition)
    foo ();
else
    bar ();
```

## Functions

Functions should be declared with the following format.

```
static void
my_function_name (DataType1     *dt1,
                  OtherDataType *dt2,
                  Data           dt3)
{
    ...
}
```

The return type is placed in a separate line, then each parameter should be placed in a different line.

## Whitespace

Always put a space before an opening parenthesis, but never after.

```
/* valid */
switch (confition)
{
    ...
}
```

```
/* valid */
if (condition)
{
    foo ();
}
```

```
/* invalid */
if(condition)
{
    if ( condition )
    {
        foo();
    }
}
```

## Switch statement

The switch statement opens a block in a new indentation level, the case blocks should be placed at the next indentation level sorrounding the block with braces, the break statment should be inserted after the closing braces.

```
switch (condition)
{
    case OPT1:
    {
        ...
    }
    break;

    case OPT2:
    {
        ...
    }
    break;

    case OPT3: /* fall-through */
    default:
    {
        ...
    }
    break;
}
```

## Header files

WIP
