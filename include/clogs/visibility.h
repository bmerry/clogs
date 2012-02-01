/**
 * @file
 *
 * Internal header file that provides macros to control symbol visibility.
 * Do not include this header directly or use any of the macros it defines.
 * They are not part of the API and are subject to change.
 *
 * @see http://gcc.gnu.org/wiki/Visibility
 */

#ifndef CLOGS_VISIBILITY_H
#define CLOGS_VISIBILITY_H

#if defined(_WIN32) || defined(__CYGWIN__)
# define CLOGS_DLL_IMPORT __declspec(dllimport)
# define CLOGS_DLL_EXPORT __declspec(dllexport)
# define CLOGS_DLL_LOCAL
#else
# if __GNUC__ >= 4
#  define CLOGS_DLL_DO_PUSH_POP
#  define CLOGS_DLL_IMPORT __attribute__((visibility("default")))
#  define CLOGS_DLL_EXPORT __attribute__((visibility("default")))
#  define CLOGS_DLL_LOCAL __attribute__((visibility("hidden")))
# else
#  define CLOGS_DLL_IMPORT
#  define CLOGS_DLL_EXPORT
#  define CLOGS_DLL_LOCAL
# endif
#endif

/* CLOGS_DLL_BUILD is defined by the build system when building the library */
#ifdef CLOGS_DLL_DO_EXPORT
# define CLOGS_API CLOGS_DLL_EXPORT
#else
# define CLOGS_API CLOGS_DLL_IMPORT
#endif
#define CLOGS_LOCAL CLOGS_DLL_LOCAL

#endif /* !CLOGS_VISIBILITY_H */
