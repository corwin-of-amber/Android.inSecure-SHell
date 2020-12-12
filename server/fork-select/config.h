#ifndef ISHD_CONFIG_H
#define ISHD_CONFIG_H


#ifdef __ANDROID_API__
#define SHELL "/system/xbin/bash"
#else
#define SHELL "/bin/bash"
#endif
#define SHELL_NAME "bash"

#define PORT   "2220"
#define SECRET "<plain-sight>"
#define CTRLTOK "<control>"

#define BUFFERSIZE 64 * 1024


#endif
