#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STUDENTS 1000
#define MAX_EVENTS 2000
#define SCANNERS 3
#define BELT_CAP 2
#define HANDLERS 2

// Synchronization primitives compatible with both macOS and Linux
typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int count;
} Sem;

static void sem_custom_init(Sem *s, int val) {
  pthread_mutex_init(&s->mutex, NULL);
  pthread_cond_init(&s->cond, NULL);
  s->count = val;
}

static void sem_custom_wait(Sem *s) {
  pthread_mutex_lock(&s->mutex);
  while (s->count <= 0) {
    pthread_cond_wait(&s->cond, &s->mutex);
  }
  s->count--;
  pthread_mutex_unlock(&s->mutex);
}

static void sem_custom_post(Sem *s) {
  pthread_mutex_lock(&s->mutex);
  s->count++;
  pthread_cond_signal(&s->cond);
  pthread_mutex_unlock(&s->mutex);
}

static void sem_custom_destroy(Sem *s) {
  pthread_mutex_destroy(&s->mutex);
  pthread_cond_destroy(&s->cond);
}

typedef struct ThreadControl {
  pthread_t thread;
  int id;
  int is_student;
  int is_officer;
  int is_handler;
  Sem sem_worker;
  int terminated;
} ThreadControl;

static Sem sem_sched;
static ThreadControl *current_thread = NULL;

static void yield_to_scheduler(void) {
  sem_custom_post(&sem_sched);
  sem_custom_wait(&current_thread->sem_worker);
}

static void finish_thread(void) {
  current_thread->terminated = 1;
  sem_custom_post(&sem_sched);
}

typedef enum { ACTOR_STUDENT, ACTOR_OFFICER } ActorType;

typedef struct {
  int time;
  ActorType type;
  int id;
} InputEvent;

static InputEvent input_events[MAX_EVENTS];
static int num_events = 0;

static int cur_time = 0;
static int officer_in = 0;
static int officer_wait = 0;
static int approve_time = -1;
static int officer_leave_time = -1;
static int scanners_busy = 0;
static int belt_cnt = 0;
static int belt[BELT_CAP];
static int handler_next = 1;

typedef struct {
  int id;
  int entered;
  int enter_time;
  int scanning;
  int scan_end;
  int scanned;
  int stamped;
  int stamp_time;
  int left_hall;
  int bag_loaded;
  int load_time;
  int bag_collected;
} Student;

static Student stu[MAX_STUDENTS];
static ThreadControl student_tc[MAX_STUDENTS];
static ThreadControl officer_tc;
static ThreadControl handler_tc[HANDLERS];

static int wait_q[MAX_STUDENTS];
static int w_head = 0, w_tail = 0;

static int pending_arr[MAX_STUDENTS];
static int pa_head = 0, pa_tail = 0;

static int load_q[MAX_STUDENTS];
static int l_head = 0, l_tail = 0;

typedef enum {
  ACT_STU_ENTER,
  ACT_STU_SCAN,
  ACT_STU_STAMP,
  ACT_STU_LEAVE,
  ACT_STU_COLLECT,
  ACT_OFF_ENTER,
  ACT_OFF_APPROVE,
  ACT_OFF_LEAVE,
  ACT_HAND_LOAD,
  ACT_TERMINATE
} ActionType;

static ActionType next_action;
static int action_student_id;

static void *student_worker(void *arg) {
  ThreadControl *self = (ThreadControl *)arg;
  sem_custom_wait(&self->sem_worker);

  while (1) {
    if (next_action == ACT_STU_ENTER) {
      printf("%d Student %d enters\n", cur_time, self->id);
      stu[self->id].id = self->id;
      stu[self->id].entered = 1;
      stu[self->id].enter_time = cur_time;
      wait_q[w_tail++] = self->id;
    } else if (next_action == ACT_STU_SCAN) {
      printf("%d Student %d scans passport\n", cur_time, self->id);
      stu[self->id].scanning = 1;
      stu[self->id].scan_end = cur_time + 2;
      scanners_busy++;
    } else if (next_action == ACT_STU_STAMP) {
      printf("%d Student %d receives stamp\n", cur_time, self->id);
      stu[self->id].stamped = 1;
      stu[self->id].stamp_time = cur_time;
    } else if (next_action == ACT_STU_LEAVE) {
      printf("%d Student %d leaves hall\n", cur_time, self->id);
      stu[self->id].left_hall = 1;
    } else if (next_action == ACT_STU_COLLECT) {
      printf("%d Student %d collects bag\n", cur_time, self->id);
      stu[self->id].bag_collected = 1;
      for (int j = 0; j < belt_cnt; j++) {
        if (belt[j] == self->id) {
          for (int k = j; k < belt_cnt - 1; k++) {
            belt[k] = belt[k + 1];
          }
          belt_cnt--;
          break;
        }
      }
      finish_thread();
      break;
    } else if (next_action == ACT_TERMINATE) {
      finish_thread();
      break;
    }
    yield_to_scheduler();
  }
  return NULL;
}

static void *officer_worker(void *arg) {
  ThreadControl *self = (ThreadControl *)arg;
  sem_custom_wait(&self->sem_worker);

  while (1) {
    if (next_action == ACT_OFF_ENTER) {
      printf("%d Immigration officer 1 enters\n", cur_time);
      officer_in = 1;
      officer_wait = 1;
    } else if (next_action == ACT_OFF_APPROVE) {
      printf("%d Immigration officer 1 approves\n", cur_time);
      officer_wait = 0;
      approve_time = cur_time;
    } else if (next_action == ACT_OFF_LEAVE) {
      printf("%d Immigration officer 1 leaves\n", cur_time);
      officer_in = 0;
      approve_time = -1;
    } else if (next_action == ACT_TERMINATE) {
      finish_thread();
      break;
    }
    yield_to_scheduler();
  }
  return NULL;
}

static void *handler_worker(void *arg) {
  ThreadControl *self = (ThreadControl *)arg;
  sem_custom_wait(&self->sem_worker);

  while (1) {
    if (next_action == ACT_HAND_LOAD) {
      int sid = action_student_id;
      printf("%d Baggage handler %d loads bag for Student %d\n", cur_time,
             self->id, sid);
      handler_next = (handler_next % HANDLERS) + 1;
      belt[belt_cnt++] = sid;
      stu[sid].bag_loaded = 1;
      stu[sid].load_time = cur_time;
    } else if (next_action == ACT_TERMINATE) {
      finish_thread();
      break;
    }
    yield_to_scheduler();
  }
  return NULL;
}

static void dispatch(ThreadControl *tc, ActionType act, int sid) {
  next_action = act;
  action_student_id = sid;
  current_thread = tc;
  sem_custom_post(&tc->sem_worker);
  sem_custom_wait(&sem_sched);
}

int main(void) {
  int N;
  if (scanf("%d", &N) != 1) {
    return 0;
  }

  int student_created[MAX_STUDENTS];
  memset(student_created, 0, sizeof(student_created));
  memset(stu, 0, sizeof(stu));

  sem_custom_init(&sem_sched, 0);

  for (int i = 0; i < N; i++) {
    int t, id;
    char actor[64];
    if (scanf("%d %63s", &t, actor) != 2) {
      break;
    }
    if (strcmp(actor, "Student") == 0) {
      if (scanf("%d", &id) != 1)
        break;
      input_events[num_events].time = t;
      input_events[num_events].type = ACTOR_STUDENT;
      input_events[num_events].id = id;
      num_events++;

      if (!student_created[id]) {
        student_created[id] = 1;
        student_tc[id].id = id;
        student_tc[id].is_student = 1;
        sem_custom_init(&student_tc[id].sem_worker, 0);
        pthread_create(&student_tc[id].thread, NULL, student_worker,
                       &student_tc[id]);
      }
    } else {
      char officer_word[32];
      if (scanf("%31s %d", officer_word, &id) != 2)
        break;
      input_events[num_events].time = t;
      input_events[num_events].type = ACTOR_OFFICER;
      input_events[num_events].id = id;
      num_events++;
    }
  }

  officer_tc.id = 1;
  officer_tc.is_officer = 1;
  sem_custom_init(&officer_tc.sem_worker, 0);
  pthread_create(&officer_tc.thread, NULL, officer_worker, &officer_tc);

  for (int i = 0; i < HANDLERS; i++) {
    handler_tc[i].id = i + 1;
    handler_tc[i].is_handler = 1;
    sem_custom_init(&handler_tc[i].sem_worker, 0);
    pthread_create(&handler_tc[i].thread, NULL, handler_worker, &handler_tc[i]);
  }

  int idx = 0;

  while (1) {
    // 1. Finish scans
    for (int i = 0; i < MAX_STUDENTS; i++) {
      if (stu[i].scanning && stu[i].scan_end == cur_time) {
        stu[i].scanning = 0;
        stu[i].scanned = 1;
        scanners_busy--;
      }
    }

    // 2. Officer approval
    if (officer_in && officer_wait) {
      int all_scanned = 1;
      for (int i = 0; i < MAX_STUDENTS; i++) {
        if (stu[i].entered && !stu[i].left_hall && !stu[i].scanned) {
          all_scanned = 0;
          break;
        }
      }
      if (all_scanned && scanners_busy == 0) {
        dispatch(&officer_tc, ACT_OFF_APPROVE, 0);
      }
    }

    // 3. Give stamps and officer leaves
    if (approve_time != -1 && cur_time == approve_time + 1) {
      for (int i = 0; i < MAX_STUDENTS; i++) {
        if (stu[i].entered && !stu[i].left_hall && stu[i].scanned &&
            !stu[i].stamped) {
          dispatch(&student_tc[i], ACT_STU_STAMP, i);
          load_q[l_tail++] = i;
        }
      }
      dispatch(&officer_tc, ACT_OFF_LEAVE, 0);
      officer_leave_time = cur_time;
    }

    // 4. Handlers load ready bags onto belt
    while (belt_cnt < BELT_CAP && l_head < l_tail &&
           cur_time >= stu[load_q[l_head]].stamp_time + 1) {
      int sid = load_q[l_head++];
      int hid = handler_next;
      dispatch(&handler_tc[hid - 1], ACT_HAND_LOAD, sid);
    }

    // 5. Students leave hall
    for (int i = 0; i < MAX_STUDENTS; i++) {
      if (stu[i].stamped && !stu[i].left_hall && !officer_in &&
          cur_time >= stu[i].stamp_time + 1) {
        dispatch(&student_tc[i], ACT_STU_LEAVE, i);
      }
    }

    // 6. Release blocked arrivals after officer leaves (next time unit)
    if (!officer_in && pa_head < pa_tail &&
        cur_time >= officer_leave_time + 1) {
      while (pa_head < pa_tail) {
        int sid = pending_arr[pa_head++];
        dispatch(&student_tc[sid], ACT_STU_ENTER, sid);
      }
    }

    // 7. Input events at cur_time
    while (idx < num_events && input_events[idx].time == cur_time) {
      if (input_events[idx].type == ACTOR_STUDENT) {
        int sid = input_events[idx].id;
        if (officer_in || cur_time < officer_leave_time + 1) {
          pending_arr[pa_tail++] = sid;
        } else {
          dispatch(&student_tc[sid], ACT_STU_ENTER, sid);
        }
      } else if (input_events[idx].type == ACTOR_OFFICER) {
        dispatch(&officer_tc, ACT_OFF_ENTER, 0);
      }
      idx++;
    }

    // 8. Collect bags
    for (int i = 0; i < belt_cnt;) {
      int sid = belt[i];
      if (cur_time >= stu[sid].load_time + 1 && stu[sid].left_hall &&
          !stu[sid].bag_collected) {
        dispatch(&student_tc[sid], ACT_STU_COLLECT, sid);
        continue;
      }
      i++;
    }

    // 9. Assign scanners
    while (scanners_busy < SCANNERS && w_head < w_tail) {
      if (cur_time < stu[wait_q[w_head]].enter_time + 1) {
        break;
      }
      int sid = wait_q[w_head++];
      dispatch(&student_tc[sid], ACT_STU_SCAN, sid);
    }

    // Termination check
    int unfinished = 0;
    for (int i = 0; i < MAX_STUDENTS; i++) {
      if (student_created[i] && !stu[i].bag_collected) {
        unfinished = 1;
        break;
      }
    }

    if (idx >= num_events && !officer_in && scanners_busy == 0 &&
        w_head == w_tail && l_head == l_tail && belt_cnt == 0 && !unfinished) {
      break;
    }

    cur_time++;
  }

  // Terminate persistent threads
  dispatch(&officer_tc, ACT_TERMINATE, 0);
  for (int i = 0; i < HANDLERS; i++) {
    dispatch(&handler_tc[i], ACT_TERMINATE, 0);
  }

  // Clean join
  for (int i = 0; i < MAX_STUDENTS; i++) {
    if (student_created[i]) {
      pthread_join(student_tc[i].thread, NULL);
      sem_custom_destroy(&student_tc[i].sem_worker);
    }
  }
  pthread_join(officer_tc.thread, NULL);
  sem_custom_destroy(&officer_tc.sem_worker);
  for (int i = 0; i < HANDLERS; i++) {
    pthread_join(handler_tc[i].thread, NULL);
    sem_custom_destroy(&handler_tc[i].sem_worker);
  }
  sem_custom_destroy(&sem_sched);

  return 0;
}
