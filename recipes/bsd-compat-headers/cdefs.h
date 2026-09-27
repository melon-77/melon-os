/* melon: minimal sys/cdefs.h for software that expects glibc/BSD headers */
#ifndef _SYS_CDEFS_H
#define _SYS_CDEFS_H
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif
#define __THROW
#define __P(x) x
#define __PMT(x) x
#define __CONCAT(x,y) x ## y
#define __STRING(x) #x
#ifndef __attribute_deprecated__
# define __attribute_deprecated__ __attribute__((__deprecated__))
#endif
#ifndef __nonnull
# define __nonnull(params) __attribute__ ((__nonnull__ params))
#endif
#endif
