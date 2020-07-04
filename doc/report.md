# Pintos

## 分工

threads：刘成锴

userprog：李孜睿、柳志轩

vm：郭睿涵



## Project 1 Threads

### 1 alarm测试

主要修改device/timer.c。

重新实现了timer_sleep和timer_interrupt。并在thread中添加了blocked_thread_check函数。

在thread类中添加了ticks_blocked成员，记录此线程被阻塞的时间。

调用timer_sleep函数使线程暂时阻塞（其中调用了thread_block函数），在每次中断时调用一次timer_interrupt函数。

### 2 priority 优先级调度测试

维护就绪队列为一个优先级队列，插入线程的时候保证队列是一个优先级队列，正在运行的线程优先级最高。

以下情况线程加入就绪队列：

1. thread_unblock 线程解除阻塞
2. init_thread  初始化线程
3. thread_yield  线程中断，放弃CPU

为了维护优先级队列，将list_push_back函数改为调用list_insert_ordered函数，根据优先级比较确定插入到队列的位置。

在优先级倒置情况下，当发现高优先级的任务因为低优先级任务占用资源而阻塞时，就将低优先级任务的优先级提升到等待它所占有的资源的最高优先级任务的优先级。

在执行过程中看，在一个线程获取一个锁的时候，如果拥有这个锁的线程优先级比自己低就提高它的优先级，然后在这个线程释放掉这个锁之后把原来拥有这个锁的线程改回原来的优先级。释放一个锁的时候， 将该锁的拥有者改为该线程被捐赠的第二优先级，如果没有其余捐赠者， 则恢复原始优先级。线程需要一个数据结构来记录所有对这个线程有捐赠行为的线程，以及需要一个数据结构，获取这个线程被锁于哪个线程。

实现函数thread_cmp_priority，thread_hold_the_lock，thread_remove_lock，thread_donate_priority，thread_update_priority，synch.c中的lock_cmp_priority，cond_sema_cmp_priority函数。并且重写了synch.c中的lock_acquire， lock_release函数。

### 3 mlfqs 多级反馈队列调度测试

在多级反馈队列调度器中，线程的优先级是通过以下公式动态计算的：

```
priority = PRI_MAX - (recent_cpu/4) - (nice * 2)
recent_cpu = (2*load_avg)/(2*load_avg + 1)*recent_cpu + nice
load_avg= (59/60)*load_avg + (1/60)*ready_threads
```

在thread类中添加了nice，recent_cpu成员，以及load_avg全局变量。

在调度时会从高优先级队列开始选择线程执行， 线程的优先级随着操作系统的运转数据而动态改变。

由于recent_cpu和load_avg是实数，添加了浮点数运算。在fixed_point.h中实现了浮点数及其运算，用前16位作为浮点数的小数部分。

在thread中加入了thread_mlfqs_increase_recent_cpu_by_one, thread_mlfqs_update_priority, thread_mlfqs_update_load_avg_and_recent_cpu函数。实现了thread_set_nice, thread_get_nice, thread_get_load_avg, thread_get_recent_cpu函数。并重新实现了device/timer.c中的timer_interrupt函数。



## Project 2 Userprog

### 1 Argument Pass

在要运行一个用户程序(process_exec)的时候，先创建一个进程，再读取command line，并将参数切割后放到栈上，将可执行文件load到内存里，再跳转到程序初始指令处。

同时也完成了等待进程(process_wait)、退出进程(process_exit)等操作，这些操作同时更改了threads类，使得它可以维护子进程的信息。

在thread_exec中，将正要执行的可执行文件加锁，禁止写入。在thread_exit中，将该可执行文件解锁。

### 2 Syscall

用户程序有时需要借用内核的权限，来进行一些底层的操作，如执行一个新的程序、读写I/O等等。Syscall 提供了一个给用户程序内核权限的接口，并且要进行严格的检查以保证用户程序的所有操作都是合法的。 在pintos中，一共涉及到了11个**syscall**操作，如下所示：

- `exec` 执行一个程序
  - 调用`process_execute` 实现。使用信号量确保父子进程的正确调度。
- `wait` 等待孩子执行完毕
  - 调用 `process_wait` 实现
- `halt` 停机
  - 调用`shutdown_power_off`函数
- `exit` 退出当前进程
  - child需要将自己的返回值和状态告诉father，并设置好返回值，最后调用`thread_exit`让自己的线程退出
- `open` 打开某个文件
  - 调用`filesys_open`函数实现。此处需要一个thread_file_list记录已打开文件
- `create` 创建文件
  - 调用 `filesys_create` 函数实现
- `remove` 删除文件
  - 调用 `filesys_remove` 函数实现
- `file_size` 获得文件的大小
  - 调用 `file_length` 实现
- `read` 读取输入源中指定长度的内容
  - 读取源分为标准输入和文件输入两种。标准输入调用 `input_getc` 内容，文件输入调用 `file_read` 直接读取得到指定大小的内容
- `write` 向输出对象写入指定长度的内容
  - 写入对象分为标准输出和文件输出两种。标准输出调用 `putbuf` 输出内容，文件输出调用 `file_write` 直接写入到指定文件
- `seek` 将文件指针跳转到文件中的指定位置
  -  调用 `file_seek` 实现。
- `tell` 获取文件指针目前在文件中的位置
  -  调用 `file_tell` 实现
- `close` 关闭文件
  -  调用 `file_close` 关闭文件 并且在thread_file_list中删除这个文件句柄。

### 3 未通过测试的原因

调试过程中出现了kernel panic。这个kernel panic我们判断是由进程调度过早出现导致thread的初始化没有完成。但因为我们工作安排出了问题，并没能将其排除掉。



## Project 3 Virtual Memory

### 一. 设计概览

我们的第三个项目，虚拟内存，主要任务就是 page table，frame table 和 swap table的实现


### 二. 设计细节

#### 1. page table

1）维护的信息：每个进程有一个记录虚拟页信息的表，分别记录：
	
- writable：是否可以写回文件系统
- 当前页的所在位置：FRAME（在物理内存中），SWAP（在交换区上），FILE（在文件系统中）
- key：虚拟页地址
- value：物理页地址 或 交换区上的编号 或 文件信息（origin）
- origin：储存来自文件系统的页的对应细节（文件指针、文件偏移量、页对齐信息等）

2）互斥锁的应用：为了保证线程不会冲突，每次对页的操作都会上锁

3）虚拟页的存取：我选用了一个hash表来讲地址和对应的页表联系起来

4）实现了page_init,page_destroy,page_find,page_set_frame,page_install_file,page_unmap这几个函数，完成了页的基本操作，以及与内存的交互以及和物理页的绑定、解绑定

#### 2. frame table

1）这个表是属于整个系统共用的，存储了每一个user program使用的物理地址的信息，包括所述进程的指针，虚拟和物理的页地址，以及一些用于置换算法的标记

2）互斥锁的应用：为了保证线程不会冲突，每次对页的操作（申请物理页，释放物理页）都会上锁

#### 3. swap table

1）这个表也是属于整个系统共用的，用来与硬盘上面的交换区来进行简单的交互，使用block的序号来进行索引访问

### 三. 最终遇到的一些问题

因为我们本地环境的一些问题，以及最后实现进度不尽如人意，虽然相关的测试并没有来得及跑，但是网站上具体布置的任务都完成了。



## 总结反思

李孜睿：完成了userprog的process（用户进程相关）和exception（异常处理）部分。但最终和threads拼接还是产生了kernel panic，没能把bug找出来。主要原因是我对threads的机制不够了解，没有对thread做出正确的改动，导致thread在初始化和进程调度间产生了冲突。

刘成锴：完成了threads部分。在考试周之前花了一些时间配置环境和阅读文档，考试周期间花了一星期时间了解学习、看博客，剩下的时间实现和调试。由于线上跟队友交流存在很多困难，以及mac上的ubuntu虚拟机运行和调试不方便，造成了许多困难。最终只是通过了threads测试，但在3个项目的衔接方面不尽人意。

柳志轩：完成了useprog的syscall的部分。反思是以后在写代码前，先搞好整体框架，规定各种接口的名称，即需要在项目开始前进行算法整体设计，而不是每个人写完后拼接起来。

郭睿涵：时间：一个星期的了解，六天实现，两天调试。完成了virtual memory的部分，实现了page table， frame table和swap table三个表，但是最后由于本地环境以及和前面的交互出现了一些问题，最终没有调试出来。我觉着主要问题在于进程过于仓促，没有和队友打好配合。也许在实现的过程中就实现衔接可以更好地完成任务，而不是每个人写完再强行合起来。这一点以后一定要注意。

