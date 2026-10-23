#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STUDENTS 1000
#define MAX_EVENTS 2000
#define SCANNERS 3
#define HANDLERS 2
#define BELT_CAP 2

typedef struct {
  int time;
  char actor[32];
  int id;
} InputEvent;

typedef struct {
  int id;
  int entered;
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

int main() {
  int N;
  if (scanf("%d", &N) != 1)
    return 0;
  InputEvent ev[MAX_EVENTS];
  for (int i = 0; i < N; i++)
    scanf("%d %31s %d", &ev[i].time, ev[i].actor, &ev[i].id);
  Student stu[MAX_STUDENTS];
  memset(stu, 0, sizeof(stu));
  int cur = 0;
  int idx = 0;
  int officer_in = 0;
  int officer_wait = 0;
  int approve_time = -1;
  int scanners_busy = 0;
  int wait_q[MAX_STUDENTS];
  int w_head = 0, w_tail = 0;
  int pending_arr[MAX_STUDENTS];
  int pa_head = 0, pa_tail = 0;
  int load_q[MAX_STUDENTS];
  int l_head = 0, l_tail = 0;
  int belt[BELT_CAP];
  int belt_cnt = 0;
  int handler_next = 1;
  while (1) {

    for (int i = 0; i < MAX_STUDENTS; i++)
      if (stu[i].scanning && stu[i].scan_end == cur) {
        stu[i].scanning = 0;
        stu[i].scanned = 1;
        scanners_busy--;
      }

    if (officer_in && officer_wait) {
      int all_scanned = 1;
      for (int i = 0; i < MAX_STUDENTS; i++)
        if (stu[i].entered && !stu[i].scanned) {
          all_scanned = 0;
          break;
        }
      if (all_scanned && scanners_busy == 0) {
        printf("%d Immigration officer 1 approves\n", cur);
        officer_wait = 0;
        approve_time = cur;
      }
    }

    if (approve_time != -1 && cur == approve_time + 1) {
      for (int i = 0; i < MAX_STUDENTS; i++)
        if (stu[i].entered && stu[i].scanned && !stu[i].stamped) {
          printf("%d Student %d receives stamp\n", cur, stu[i].id);
          stu[i].stamped = 1;
          stu[i].stamp_time = cur;
        }
    }

    for (int i = 0; i < MAX_STUDENTS; i++)
      if (stu[i].stamped && !stu[i].left_hall && cur == stu[i].stamp_time + 1) {
        printf("%d Student %d leaves hall\n", cur, stu[i].id);
        stu[i].left_hall = 1;
        load_q[l_tail++] = stu[i].id;
      }

    while (belt_cnt < BELT_CAP && l_head < l_tail) {
      int sid = load_q[l_head++];
      printf("%d Baggage handler %d loads bag for Student %d\n", cur,
             handler_next, sid);
      handler_next = (handler_next % HANDLERS) + 1;
      belt[belt_cnt++] = sid;
      stu[sid].bag_loaded = 1;
      stu[sid].load_time = cur;
    }

    for (int i = 0; i < belt_cnt;) {
      int sid = belt[i];
      if (cur == stu[sid].load_time + 1 && stu[sid].left_hall &&
          !stu[sid].bag_collected) {
        printf("%d Student %d collects bag\n", cur, sid);
        stu[sid].bag_collected = 1;
        for (int j = i; j < belt_cnt - 1; j++)
          belt[j] = belt[j + 1];
        belt_cnt--;
        continue;
      }
      i++;
    }

    if (approve_time != -1 && cur == approve_time + 1) {
      printf("%d Immigration officer 1 leaves\n", cur);
      officer_in = 0;
      approve_time = -1;
    }

    while (idx < N && ev[idx].time == cur) {
      if (strncmp(ev[idx].actor, "Student", 7) == 0) {
        if (officer_in) {
          pending_arr[pa_tail++] = ev[idx].id;
        } else {
          printf("%d Student %d enters\n", cur, ev[idx].id);
          stu[ev[idx].id].id = ev[idx].id;
          stu[ev[idx].id].entered = 1;
          wait_q[w_tail++] = ev[idx].id;
        }
      } else if (strncmp(ev[idx].actor, "Immigration", 11) == 0) {
        printf("%d Immigration officer 1 enters\n", cur);
        officer_in = 1;
        officer_wait = 1;
        int any = 0;
        for (int i = 0; i < MAX_STUDENTS; i++)
          if (stu[i].entered) {
            any = 1;
            break;
          }
        if (!any) {
          printf("%d Immigration officer 1 approves\n", cur);
          officer_wait = 0;
          approve_time = cur;
        }
      }
      idx++;
    }

    if (!officer_in && pa_head < pa_tail) {
      while (pa_head < pa_tail) {
        int sid = pending_arr[pa_head++];
        printf("%d Student %d enters\n", cur, sid);
        stu[sid].id = sid;
        stu[sid].entered = 1;
        wait_q[w_tail++] = sid;
      }
    }

    while (scanners_busy < SCANNERS && w_head < w_tail) {
      int sid = wait_q[w_head++];
      printf("%d Student %d scans passport\n", cur, sid);
      stu[sid].scanning = 1;
      stu[sid].scan_end = cur + 2;
      scanners_busy++;
    }

    int unfinished = 0;
    for (int i = 0; i < MAX_STUDENTS; i++)
      if (stu[i].entered && !stu[i].bag_collected) {
        unfinished = 1;
        break;
      }
    if (idx >= N && !officer_in && scanners_busy == 0 && w_head == w_tail &&
        l_head == l_tail && belt_cnt == 0 && !unfinished)
      break;
    cur++;
  }
  return 0;
}
