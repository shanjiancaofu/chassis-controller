#ifndef CHASSIS_KERNEL_CRITICAL_H
#define CHASSIS_KERNEL_CRITICAL_H

void kernel_critical_enter(void);
void kernel_critical_exit(void);
void kernel_interrupts_disable(void);

#endif
