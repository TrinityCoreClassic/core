/* Fix for va_copy on older systems */
#ifndef VA_COPY_FIX_H
#define VA_COPY_FIX_H

/* Include config.h first */
#include "config.h"

/* Define va_copy if not already defined */
#ifndef va_copy
# ifdef __va_copy
#  define va_copy(dest, src) __va_copy(dest, src)
# elif defined(__GNUC__)
#  define va_copy(dest, src) __builtin_va_copy(dest, src)
# else
#  define va_copy(dest, src) ((dest) = (src))
# endif
#endif

#endif /* VA_COPY_FIX_H */