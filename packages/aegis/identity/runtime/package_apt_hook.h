#ifndef AEGIS_PACKAGE_APT_HOOK_H
#define AEGIS_PACKAGE_APT_HOOK_H
// Fixed APT JSON protocol collector. Needs a confined trusted planner process;
// currently callable only through the test probe, never a product endpoint.
int aegis_apt_plan_hook(void);
#endif
