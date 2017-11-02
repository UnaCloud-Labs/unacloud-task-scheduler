/*
 * Unacloud scheduler class
 * 2017 - COMIT Research group, Universidad de los Andes
 *
 */

#include <linux/types.h>
#include <linux/latencytop.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/profile.h>
#include <linux/interrupt.h>
#include <linux/mempolicy.h>
#include <linux/migrate.h>
#include <linux/task_work.h>
#include <trace/events/sched.h>

#include "sched.h"

#ifdef CONFIG_UNACLOUD

/*
 * Determine if the unacloud tasks must run
 */
static int should_run_unacloud(void)
{
	int countRunning = 0;
	int countUnacloud = 0;
	int i = 0;
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		if (rq->curr){
			struct task_struct *curr = rq->curr;
			if (curr->sched_class == &unacloud_sched_class)
				countUnacloud++;
			else if (curr->sched_class != &idle_sched_class)
				countRunning++;
		}
	}
	if (countUnacloud > 0) {
		//printk("should_run_unacloud: %d, %d\n", countUnacloud, countRunning);
	}
	return countUnacloud + countRunning < 5;
}

/*
 * Return the task for an unacloud scheduler entity
 */
static inline struct task_struct *unacloud_task_of(struct sched_unacloud_entity *unacloud)
{
	return container_of (unacloud, struct task_struct, unacloud_se);
}

/*
 * Update information of the current unacloud task
 */
static void update_curr_unacloud(struct rq *rq)
{
	//printk("unacloud: update_curr_unacloud RQ: %d\n", rq->cpu);
	struct task_struct *curr = rq->curr;
	u64 delta_exec;

	if (curr->sched_class != &unacloud_sched_class)
		return;

	delta_exec = rq_clock_task(rq) - curr->se.exec_start;
	if (unlikely((s64)delta_exec <= 0))
		return;

	schedstat_set(curr->se.statistics.exec_max, max(curr->se.statistics.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;

	curr->se.exec_start = rq_clock_task(rq);
	cpuacct_charge(curr, delta_exec);
}

/*
 * Pick the next unacloud task
 */
static struct task_struct * _pick_next_task_unacloud(struct rq *rq)
{
	struct sched_unacloud_entity * use;
	list_for_each_entry(use, &rq->unacloud_rq.unacloud_list, run_list) {
		return unacloud_task_of(use);
	}
	return NULL;
}

static struct task_struct * _pick_pull_task_unacloud(struct rq *rq)
{
	struct sched_unacloud_entity * use;
	int pos = 0;
	list_for_each_entry(use, &rq->unacloud_rq.unacloud_list, run_list) {
		if(pos++ > 0)
			return unacloud_task_of(use);
	}
	return NULL;
}

static bool pull_unacloud_task(struct rq *this_rq)
{
	int this_cpu = this_rq->cpu, cpu;
	bool resched = false;
	struct task_struct *p = NULL;
	struct rq *src_rq;
	for_each_possible_cpu(cpu) {
		if (this_cpu == cpu)
			continue;
		src_rq = cpu_rq(cpu);
		double_lock_balance(this_rq, src_rq);
		
		p = _pick_pull_task_unacloud(src_rq);
		if (p) {
			//printk("pull_unacloud_task pulledPID: %d, toRQ: %d, fromRQ: %d\n", p->pid, this_rq->cpu, src_rq->cpu);
	
			WARN_ON(p == src_rq->curr);
			WARN_ON(!task_on_rq_queued(p));
			resched = true;

			deactivate_task(src_rq, p, 0);
			set_task_cpu(p, this_cpu);
			activate_task(this_rq, p, 0);
		}
		
skip: 
		double_unlock_balance(this_rq, src_rq);
		if (resched)
			break;
	}
/*
	if (resched)
	resched_curr(this_rq);*/
	return resched;
}

static struct task_struct * pick_next_task_unacloud(struct rq *rq, struct task_struct *prev)
{
	if (list_empty(&rq->unacloud_rq.unacloud_list)){
		if (!should_run_unacloud())
			return NULL;
		//Need to pull tasks
		lockdep_unpin_lock(&rq->lock);
		bool retry = pull_unacloud_task(rq);
		lockdep_pin_lock(&rq->lock);
		
		if (retry)
			return RETRY_TASK;
		
	} else if (!should_run_unacloud())
		return NULL;
	
	if (list_empty(&rq->unacloud_rq.unacloud_list))
		return NULL;
	
	put_prev_task(rq, prev);
	struct task_struct * ret;
	ret = _pick_next_task_unacloud(rq);
	ret->se.exec_start = rq_clock_task(rq);
	printk("pick_next_task_unacloud PID: %d, RQ: %d\n", ret->pid, rq->cpu);
	return ret;
	
}

/*
 * Enqueue a task in the unacloud run queue
 */
static void enqueue_task_unacloud(struct rq *rq, struct task_struct *p, int flags)
{
	printk("enqueue_task_unacloud PID: %d, RQ: %d\n", p->pid, rq->cpu);
	struct sched_unacloud_entity *unacloud_se = &p->unacloud_se;
	unacloud_se->unacloud_time = 25;
	list_add_tail(&unacloud_se->run_list, &rq->unacloud_rq.unacloud_list);
}

/*
 * The dequeue_task method is called before nr_running is 
 * decreased. We remove the task from the rbtree and
 * update the fair scheduling stats:
 */
static void dequeue_task_unacloud(struct rq *rq, struct task_struct *p, int flags)
{
	printk("dequeue_task_unacloud PID: %d, RQ: %d\n", p->pid, rq->cpu);
	update_curr_unacloud(rq);
	struct sched_unacloud_entity *unacloud_se = &p->unacloud_se;
	list_del(&unacloud_se->run_list);
}

/*
 * sched_yield() is very simple
 *
 * The magic of dealing with the ->skip buddy is in pick_next_entity.
 */
static void yield_task_unacloud(struct rq *rq)
{
	//printk("unacloud: yield_to_task_unacloud RQ: %d\n", rq->cpu);
}

static bool yield_to_task_unacloud(struct rq *rq, struct task_struct *p, bool preempt)
{
	//printk("unacloud: yield_to_task_unacloud PID: %d RQ: %d\n", p->pid, rq->cpu);
	return true;
}

/*
 * Preempt the current task with a newly woken task if needed:
 */
static void check_preempt_wakeup(struct rq *rq, struct task_struct *p, int wake_flags)
{
	//printk("check_preempt_wakeup_unacloud PID: %d RQ: %d\n", p->pid, rq->cpu);
}

/*
 * Account for a descheduled task:
 */
static void put_prev_task_unacloud(struct rq *rq, struct task_struct *prev)
{
	update_curr_unacloud(rq);
	//printk("put_prev_task_unacloud PID: %d RQ: %d\n", prev->pid, rq->cpu);
}


/* 
 * Account for a task changing its policy or group.
 *
 * This routine is mostly called to set cfs_rq->curr field when a task
 * migrates between groups/classes.
 */
static void set_curr_task_unacloud(struct rq *rq)
{
	struct task_struct *p = rq->curr;
	
	//printk("unacloud: set_curr_task_rt START PID: %d RQ: %d\n", p->pid, rq->cpu);
	
	p->se.exec_start = rq_clock_task(rq);
	
	//printk("unacloud: set_curr_task_rt END PID: %d RQ: %d\n", p->pid, rq->cpu);
}

/*
 * scheduler tick hitting a task of our scheduling class:
 */
static void task_tick_unacloud(struct rq *rq, struct task_struct *curr, int queued)
{
	update_curr_unacloud(rq);
	if (curr->policy != SCHED_UNACLOUD)
		return;
	
	curr->unacloud_se.unacloud_time--;
	
	if (curr->unacloud_se.unacloud_time > 0)
		return;
	
	//printk("unacloud: task_tick_unacloud resched PID: %d RQ: %d\n", curr->pid, rq->cpu);
	
	curr->unacloud_se.unacloud_time = 25;
	
	list_del(&curr->unacloud_se.run_list);
	list_add_tail(&curr->unacloud_se.run_list, &rq->unacloud_rq.unacloud_list);
	resched_curr(rq);
}

/*
 * called on fork with the child task as argument from the parent's context
 *  - child not yet on the tasklist
 *  - preemption disabled
 */
static void task_fork_unacloud(struct task_struct *p)
{
	//printk("unacloud: task_fork_unacloud PID: %d\n", p->pid);
}

/*
 * Priority of the task has changed. Check to see if we preempt
 * the current task.
 */
static void prio_changed_unacloud(struct rq *rq, struct task_struct *p, int oldprio)
{
	//printk("unacloud: prio_changed_unacloud PID: %d RQ: %d\n", p->pid, rq->cpu);
}

static void switched_from_unacloud(struct rq *rq, struct task_struct *p)
{
	//printk("unacloud: switched_from_unacloud PID: %d RQ: %d\n", p->pid, rq->cpu);
}



/*
 * We switched to the sched_unacloud class.
 */
static void switched_to_unacloud(struct rq *rq, struct task_struct *p)
{
	//printk("unacloud: switched_to_unacloud PID: %d RQ: %d\n", p->pid, rq->cpu);	
	if (rq->curr == p)
		resched_curr(rq);
	else
		check_preempt_curr(rq, p, 0);
}



static unsigned int get_rr_interval_unacloud(struct rq *rq, struct task_struct *task)
{
	
	//printk("unacloud: get_rr_interval_unacloud: 25 PID: %d RQ: %d\n", task->pid, rq->cpu);
	return 25;
}

__init void init_sched_unacloud_class(void)
{
}

//COSAS SMP
static void record_wakee(struct task_struct *p)
{
	/*
	 * Rough decay (wiping) for cost saving, don't worry
	 * about the boundary, really active task won't care
	 * about the loss.
	 */
	if (time_after(jiffies, current->wakee_flip_decay_ts + HZ)) {
		current->wakee_flips >>= 1;
		current->wakee_flip_decay_ts = jiffies;
	}

	if (current->last_wakee != p) {
		current->last_wakee = p;
		current->wakee_flips++;
	}
}

static void task_waking_unacloud(struct task_struct *p)
{
	//printk("unacloud: task_waking_unacloud, PID: %d\n", p->pid);
	record_wakee(p);
}
static void rq_online_unacloud(struct rq *rq)
{
	//printk("unacloud: rq_online_unacloud RQ: %d\n", rq->cpu);
	//update_sysctl();

	//update_runtime_enabled(rq);
}

static void rq_offline_unacloud(struct rq *rq)
{
	//printk("unacloud: rq_offline_unacloud RQ: %d\n", rq->cpu);
	//update_sysctl();

	/* Ensure any throttled groups are reachable by pick_next_task */
	//unthrottle_offline_cfs_rqs(rq);
}
static int select_task_rq_unacloud(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags)
{
	//printk("unacloud: select_task_rq_unacloud: RQ: %d\n", prev_cpu);
	return (prev_cpu + 1) % nr_cpu_ids; 
}

static void migrate_task_rq_unacloud(struct task_struct *p, int next_cpu)
{
	//printk("unacloud: migrate_task_rq_unacloud: PID: %d\n", p->pid);
	struct sched_entity *se = &p->se;
	se->exec_start = 0;
}

static void task_woken_unacloud(struct rq *rq, struct task_struct *p)
{
	//printk("unacloud: task_woken_unacloud PID: %d RQ: %d\n", p->pid, rq->cpu);
}

static void set_cpus_allowed_unacloud(struct task_struct *p,
	const struct cpumask *new_mask)
{
	//printk("unacloud: set_cpus_allowed_unacloud PID: %d\n", p->pid);
}

static void task_dead_unacloud(struct task_struct *p)
{
	//printk("unacloud: task_dead_unacloud PID: %d\n", p->pid);
} 


/*
 * Unacloud scheduling class
 */
const struct sched_class unacloud_sched_class = {		
	.next = &idle_sched_class,
	.enqueue_task = enqueue_task_unacloud,
	.dequeue_task = dequeue_task_unacloud,
	.yield_task = yield_task_unacloud,
	.yield_to_task = yield_to_task_unacloud,

	.check_preempt_curr = check_preempt_wakeup,

	.pick_next_task = pick_next_task_unacloud,
	.put_prev_task = put_prev_task_unacloud,

	.set_curr_task = set_curr_task_unacloud,
	.task_tick = task_tick_unacloud,
	.task_fork = task_fork_unacloud,
	.task_dead = task_dead_unacloud,
	
	
	
#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_unacloud,
	.migrate_task_rq	= migrate_task_rq_unacloud,

	.rq_online		= rq_online_unacloud,
	.rq_offline		= rq_offline_unacloud,
	.set_cpus_allowed = set_cpus_allowed_unacloud,

	.task_waking		= task_waking_unacloud,
	.task_woken			= task_woken_unacloud,
#endif

	.prio_changed = prio_changed_unacloud,
	.switched_from = switched_from_unacloud,
	.switched_to = switched_to_unacloud,

	.get_rr_interval = get_rr_interval_unacloud,
 
	.update_curr = update_curr_unacloud,
};

/*
 * Initialize the Unacloud runqueue
 */
void init_unacloud_rq(struct unacloud_rq *rq)
{
	INIT_LIST_HEAD(&rq->unacloud_list);
}

#endif
