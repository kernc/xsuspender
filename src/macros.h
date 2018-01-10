#ifndef XSUSPENDER_MACROS_H
#define XSUSPENDER_MACROS_H

#include <glib.h>

// Remove once glib-2.44+ is ubiquitous
#if !defined(g_auto)    || \
    !defined(g_autoptr) || \
    !defined(g_autofree)

static inline void
_cleanup_generic_autofree (void *p)
{
    void **pp = (void**) p;
    if (*pp)
        g_free (*pp);
}

#define _GNUC_CLEANUP(func) __attribute__((cleanup(func)))
#define _AUTOPTR_FUNC_NAME(TypeName) _xsus_autoptr_cleanup_##TypeName
#define _AUTOPTR_TYPENAME(TypeName)  TypeName##_autoptr
#define _AUTO_FUNC_NAME(TypeName)    _xsus_auto_cleanup_##TypeName
#define _DEFINE_AUTOPTR_CLEANUP_FUNC(TypeName, func) \
  typedef TypeName *_AUTOPTR_TYPENAME(TypeName);     \
  static inline void _AUTOPTR_FUNC_NAME(TypeName) (TypeName **_ptr) { if (*_ptr) (func) (*_ptr); }
#define _DEFINE_AUTO_CLEANUP_FREE_FUNC(TypeName, func) \
  static inline void _AUTO_FUNC_NAME(TypeName) (TypeName *_ptr) { if (*_ptr) (func) (*_ptr); }

_DEFINE_AUTOPTR_CLEANUP_FUNC(GError, g_error_free)
_DEFINE_AUTOPTR_CLEANUP_FUNC(GHashTable, g_hash_table_unref)
_DEFINE_AUTOPTR_CLEANUP_FUNC(GKeyFile, g_key_file_unref)
_DEFINE_AUTOPTR_CLEANUP_FUNC(GOptionContext, g_option_context_free)
_DEFINE_AUTO_CLEANUP_FREE_FUNC(GStrv, g_strfreev)
#undef _DEFINE_AUTO_CLEANUP_FREE_FUNC
#undef _DEFINE_AUTOPTR_CLEANUP_FUNC

#define g_autoptr(TypeName) _GNUC_CLEANUP(_AUTOPTR_FUNC_NAME(TypeName)) _AUTOPTR_TYPENAME(TypeName)
#define g_auto(TypeName) _GNUC_CLEANUP(_AUTO_FUNC_NAME(TypeName)) TypeName
#define g_autofree _GNUC_CLEANUP(_cleanup_generic_autofree)

#endif


#endif  // XSUSPENDER_MACROS_H
