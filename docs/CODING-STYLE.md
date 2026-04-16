# Nautilus Coding Style

This document describes the recommended coding style for the Nautilus project. These guidelines are based by the [GNOME C Coding Style](https://developer.gnome.org/documentation/guidelines/programming/coding-style.html).

## Line width

A width between 80 and 120 characters long fits easily in most monitors. The readability of the code should be always the priority. Sometimes having a long line width could be a sign that the code should be restructured, also, having many indentation levels certainly means that the code needs refactoring.

## Indentation

Each indentation level is set to 4 spaces.

```c
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

```c
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

```c
for (i = 0; i < max_lenght; i++)
{
    some_func (i);
}
```

## Conditions

Following the same rules with braces, conditions will always include braces, even for single statement blocks.

```c
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

```c
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

```c
/* invalid */
if (condition)
    foo ();
else
    bar ();
```

## Functions

Functions should be declared with the following format.

```c
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

```c
/* valid */
switch (condition)
{
    ...
}
```

```c
/* valid */
if (condition)
{
    foo ();
}
```

```c
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

```c
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

## Variable declerations

Variables must be declared in the smallest possible scope.

```c
/* valid */
g_autoptr (type) variable = NULL;

if (condition)
{
    /* ... */

    variable = value1;
}
else
{
    /* ... */

    variable = value2;
}

function_name (argument, variable);
```

```c
/* invalid */
type variable;

if (condition)
{
    variable = value1;

    /* ... */

    unref (variable);
}
else
{
    variable = value2;

    /* ... */

    unref (variable);
}
```

Declerations should only be in a contigous block after the start of the scope or after a conditional return, break, or continue. There must be one empty line between variables blocks and statements.

```c
/* valid */
while (condition)
{
    type other_condition = value

    if (other_condition)
    {
        continue;
    }

    type variable1;

    /* ... */
}
```

```c
/* invalid */
type variable;
function_name (variable);
```

```c
/* invalid */
type variable1;

function_name (argument1, argument2, &variable1);

type variable2 = value;
```

You should use auto cleanup macros where possible. Never declear variables with auto cleanup pointers without assigning to a value or NULL.

```c
/* valid */
g_autoptr (type) variable1 = value;
g_autoptr (type) variable2 = NULL;

/* ... */

variable2 = variable2;
```

```c
/* invalid */
g_autoptr (type) variable2;
```

## Header files

The only major rule for headers is that the function definitions should be vertically aligned in three columns:

```c
return_type          function_name           (type   argument,
                                              type   argument,
                                              type   argument);
```
