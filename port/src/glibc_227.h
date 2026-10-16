/*
 * THE LINUX RELEASE ONLY (cmake -DOS_GLIBC_227=ON, tools/release_linux.sh): every file is compiled
 * with this in front, so that pow, exp and log are taken in their first versions, which every
 * glibc still has -- the versions of glibc 2.29 (2019) would make the program refuse to start on
 * the distributions of 2018 the release is for (README: glibc 2.27).
 */
#if defined(__x86_64__) && defined(__linux__)
__asm__(".symver pow,pow@GLIBC_2.2.5");
__asm__(".symver exp,exp@GLIBC_2.2.5");
__asm__(".symver log,log@GLIBC_2.2.5");
#endif
