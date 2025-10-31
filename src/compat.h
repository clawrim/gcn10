#ifndef GCN10_COMPAT_H
#define GCN10_COMPAT_H

#ifdef _MSC_VER
  #ifndef _CRT_SECURE_NO_WARNINGS
  #define _CRT_SECURE_NO_WARNINGS
  #endif
  #ifndef _CRT_NONSTDC_NO_DEPRECATE
  #define _CRT_NONSTDC_NO_DEPRECATE
  #endif
#endif

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <sys/stat.h>
  #include <time.h>

  /* S_ISDIR replacement */
  #ifndef S_IFMT
  #define S_IFMT  0170000
  #endif
  #ifndef S_IFDIR
  #define S_IFDIR 0040000
  #endif
  #ifndef S_ISDIR
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
  #endif

  /* strdup mapping */
  #ifndef strdup
  #define strdup _strdup
  #endif

  /* localtime_r replacement using localtime_s */
  static inline struct tm *gcn10_localtime_r(const time_t *t, struct tm *out) {
    return (localtime_s(out, t) == 0) ? out : NULL;
  }
  #ifndef localtime_r
  #define localtime_r gcn10_localtime_r
  #endif
#endif

#endif /* GCN10_COMPAT_H */