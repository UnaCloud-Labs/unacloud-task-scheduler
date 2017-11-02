/*
 * Unacloud scheduler class
 * 2017 - COMIT Research group, Universidad de los Andes
 *
 */

#include <linux/sched/prio.h>

#ifdef CONFIG_UNACLOUD

#define UNACLOUD_PRIO		141

/*
 * Returns true if the priority is the defined for unacloud tasks
 */
static inline int unacloud_prio(int prio)
{
	if (prio == UNACLOUD_PRIO) {
		return 1;
	}
	return 0;
}

/*
 * Returns if a task is managed by the unacloud scheduler class
 */
static inline int unacloud_task(struct task_struct *p)
{
	return unacloud_prio(p->prio);
}

#endif
