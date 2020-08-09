= File Ownership =
In the minimal case, a nautilus file is owned by:
- Window slot -> viewed_file
- Window slot -> history list using bookmarks
- Path bar -> button -> file
- Path bar -> button -> nautilus_drag_slot_proxy

When a file is a bookmark, nautilus application keeps a list with them, so it
also owns those files.

Window slot is the creator of the file if the file was already not present due
to be a bookmark.

The window has a queue with information of the closed tabs, owning references
to previous files.

= Directory Ownership =
Every file has a directory associated, that is usually the parent. However, when
the file is a root and has no parent, the file is called self_owned, and the directory
and the file are the same location, but different objects.

It's better to always deal with files instead of directories, and let the file handle
the ownership of its associated directory.

= View Ownership =
It's owned by:
- Window slot as a strong reference, since the view sinks the floating reference.
So to freed it the window slot needs to destroy it with gtk_widget_destroy ()
since it's the container, but also needs to unref it.
