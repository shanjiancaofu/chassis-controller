#ifndef BOOT_APP_LAUNCHER_H
#define BOOT_APP_LAUNCHER_H

#include <stdbool.h>

bool BootAppLauncher_IsApplicationValid(void);
void BootAppLauncher_Jump(void) __attribute__((noreturn));

#endif
