#include <sched/run_queue.h>

#include <core/panic.h>
#include <memory/kmem_cache.h>
#include <sync/spinlock.h>

static struct kmem_cache rq_node_cache;
static bool rq_cache_initialised;
static struct spinlock rq_cache_lock = SPINLOCK_INIT;

static void ensure_rq_cache_initialised(void) {
  if (!rq_cache_initialised) {
    bool irq = spin_lock_irqsave(&rq_cache_lock);
    if (!rq_cache_initialised) {
      if (kmem_cache_init(&rq_node_cache, "run_queue_node",
                          sizeof(struct run_queue_node)) != 0) {
        kpanic("Failed to initialise run queue node cache.\n");
      }
      rq_cache_initialised = true;
    }
    spin_unlock_irqrestore(&rq_cache_lock, irq);
  }
}

void run_queue_init(struct run_queue* rq) {
  spinlock_init(&rq->lock);
  rq->node_count = 0;
  rq->head = NULL;
  rq->tail = NULL;
  ensure_rq_cache_initialised();
}

void run_queue_push(struct run_queue* rq, struct task* task) {
  if (rq == NULL || task == NULL) {
    return;
  }

  ensure_rq_cache_initialised();

  struct run_queue_node* node = kmem_cache_zalloc(&rq_node_cache);
  if (node == NULL) {
    kpanic("Failed to allocate run queue node.\n");
  }

  node->task = task;
  node->next = NULL;

  bool irq = spin_lock_irqsave(&rq->lock);

  node->prev = rq->tail;
  if (rq->tail != NULL) {
    rq->tail->next = node;
  } else {
    rq->head = node;
  }
  rq->tail = node;
  rq->node_count++;

  spin_unlock_irqrestore(&rq->lock, irq);
}

struct task* run_queue_pop(struct run_queue* rq) {
  if (rq == NULL) {
    return NULL;
  }

  bool irq = spin_lock_irqsave(&rq->lock);

  struct run_queue_node* popped = rq->head;
  if (popped == NULL) {
    spin_unlock_irqrestore(&rq->lock, irq);
    return NULL;
  }

  rq->head = popped->next;
  if (rq->head == NULL) {
    rq->tail = NULL;
  } else {
    rq->head->prev = NULL;
  }

  struct task* task = popped->task;
  rq->node_count--;

  spin_unlock_irqrestore(&rq->lock, irq);

  kmem_cache_free(&rq_node_cache, popped);

  return task;
}
