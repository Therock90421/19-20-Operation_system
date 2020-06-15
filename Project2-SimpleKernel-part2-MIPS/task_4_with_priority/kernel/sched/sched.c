#include "lock.h"
#include "time.h"
#include "stdio.h"
#include "sched.h"
#include "queue.h"
#include "screen.h"

pcb_t pcb[NUM_MAX_TASK];

/* current running task PCB */
pcb_t *current_running;

/* global process id */
pid_t process_id = 1;

static void check_sleeping()
{
	pcb_t *pcb_sleep_head = sleep_queue.head; //ȡ��˯�߶��е�ͷ
	uint32_t time = get_timer();
	if(pcb_sleep_head != NULL)   //һֱ��⵽��β
	{
		//�������ʱ���ѵ�
		if(time - pcb_sleep_head->begin_time >= pcb_sleep_head->sleep_time)
		{
			pcb_t *out_queue = (pcb_t *)queue_remove(&sleep_queue, (void *)pcb_sleep_head);
			/* remove this item and return next item */
			//ע��,��&sleep_queue����sleep_queue��
			pcb_sleep_head->sleep_time = 0;
			pcb_sleep_head->status = TASK_READY;
			
			priority_queue_push(&ready_queue, (void *)pcb_sleep_head);
			pcb_sleep_head = out_queue;
		}
		else
			pcb_sleep_head = pcb_sleep_head->next;
	}
	
}

void scheduler(void)
{
    // TODO schedule
    // Modify the current_running pointer.
	/*pcb_t *next_pcb;
	next_pcb = (pcb_t *)queue_dequeue(&ready_queue);  //��ջ
	
	if(current_running->status != TASK_BLOCKED)
	{
		current_running->status = TASK_READY;//������Ǳ��������ͻص�������
		if(current_running->pid != 1)      //��������pcb�ص�����
			queue_push(&ready_queue,current_running);
	}
	current_running = next_pcb;
	current_running->status = TASK_RUNNING;
	
	return ;*/
	check_sleeping();
	pcb_t *next_pcb, *p;
	
	
	
	if(queue_is_empty(&ready_queue))
        next_pcb = current_running;
    else
        next_pcb = (pcb_t *)queue_dequeue(&ready_queue);  //������Ϊ��ʱ���������е�ǰpcb
	
	if(current_running->status != TASK_BLOCKED && next_pcb != current_running)
	{
		current_running->status = TASK_READY;//������Ǳ��������ͻص�������
		if(current_running->pid != 1)      //��������pcb�ص�����
		{
			//queue_push(&ready_queue,current_running);
			priority_queue_push(&ready_queue, current_running);
		}
	}



	current_running = next_pcb;
	current_running->status = TASK_RUNNING;
	

	current_running->priority = current_running->task_priority;//���ȼ�����

	p = (pcb_t *)ready_queue.head;
    while(p != NULL)
    {
        p->priority += 1; //���ڵȴ�ʱ���������ȼ�
        p = p->next;
    }

	return ;


}

void do_sleep(uint32_t sleep_time)
{
    // TODO sleep(seconds)
	current_running->status = TASK_BLOCKED;
	current_running->begin_time = get_timer();
	current_running->sleep_time = sleep_time;
	queue_push(&sleep_queue,(void *)current_running);
	do_scheduler();
}

void do_block(queue_t *queue)
{
    // block the current_running task into the queue
	current_running->status = TASK_BLOCKED;
    //queue_push(queue, (void *)current_running);
	priority_queue_push(queue, (void *)current_running);
    do_scheduler();
	
}

void do_unblock_one(queue_t *queue)
{
    // unblock the head task from the queue
    
	pcb_t *p = (pcb_t *)(queue->head);
    while(p != NULL)
    {
        p->priority += 1;//���ڵȴ�ʱ���������ȼ�
        p = p->next;
    }

	pcb_t *block_head = (pcb_t *)queue_dequeue(queue);
    block_head->status = TASK_READY;
	priority_queue_push(&ready_queue, block_head);
    //queue_push(&ready_queue, block_head);     //���ﲻ��Ҫdo_schedule
	
}

void do_unblock_all(queue_t *queue)
{
    // unblock all task in the queue
	pcb_t *block_list;
    while(!queue_is_empty(queue))
    {
        block_list = (pcb_t *)queue_dequeue(queue);
        block_list->status = TASK_READY;
		priority_queue_push(&ready_queue, block_list);
       //queue_push(&ready_queue, block_list);
    }
	
}
