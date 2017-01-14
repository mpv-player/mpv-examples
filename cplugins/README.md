# C plugin examples

Documentation is here: https://mpv.io/manual/master/#c-plugins

Other than this, the normal libmpv documentation applies.
Very primitive terminal-only example. Shows some most basic API usage.

## List of Examples

### simple

Very primitive example showing basic API usage.

### GTK

Demonstrates how to use GTK UI elements within a mpv C plugin. Includes some
glue code to overcome the hostileness of GTK to embedding (mpv_gtk_helper.inc).
