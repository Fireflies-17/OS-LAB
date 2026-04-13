# 实验一: Threads and Synchronize (多线程与同步) 实验指导书

本实验指南根据官方 `lab1.pdf` 编写。此向导将带有具体的代码和解决步骤，帮助你一步步完成有关多线程、锁和条件变量同步的实验练习。

## 目录
- [环境准备](#环境准备)
- [Part 1: Locking (锁机制)](#part-1-locking-锁机制)
- [Part 2: Barriers (屏障同步)](#part-2-barriers-屏障同步)
- [提交说明](#提交说明)

---

## 环境准备

本实验**不需要**运行 xv6 系统环境，在任何通用的 Linux 发行版（如虚拟机中的 Ubuntu 或者 WSL）中均可完成。
**重要注意事项**：请务必确保为你的虚拟机**分配多于 1 个 CPU 核心**，这有益于充分观察多核并发在系统中的竞争和同步现象。

---

## Part 1: Locking (锁机制)

在这一部分，你需要通过修改哈希表的存放、获取函数来掌握利用锁和线程提升程序性能与正确性的知识。文件：`ph.c`。

### 1. 编译并体验数据竞争
首先在终端执行以下命令编译 `ph.c`：
```bash
gcc -g -O2 ph.c -pthread
```

我们首先采用**双线程**运行：
```bash
./a.out 2
```
等待运行结束，你会观察到终端输出显示类似如下内容：
`0: X keys missing` / `1: Y keys missing`。因为多个线程未加锁并行执行，导致竞争条件发生了 keys 的覆盖和丢失。

对比使用**单线程**的情况：
```bash
./a.out 1
```
单线程环境下，`put` 操作不会有竞争，因此输出只会是 `0 keys missing`。

### 2. 分析为何丢失 keys（用于完成你的实验报告）
你需要先思考产生数据竞争的底层原因，并将结果写入你的**实验报告**中：
> **问题**：为什么在 2 个或更多线程时会出现键丢失，而在 1 个线程时不会？请找出一种会导致 2 个线程情况下键丢失的事件执行序列。
> **参考答案（供参考填写在报告里）**：当进行多线程操作时，如果线程A和线程B同时将不同的 key 映射到同一个哈希桶并同时执行 `put()` -> `insert()`。比如线程A读取原来的头节点准备插入，在这个瞬间发生线程切换，线程B也读到了同样的头节点，并完成插入。随后再次切换回线程A完成它的插入操作。此时线程B刚刚插入的节点就会被线程A所覆盖，造成了键值对的丢失。单线程下由于指令只会顺序执行，因此不存在这种被干扰的中断交错情况。

### 3. 代码实战 - 第一阶段：添加一把大锁
首先我们要消除那些丢失的 keys。需要用到如下的互斥锁原语。

在 `ph.c` 的开头添加全局互斥锁变量，并在 `main()` 函数中做初始化：
```c
// ... 这里是前面的 #include 内容 ...

// 1. 在这里声明一个全局互斥锁
pthread_mutex_t lock;

// ... 
```
接着向下拉到文件的 `main` 函数，我们需要在创建任何线程之前把它初始化一下：
```c
int
main(int argc, char *argv[])
{
  // ... 其他代码
  srandom(0);
  assert(NKEYS % nthread == 0);

  // 2. 在线程执行逻辑前对锁进行初始化
  pthread_mutex_init(&lock, NULL);

  for (i = 0; i < NKEYS; i++) {
// ...
```

最后，修改 `put()` 函数，将可能有写入冲突的地方上锁：
```c
static 
void put(int key, int value)
{
  int i = key % NBUCKET;
  
  // 3. 每次执行将新 key 加入 table 前上锁
  pthread_mutex_lock(&lock);
  
  insert(key, value, &table[i], table[i]);
  
  // 4. 插入结束后立刻解锁
  pthread_mutex_unlock(&lock);
}
```
保存文件，此时通过 `gcc -g -O2 ph.c -pthread` 重新编译，再运行 `./a.out 2`，丢失丢失的情况就会解决，稳定输出 `0 keys missing`。但是加锁可能会导致程序执行由于串行化而略微变慢。

### 4. 代码实战 - 第二阶段：思考 get 操作是否要加锁？
我们注意到全局互斥锁导致大量毫无干涉的操作也在互相排队等待。
- **思考并修改**：我们可以注意到在本程序里 `get()` 读取操作和初期的 `put()` 插入阶段是被代码里的 barrier 给阻断并**分成两个独立阶段**运行的。这意味着 `get()` 被调用的时候，结构体链表已经非常稳定，不会发生插入变动了。因此在**只读阶段，我们不需要对 `get()` 加任何锁即可使得所有线程保持高并发正确读取！**。

### 5. 代码实战 - 第三阶段：按哈希桶（Bucket）分配多把锁以增强并行化
只把 `put()` 的全局单锁改为“**每桶一锁**”，即如果两个线程要向原本毫无冲突的哈希表不同位置插入数据，它们不应该互相锁死；当且仅当多个线程插入同一个 bucket 时才需要加锁排队。

**最终代码修改（结合并覆盖掉之前的全局锁）**：
将 `ph.c` 中锁的声明改成数组：
```c
// 删去原来的单体锁： pthread_mutex_t lock;
// 改为锁的数组：
pthread_mutex_t locks[NBUCKET];
```

修改 `main` 当中的锁数组初始化：
```c
// 在原来的 pthread_mutex_init(&lock, NULL); 处，用循环替换掉：
for (int j = 0; j < NBUCKET; j++) {
    pthread_mutex_init(&locks[j], NULL);
}
```

修改 `put()` 中的用法，此时将对它所对应位置的那个桶单独上锁：
```c
static 
void put(int key, int value)
{
  int i = key % NBUCKET;
  
  // 对该数组对应的具体位置 i 加独立的锁
  pthread_mutex_lock(&locks[i]);
  
  insert(key, value, &table[i], table[i]);
  
  // 插入结束后解锁
  pthread_mutex_unlock(&locks[i]);
}
```
编译后再次运行 `./a.out 2`，可以观察到 put 操作时的消耗时间几乎对半缩短！多线程性能此时甚至能够接近理论上的加速比。

---

## Part 2: Barriers (屏障同步)

这一部分的实验目的是通过条件变量来实现所有线程的“屏障机制（Barriers）”——也就是所有的线程执行到同一行代码时都需要强制阻塞等待，直到所有的兄弟线程都抵达这行代码时，再全部一起继续往下执行。

### 1. 编译并体验同步问题
我们将操作目标文件 **`barrier.c`**。使用以下命令编译运行测试断言报错：
```bash
gcc -g -O2 -pthread barrier.c
./a.out 2
```
报错：`Assertion failed: (i == t), function thread, file barrier.c, line 55.`
报错是因为某些线程提前抵达了屏障并立刻返回跑飞了，破坏了尚未结算完数据的上一轮轮次结构。

### 2. 代码实战：完善条件变量逻辑
为了修复本问题我们需要在 `barrier_mutex` (互斥锁) 以及 `barrier_cond`（条件变量） 的帮助下修补。

`bstate.nthread`：记录当前到达屏障了的线程总数。
`bstate.round`：记录当前的轮次。

请找到 `barrier.c` 中留空的 **`barrier()`** 函数，并将其按照如下补充完整：

```c
static void 
barrier()
{
  // 1. 各个线程都需要先尝试获取互斥保护锁才能执行判断逻辑
  pthread_mutex_lock(&bstate.barrier_mutex);

  // 记录下当前线程到来时的当前轮数（非常重要，用于后期防止虚假唤醒和跑偏）
  int round = bstate.round;

  // 2. 当前已抵达的线程数 + 1
  bstate.nthread++;

  // 3. 检查自己是不是本轮次中最后到达的一个收尾人？
  if(bstate.nthread == nthread) {
    // 3.1 是最后到达的！开始放行：轮次加一，已到达的线程数重置给下轮准备
    bstate.round++;
    bstate.nthread = 0;
    
    // 3.2 唤醒所有正在 pthread_cond_wait 休眠等候的线程群
    pthread_cond_broadcast(&bstate.barrier_cond);

  } else {
    // 3.3 如果自己不是最后一个，那就应当进入 condition wait 等待。
    // 使用 while(round == bstate.round) 是为了防范提前唤醒或者前面说的“快跑线程”产生的虚假苏醒干扰
    while(round == bstate.round) {
        pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    }
  }

  // 4. 判断和一切结束了，别忘了释放掉锁
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```

### 3. 程序测试
保存 `barrier.c` 同样编译：
```bash
gcc -g -O2 -pthread barrier.c
```
随后，验证任意线程数量不会再因没有拦在屏障上而导致由于不同步抛出的 assertion fail 报错：
```bash
./a.out 1
./a.out 2
./a.out 4
```
只要运行显示 `OK; passed` 即代表你的 barrier 在复杂多线程下也完美拦截了每一轮！

---

## 提交说明

当完成上面两个核心任务之后：
1. 请到智慧树平台上下载给定的**实验报告模板**。
2. 将 **Part 1 当中为何丢失 keys 的思考题（详见 Part 1 中的参考答案）** 整理后填写进入报告。
3. 最后，将修改好的 **`ph.c`**、**`barrier.c`** 源代码以及完成好的**实验报告记录** 这3份内容打包后提交到智慧树作业区。祝实验顺利！
