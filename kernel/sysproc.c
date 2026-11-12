#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#ifdef PGTBL_SOL
#include "riscv.h"
#endif
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0) {
    if (growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > UTOP)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep_prepare(&ticks);
    release(&tickslock);
    sleep();
    acquire(&tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_vmprint(void)
{
  struct proc *p;

  p = myproc();
  vmprint(p->pagetable);
  return 0;
}
#endif

#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 va_start;
  int npages;
  uint64 user_mask_addr;

  // 1. Đọc các tham số truyền vào từ user space
  argaddr(0, &va_start);
  argint(1, &npages);
  argaddr(2, &user_mask_addr);

  // Kiểm tra giới hạn số trang hợp lý (pgtbltest test tới 256 và 4096 trang)
  if (npages < 0 || npages > 4096)
    return -1;

  struct proc *p = myproc();
  
  // Dùng mảng byte làm bộ đệm bitmask trong kernel (4096 trang / 8 = 512 bytes)
  unsigned char mask[512];
  memset(mask, 0, sizeof(mask));

  // 2. Duyệt qua từng trang cần kiểm tra
  for (int i = 0; i < npages; i++) {
    uint64 va = va_start + (uint64)i * PGSIZE;
    if (va >= MAXVA)
      return -1;

    // Tìm PTE tương ứng với địa chỉ ảo va (alloc = 0: không cấp phát thêm bảng mới)
    pte_t *pte = walk(p->pagetable, va, 0);

    // Nếu trang chưa được ánh xạ hoặc không hợp lệ -> báo lỗi -1
    if (pte == 0 || (*pte & PTE_V) == 0)
      return -1;

    // 3. Nếu trang đã được truy cập (bit PTE_A được bật)
    if (*pte & PTE_A) {
      // Bật bit thứ i trong mảng mask
      mask[i / 8] |= (1 << (i % 8));
      // BẮT BUỘC: Xóa bit PTE_A để lần sau kiểm tra lại chính xác
      *pte &= ~PTE_A;
    }
  }

  // 4. Sao chép kết quả bitmask từ kernel buffer sang user buffer
  int mask_bytes = (npages + 7) / 8;
  if (copyout(p->pagetable, p->sz, user_mask_addr, (char *)mask, mask_bytes) < 0)
    return -1;

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

#ifdef LAB_LOCK
uint64
sys_cpupin(void)
{
  struct proc *p = myproc();
  int cpu;

  argint(0, &cpu);
  if (cpu < 0 || cpu >= NCPU)
    return -1;
  acquire(&p->lock);
  p->pincpu = &cpus[cpu];
  release(&p->lock);
  return 0;
}
#endif
