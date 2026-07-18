#include "define.h"
#include "syscall.h"
#include "interrupt.h"
#include "handler.h"
#include "kernel.h"
#include "serial.h"
#include "lib.h"
#include "memory.h"

#define THREAD_NUM       6
#define THREAD_NAME_SIZE 15
#define PRIORITY_NUM 16

typedef struct _picox_context{
  uint32_t *sp;
}picox_context;

typedef struct _picox_thread{
  struct _picox_thread *next;
  char name[THREAD_NAME_SIZE + 1];
  int priority;
  char *stack;
  uint32_t flags;
#define PICOX_THREAD_FLAG_READY (1 << 0)
  struct{
    picox_func_t func;
    int argc;
    char **argv;
  }init;
  struct{
    picox_syscall_type_t type;
    picox_syscall_param_t *param;
  }syscall;
  picox_context context;
}picox_thread;

typedef struct _picox_msgbuf{
  struct _picox_msgbuf *next;
  picox_thread *sender;
  struct {
    int size;
    char *p;
  }param;
}picox_msgbuf;

typedef struct _picox_msgbox{
  picox_thread *receiver; //受信待ち状態のスレッド
  picox_msgbuf *head;
  picox_msgbuf *tail;
}picox_msgbox;

static struct{
  picox_thread *head;
  picox_thread *tail;
} readyque[PRIORITY_NUM];

static picox_thread *current;
static picox_thread threads[THREAD_NUM];
static picox_handler_t handlers[SOFTVEC_TYPE_NUM];
static picox_msgbox msgboxs[MSGBOX_ID_NUM];

extern char userstack, userstack_end;
static char *thread_stack = &userstack;

__attribute__((naked, noreturn)) void dispatch(picox_context *context){
  __asm__ volatile (
    ".syntax unified               \n"
    "ldr  r0, [r0]               \n"    // r0←context
    "ldmia r0!, {r4-r7}          \n"    // 先頭16byte読み出す
    "mov  r8, r4                 \n"    
    "mov  r9, r5                 \n"
    "mov  r10, r6                \n"
    "mov  r11, r7                \n"
    "ldmia r0!, {r4-r7}          \n"    // 次の16byte読み出す
    "ldmia r0!, {r2-r3}          \n"    // r2=EXC_RETURN, r3=CONTROL
    "msr  control, r3            \n"    // control←r3
    "isb                         \n"    // パイプラインを破棄→CPUが再度メモリから命令をフェッチしなおすことを強制
    "msr  psp, r0                \n"    // r0はハードウェア例外フレーム
    "bx   r2                     \n"    // 例外復帰
    ".syntax divided               \n"
  );
}

static int getcurrent(void){
  if (current == NULL) return -1;
  if (!(current->flags & PICOX_THREAD_FLAG_READY)) return 1;
  readyque[current->priority].head = current->next;
  if (readyque[current->priority].head == NULL) readyque[current->priority].tail = NULL;
  current->flags &= ~PICOX_THREAD_FLAG_READY;
  current->next = NULL;
  return 0;
}

static int putcurrent(void){
  if (current == NULL) return -1;
  if (current->flags & PICOX_THREAD_FLAG_READY) return 1;
  if (readyque[current->priority].tail) readyque[current->priority].tail->next = current;
  else readyque[current->priority].head = current;
  readyque[current->priority].tail = current;
  current->flags |= PICOX_THREAD_FLAG_READY;
  return 0;
}

static void thread_end(void){
  picox_exit();
}

static void thread_init(picox_thread *thp){
  thp->init.func(thp->init.argc, thp->init.argv);
  thread_end();
}

static picox_thread_id_t thread_run(picox_func_t func, char *name, int priority, int stacksize, int argc, char *argv[]){ //初期スレッド作成
  int i;
  picox_thread *thp;
  uint32_t *sp;

  if (!func || stacksize < 128) return (picox_thread_id_t)-1;
  stacksize = (stacksize + 7) & ~7; //アラインメント調整
  if (thread_stack + stacksize > &userstack_end)
    return (picox_thread_id_t)-1;
  for (i = 0; i < THREAD_NUM; i++) {
    if (!threads[i].init.func) {
      thp = &threads[i];
      break;
    }
  }
  if (!thp) return (picox_thread_id_t)-1;
  memset(thp, 0, sizeof(*thp));
  thp->next = NULL;
  strcpy(thp->name, name);
  thp->priority = priority;
  thp->init.func = func;
  thp->init.argc = argc;
  thp->init.argv = argv;
  memset(thread_stack, 0, stacksize);
  thread_stack += stacksize;
  thp->stack = thread_stack;
  sp = (uint32_t *)thp->stack;
  /* Cortex-Mのハードウェア例外フレーム（高アドレス側から作る）。 */
  *(--sp) = 0x01000000u;             // xPSR: Thumb bit
  *(--sp) = (uint32_t)thread_init;   // PC
  *(--sp) = (uint32_t)thread_end;    // LR
  *(--sp) = 0;                       // r12
  *(--sp) = 0;                       // r3
  *(--sp) = 0;                       // r2
  *(--sp) = 0;                       // r1
  *(--sp) = (uint32_t)thp;           // r0

  *(--sp) = 2;                       // CONTROL: Thread modeでPSP
  *(--sp) = 0xFFFFFFFDu;             // EXC_RETURN: ThreadMode + PSP
  *(--sp) = 0;          // r7
  *(--sp) = 0;          // r6 
  *(--sp) = 0;          // r5
  *(--sp) = 0;          // r4 
  *(--sp) = 0;          // r11
  *(--sp) = 0;          // r10
  *(--sp) = 0;          // r9 
  *(--sp) = 0;          // r8 
  thp->context.sp = sp;
  putcurrent();
  current = thp;
  putcurrent();
  return (picox_thread_id_t)thp;
}

static int thread_exit(void){
  serial_puts(current->name);
  serial_puts(" EXIT.\n");
  memset(current, 0, sizeof(*current));
  return 0;
}

int thread_wait(void){
  putcurrent();
  return 0;
}

int thread_sleep(void){
  return 0;
}

int thread_wakeup(picox_thread_id_t id){
  putcurrent();
  current = (picox_thread *)id;
  putcurrent();
  return 0;
}

picox_thread_id_t thread_getid(void){
  putcurrent();
  return (picox_thread_id_t)current;
}

int thread_chpri(int priority){
  int old = current->priority;
  if(priority > 0) current->priority = priority;
  putcurrent();
  return old;
}

void *thread_malloc(int size){
  putcurrent();
  return picoxmem_alloc(size);
}

int thread_free(void *p){
  picoxmem_free(p);
  putcurrent();
  return 0;
}

static void sendmsg(picox_msgbox *mboxp, picox_thread *thp, int size, char *p){
  picox_msgbuf *mp;
  mp = (picox_msgbuf *)picoxmem_alloc(sizeof(*mp));
  if(mp == NULL) picox_sysdown();
  mp->next = NULL;
  mp->sender = thp;
  mp->param.size = size;
  mp->param.p = p;
  if(mboxp->tail){
    mboxp->tail->next = mp;
  }else{
    mboxp->head = mp;
  }
  mboxp->tail = mp;
}

static void recvmsg(picox_msgbox *mboxp){
  picox_msgbuf *mp;
  picox_syscall_param_t *p;
  /* メッセージ・ボックスの先頭にあるメッセージを抜き出す */
  mp = mboxp->head;
  mboxp->head = mp->next;
  if (mboxp->head == NULL) mboxp->tail = NULL;
  mp->next = NULL;
  /* メッセージを受信するスレッドに返す値を設定する */
  p = mboxp->receiver->syscall.param;
  p->un.recv.ret = (picox_thread_id_t)mp->sender;
  if (p->un.recv.sizep) *(p->un.recv.sizep) = mp->param.size;
  if (p->un.recv.pp) *(p->un.recv.pp) = mp->param.p;
  /* 受信待ちスレッドはいなくなったので，NULLに戻す */
  mboxp->receiver = NULL;
  /* メッセージ・バッファの解放 */
  picoxmem_free(mp);
}

static int thread_send(picox_msgbox_id_t id, int size, char *p){
  picox_msgbox *mboxp = &msgboxs[id];
  putcurrent();
  sendmsg(mboxp, current, size, p);
  if(mboxp->receiver){
    current = mboxp->receiver;
    recvmsg(mboxp);
    putcurrent();
  }
  return size;
}

static picox_thread_id_t thread_recv(picox_msgbox_id_t id, int *sizep, char **pp){
  picox_msgbox *mboxp =&msgboxs[id];
  if(mboxp->receiver) picox_sysdown(); //他のスレッドがすでに受信待ちしている
  mboxp->receiver = current;  //受信待ちスレッドに設定
  if(mboxp->head == NULL){  //メッセージボックスにメッセージがないときスレッドをスリープさせる
    return -1;
  }
  recvmsg(mboxp);
  putcurrent();
  return current->syscall.param->un.recv.ret;
}

static void thread_intr(softvec_type_t type, uint32_t *sp);

static int thread_setintr(softvec_type_t type, picox_handler_t handler){
  softvec_setintr(type, thread_intr);
  handlers[type] = handler;
  putcurrent();
  return 0;
}

static void call_functions(picox_syscall_type_t type, picox_syscall_param_t *p){
  switch (type){
    case SYSCALL_TYPE_RUN:
      p->un.run.ret = thread_run(p->un.run.func, p->un.run.name, p->un.run.priority, p->un.run.stacksize, p->un.run.argc, p->un.run.argv);
      break;
    case SYSCALL_TYPE_EXIT:
      thread_exit();
      break;
    case SYSCALL_TYPE_WAIT:
      p->un.wait.ret = thread_wait();
      break;
    case SYSCALL_TYPE_SLEEP:
      p->un.sleep.ret = thread_sleep();
      break;
    case SYSCALL_TYPE_WAKEUP:
      p->un.wakeup.ret = thread_wakeup(p->un.wakeup.id);
      break;
    case SYSCALL_TYPE_GETID:
      p->un.getid.ret = thread_getid();
      break;
    case SYSCALL_TYPE_CHPRI:
      p->un.chpri.ret = thread_chpri(p->un.chpri.priority);
      break;
    case SYSCALL_TYPE_MALLOC:
      p->un.malloc.ret = thread_malloc(p->un.malloc.size);
      break;
    case SYSCALL_TYPE_FREE:
      p->un.free.ret = thread_free(p->un.free.p);
      break;
    case SYSCALL_TYPE_SEND:
      p->un.send.ret = thread_send(p->un.send.id, p->un.send.size, p->un.send.p);
      break;
    case SYSCALL_TYPE_RECV:
      p->un.recv.ret = thread_recv(p->un.recv.id, p->un.recv.sizep, p->un.recv.pp);
      break;
    case SYSCALL_TYPE_SETINTR:
      p->un.setintr.ret = thread_setintr(p->un.setintr.type, p->un.setintr.handler);
      break;
    default:
      break;
  }
}

static void syscall_proc(picox_syscall_type_t type, picox_syscall_param_t *p){
  getcurrent();
  call_functions(type, p);
}

static void srvcall_proc(picox_syscall_type_t type, picox_syscall_param_t *p){
  current = NULL;
  call_functions(type, p);
}

static void schedule(void){
  int i;
  for(i = 0; i < PRIORITY_NUM; i++){
    if(readyque[i].head) break;
  }
  if (i == PRIORITY_NUM) picox_sysdown();
  current = readyque[i].head;
}

static void syscall_intr(void){
  syscall_proc(current->syscall.type, current->syscall.param);
}

static void softerr_intr(void){
  serial_puts(current->name);
  serial_puts(" DOWN.\n");
  getcurrent();
  thread_exit();
}

static void thread_intr(softvec_type_t type, uint32_t *sp){
  current->context.sp = sp;
  if (handlers[type]) handlers[type]();
  schedule();
  dispatch(&current->context);
}

void picox_start(picox_func_t func, char *name, int priority, int stacksize, int argc, char *argv[]){ //初期スレッド生成
  current = NULL;
  memset(readyque, 0, sizeof(readyque));
  memset(threads, 0, sizeof(threads));
  memset(handlers, 0, sizeof(handlers));
  memset(msgboxs, 0, sizeof(msgboxs));
  picoxmem_init();
  thread_setintr(SOFTVEC_TYPE_SYSCALL, syscall_intr);
  thread_setintr(SOFTVEC_TYPE_SOFTERR, softerr_intr);
  current = (picox_thread *)thread_run(func, name, priority, stacksize, argc, argv);
  if ((picox_thread_id_t)current == (picox_thread_id_t)-1) picox_sysdown();
  register picox_context *context __asm__("r0");
  context = &current->context;
  __asm__ volatile ("svc #1" : "+r"(context));
}

void picox_sysdown(void){
  INTR_DISABLE();
  serial_puts("system error!\n");
  while(1);
}

void picox_syscall(picox_syscall_type_t type, picox_syscall_param_t *param){
  current->syscall.type = type;
  current->syscall.param = param;
  __asm__ volatile ("svc #0");
}

void picox_srvcall(picox_syscall_type_t type, picox_syscall_param_t *param){
  srvcall_proc(type, param);
}