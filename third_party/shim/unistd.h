#ifndef VO_SHIM_UNISTD_H
#define VO_SHIM_UNISTD_H
typedef int pid_t;
pid_t getpid(void); /* the random generator's fork protection (vo forks tasks) */
#endif
