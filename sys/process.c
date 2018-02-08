#include "sys/process.h"
#include "sys/kprintf.h"
#include "sys/vmem.h"
#include <sys/gdt.h>
#include "sys/pmem.h"
#include "sys/tarfs.h"
#include "sys/string.h"
#include "sys/syscall.h"

THREAD* thread1;
THREAD* thread2;
THREAD* thread3;
extern uint64_t* PML4;
extern uint64_t* cr3;
PCB* createProcess();

extern uint64_t * virtual_PML4;

uint64_t getNewPid(){
	uint64_t retPid = pid_pp;
	pid_pp++;
	return retPid;
}

uint64_t getCurrentProcPid()
{
	return current_PCB->pid;
}
uint64_t getParentPid()
{
	return current_PCB->ppid;
}


void context_switch_old(THREAD* thready1, THREAD* thready2)
{
	__asm__ __volatile__("movq %%rsp, %0;"
				:"=r"(thready1->rsp)
				:
				);
	
	__asm__ __volatile__("movq %0, %%rsp;"
				:"=r"(thready2->rsp)
				:
				);
}

void context_switch(THREAD* old, THREAD* new)
{
	 __asm__ __volatile__ ("pushq %%rdi"::);
	 __asm__ __volatile__ ("pushq %%rsi"::);
	 __asm__ __volatile__ ("pushq %%rax"::);
	 __asm__ __volatile__ ("pushq %%rbx"::);
	 __asm__ __volatile__ ("pushq %%rbp"::);
	 __asm__ __volatile__ ("movq %%rsp, %0":"=r"(old->rsp));

	 __asm__ __volatile__("movq %0, %%rsp;"\
				  "popq %%rbp;"\
				  "popq %%rbx;"\
				  "popq %%rax;"\
				  "popq %%rsi;"\
				  "popq %%rdi;"
				  ::"r"(new->rsp)
					);
}

void context_switch2(PCB* old, PCB* new)
{
	 __asm__ __volatile__("movq %%rsp, %0":"=r"(old->kernelStackAddress));

	 __asm__ __volatile__("movq %0, %%rsp" ::"r"(new->kernelStackAddress));
	 __asm__ __volatile__("retq");
}
void switchToRing3(PCB* pcb)
{
	load_cr3(pcb->PML4);

	virtual_PML4 = (uint64_t*)(pcb->virtualPML4);

	set_tss_rsp((void*)(pcb->kernelStack));
	//updating all the register values to 0x23
	__asm__ volatile("mov $0x23, %%cx;"\
			 "mov %%cx, %%ds;"\
			 "mov %%cx, %%es;"\
			 "mov %%cx, %%fs;"\
			 "mov %%cx, %%gs;"
			 ::
			);

	__asm__ __volatile__("pushq $0x23;"\
			     "pushq %0;"\
			     "pushfq;"\
				 "popq %%rbx;"\
				 "orq $0x200, %%rbx;"\
				 "pushq %%rbx;"\
			     "pushq $0x2B;"\
			     "pushq %1"
			     :
			     :"a"(pcb->rsp),"c"(pcb->rip)
			    );
	__asm__ __volatile__("iretq");
}

void function_1(){
	__asm__ __volatile__("int $0x80");
	while(1){}
}

void function_2(){
	kprintf("In 2\n");
//	while(1){}
	context_switch(thread2,thread1);
	kprintf("back in 2\n");
	context_switch(thread2,thread1);
	kprintf("Executing function2\n");

	while(1){}
}
void function_3(){
	kprintf("In 3\n");
	//kprintf("Executing function2\n");
	context_switch(thread2,thread1);
	while(1);
	kprintf("back in 2.1\n");

	//kprintf("back in 2.1\n");
	//kprintf("5");
	//context_switch(thread2,thread1);
	while(1){}
}




PCB* createProcess()
{
	//creating PCB block
	PCB* pcb = (PCB*)kmalloc();


	pcb->next = pcb;
	pcb->pid = getNewPid();
	//creating kernelStack to be used for context switching
	pcb->kernelStack = (uint64_t*)((uint64_t)kmalloc() +0x1000 - 0x8);

	//	Creating PML4 table for PCB
	uint64_t page = (uint64_t)kmalloc();
	pcb->PML4 = ((uint64_t)page) & 0xffffff;		//saving physical address of PML4
	pcb->virtualPML4 = page;				//saving virtual address of PML4
	updatePageEntriesToZero((uint64_t)(pcb->virtualPML4 )); //setting all values of page table to zero
	((uint64_t*)page)[510] = (pcb->PML4) | 0x7ull; // for self-refrencing
	((uint64_t*)page)[511]= virtual_PML4[511];

	//setting up the mm_struct
	mm_struct* mem = (mm_struct*) kmalloc();
	mem->firstVMA = NULL;
	pcb->mem  = mem;

	//	allocate file structure
	int i = 0;
	while(strlen(fd[i].name) != 0) {
		pcb->pcb_fd[i].inode_no = 0x9999ull;
		pcb->pcb_fd[i].read_offset = 0x0ull;
		pcb->pcb_fd[i].open_flag = 0;
		i++;
	}
	
	pcb->curr_inode_no = fd[3].inode_no;
	return pcb;
}

uint64_t malloc(uint64_t size) {
	uint64_t ret_addr = (uint64_t)current_PCB->heap_ptr;
	current_PCB->heap_ptr = (uint64_t*)((uint64_t)(current_PCB->heap_ptr) + size);
	return ret_addr;
}

uint64_t getpid() {
	uint64_t ret_val = current_PCB->pid;
	return ret_val;
}

void addToZombieList(PCB *temp){
	deletePageTables(temp);
	if(zombieList == NULL){
		zombieList = temp;
		temp->next=NULL;
	}
	else{
		PCB* current = zombieList;
		while(current->next != NULL){
			current = current->next;
		}
		current->next = temp;
		temp->next=NULL;
	}

}


THREAD* createThread()
{
	THREAD* thread = (THREAD*)kmalloc();
	thread->stack = (uint64_t*)kmalloc();
	thread->stack = (uint64_t*)((uint64_t)thread->stack+ (0x1000 - 0x10));
	thread->rsp = (uint64_t)(thread->stack);
	return thread;
}


void copyFileDescriptors(PCB *child){

	child->curr_inode_no = current_PCB->curr_inode_no;
	int i = 0;
	while(strlen(fd[i].name) != 0) {
			child->pcb_fd[i].inode_no = current_PCB->pcb_fd[i].inode_no;
			child->pcb_fd[i].read_offset = current_PCB->pcb_fd[i].read_offset;
			child->pcb_fd[i].open_flag = current_PCB->pcb_fd[i].open_flag ;
			i++;
		}
}


uint64_t createChildProcess()
{
//	create a child structure
	PCB* child = (PCB*)kmalloc();
	
//	create its page tables
	child->virtualPML4 = kmalloc();
	child->PML4 = (child->virtualPML4) & 0xfffffff;
	((uint64_t*)(child->virtualPML4))[510] = child->PML4;
	((uint64_t*)(child->virtualPML4))[511] = ((uint64_t*)currentPML4)[511];

//	copy page tables
	copyParentPageTablesToChild((uint64_t*)(current_PCB->virtualPML4), (uint64_t*)(child->virtualPML4));

//	copy VMA structure
	mm_struct* mem = (mm_struct*) kmalloc();
	mem->firstVMA = NULL;
	child->mem  = mem;
	copyVMAStructure(child, current_PCB);


//	create child kernel Stack
	child->kernelStack = (uint64_t*)((uint64_t)kmalloc() +0x1000 - 0x8);
	__asm__ __volatile__ ("movq %0, %%cr3"::"r"((child->virtualPML4)&0xfffffff));
	memcpy((char*)((uint64_t)(current_PCB->kernelStack)-0x1000),
		   (char*)((uint64_t)(child->kernelStack)-0x1000),
		   0x1000 );

	child->pid = getNewPid();
	child->ppid = current_PCB->pid;
	PCB* temp = current_PCB->next;
	current_PCB->next = child;
	child->next  = temp;

	copyFileDescriptors(child);

	child->kernelStackAddress = (((uint64_t)(child->kernelStack) & ~(0xfff)) | (globalAddress & 0xfff));
	current_PCB->time_slice = (current_PCB->time_slice)/2;
	child->time_slice = current_PCB->time_slice;
	child->time_slice_left = child->time_slice;
	__asm__ __volatile__("movq %0, %%cr3"::"r"((current_PCB->virtualPML4)&0xfffffff));

	return child->pid;
}
