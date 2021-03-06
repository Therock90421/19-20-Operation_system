/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this 
 * software and associated documentation files (the "Software"), to deal in the Software 
 * without restriction, including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit 
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE. 
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include "irq.h"
#include "test.h"
#include "stdio.h"
#include "sched.h"
#include "screen.h"
#include "common.h"
#include "syscall.h"

#define STACK_MAX 0xa0f00000
#define STACK_MIN 0xa0d00000
#define PCB_STACK_SIZE 0x10000    //process的大小
uint32_t stack_top = STACK_MAX;
uint32_t initial_cp0_status;
uint32_t exception_handler[32];
queue_t ready_queue, block_queue[NUM_MAX_TASK];

static void init_pcb()
{
	int i, cur_queue_id;

	queue_init(&ready_queue);
	for(i = 0; i < NUM_MAX_TASK; i++)
		queue_init(&block_queue[i]);   //初始化阻塞队列
	//queue_init(&sleep_queue);
	
	pcb[0].pid = process_id++;         //将队伍头初始化，以便第一次调用scheduler时使current_running指向第一个任务
	pcb[0].status = TASK_RUNNING;
	cur_queue_id = 1;
	
	//scheduler1 task1
	for(i = 0; i < num_sched1_tasks; i++, cur_queue_id++)  //把等待队列初始化
	{
		memset(&pcb[cur_queue_id].kernel_context,0, sizeof(pcb[cur_queue_id].kernel_context));
		memset(&pcb[cur_queue_id].user_context,0, sizeof(pcb[cur_queue_id].user_context));
		pcb[cur_queue_id].pid = process_id++;
		
		// stack 
		pcb[cur_queue_id].kernel_stack_top = stack_top;               //具体每个进程可使用的栈空间都是0x10000
		pcb[cur_queue_id].kernel_context.regs[29] = stack_top;
		stack_top -= PCB_STACK_SIZE;

		pcb[cur_queue_id].kernel_context.regs[31] = (uint32_t)&exception_handler_work_for_exit;
		pcb[cur_queue_id].kernel_context.cp0_status = 0x10008002;
			
		pcb[cur_queue_id].kernel_context.cp0_epc = sched1_tasks[i]->entry_point;
		
		pcb[cur_queue_id].user_stack_top = stack_top;
		pcb[cur_queue_id].user_context.regs[29] = stack_top;
		stack_top -= PCB_STACK_SIZE;
		
		pcb[cur_queue_id].user_context.regs[31] = sched1_tasks[i]->entry_point;
		pcb[cur_queue_id].user_context.cp0_status = 0x10008001;

		pcb[cur_queue_id].user_context.cp0_epc = sched1_tasks[i]->entry_point;
		pcb[cur_queue_id].type = sched1_tasks[i]->type;
		pcb[cur_queue_id].status = TASK_READY;

		//pcb[cur_queue_id].priority = 1;//(i+2)/2;
		queue_push(&ready_queue, (void *)&pcb[cur_queue_id]);
	}
	//lock1 task2
	for(i = 0; i < num_lock_tasks; i++, cur_queue_id++)
	{
		memset(&pcb[cur_queue_id].kernel_context,0, sizeof(pcb[cur_queue_id].kernel_context));
		memset(&pcb[cur_queue_id].user_context,0, sizeof(pcb[cur_queue_id].user_context));
		pcb[cur_queue_id].pid = process_id++;
		
		//stack 
		pcb[cur_queue_id].kernel_stack_top = stack_top;
		pcb[cur_queue_id].kernel_context.regs[29] = stack_top;
		stack_top -= PCB_STACK_SIZE;

		pcb[cur_queue_id].kernel_context.regs[31] = (uint32_t)&exception_handler_work_for_exit;
		pcb[cur_queue_id].kernel_context.cp0_status = 0x10008002;
			
		pcb[cur_queue_id].kernel_context.cp0_epc = lock_tasks[i]->entry_point;
		
		pcb[cur_queue_id].user_stack_top = stack_top;
		pcb[cur_queue_id].user_context.regs[29] = stack_top;
		stack_top -= PCB_STACK_SIZE;
        
		pcb[cur_queue_id].user_context.regs[31] = lock_tasks[i]->entry_point;
		pcb[cur_queue_id].user_context.cp0_status = 0x10008001;

		pcb[cur_queue_id].user_context.cp0_epc = lock_tasks[i]->entry_point;
		pcb[cur_queue_id].type = lock_tasks[i]->type;
		pcb[cur_queue_id].status = TASK_READY;

		
		queue_push(&ready_queue, (void *)&pcb[cur_queue_id]);
	} 
	current_running = &pcb[0];
}

static void init_exception_handler()   //32种例外处理函数
{
	int i;
	exception_handler[0] = (uint32_t)&handle_int;   //中断例外
	for(i = 1; i < 8; i++)
		exception_handler[i] = (uint32_t)&handle_other;
	exception_handler[8] = (uint32_t)&handle_syscall; //系统调用例外
	for(i = 9; i < 32; i++)
		exception_handler[i] = (uint32_t)&handle_other;

}

static void init_exception()
{
	init_exception_handler();
	// 1. Get CP0_STATUS
	initial_cp0_status = GET_CP0_STATUS();
	// 2. Disable all interrupt
	initial_cp0_status = initial_cp0_status | 0x10008001;    //先把必要的部分全部置1
	initial_cp0_status = initial_cp0_status ^ 0x1;           //第0位置0，关中断
	SET_CP0_STATUS(initial_cp0_status);              //只修改第0位
	initial_cp0_status |= 0x1;

	// 3. Copy the level 2 exception handling code to 0x80000180  //把二级例外的代码拷贝过来
	memcpy((void *)0x80000180, exception_handler_entry, exception_handler_end-exception_handler_begin);
	//BEV0_EBASE+BEV0_OFFSET   中断
	memcpy((void *)0xbfc00380, exception_handler_entry, exception_handler_end-exception_handler_begin);
	//BEV1_EBASE+BEV1_OFFSET)  其他
	// 4. reset CP0_COMPARE & CP0_COUNT register
	SET_CP0_COUNT(0);
	SET_CP0_COMPARE(150000);//TIME_INTERVAL

}

static void init_syscall(void)
{

}

// jump from bootloader.
// The beginning of everything >_< ~~~~~~~~~~~~~~
void __attribute__((section(".entry_function"))) _start(void)
{
	// Close the cache, no longer refresh the cache 
	// when making the exception vector entry copy
	asm_start();

	// init interrupt (^_^)
	init_exception();
	printk("> [INIT] Interrupt processing initialization succeeded.\n");

	// init system call table (0_0)
	init_syscall();
	printk("> [INIT] System call initialized successfully.\n");

	// init Process Control Block (-_-!)
	init_pcb();
	printk("> [INIT] PCB initialization succeeded.\n");

	// init screen (QAQ)
	init_screen();
	printk("> [INIT] SCREEN initialization succeeded.\n");

	// TODO Enable interrupt
	SET_CP0_STATUS(initial_cp0_status);
	while (1)
	{
		// (QAQQQQQQQQQQQ)
		// If you do non-preemptive scheduling, you need to use it to surrender control
		//do_scheduler();
	};
	return;
}
