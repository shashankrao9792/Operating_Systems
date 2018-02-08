#include "sys/defs.h"
#include "sys/kprintf.h"
#include "sys/irq.h"
#include "sys/vmem.h"
#include "sys/tarfs.h"
#include "sys/string.h"
#include "sys/syscall.h"
#include "sys/process.h"
#include "sys/gdt.h"
#include "sys/timer.h"


volatile int changeScanValue=0;
extern uint64_t * cr3;

void syscall_handler(registers_t r)
{
	PCB* pcbe = NULL;
	uint64_t retVal;
	__asm__ __volatile__("movq %%rsp, %0":"=r"(globalAddress));
	switch(r.rax) {
		case SYS_READ: //read call
			syscall_read(r);
			break;
			
		case SYS_WRITE: //write call
			syscall_write(r);
			break;
			
		case SYS_OPEN: //open file
			syscall_open(r);
			break;
			
		case SYS_CLOSE:
			syscall_close(r);
			break;
			
		case SYS_MALLOC:
			syscall_malloc(r);
			break;
			
		case SYS_YIELD:
			syscall_yield();
			break;
			
		case SYS_SLEEP: //sleep
			syscall_sleep(r);
			break;
			
		case SYS_GETPID: //getpid
			syscall_getpid(r);
			break;
			
		case SYS_FORK:	//fork
			retVal = syscall_fork(r);
			__asm__ __volatile__("movq %0,%%rax"::"r"(retVal));
			break;
			
		case SYS_CAT:
			retVal = (uint64_t)syscall_cat(r);
			__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)retVal));
			break;
			
		case SYS_GETDENTS: //read directory
			syscall_getdents(r);
			break;
			
		case SYS_CWD:
			syscall_getcwd(r);
			break;
			
		case SYS_CHDIR:
			syscall_chdir(r);
			break;
			
		case SYS_CLRSCR:
			clear_screen();
			break;
			
		case SYS_KILL:
			syscall_kill(r);
			break;
			
		case SYS_EXECV:	//execv
			pcbe = execve(current_PCB, (char*)r.rbx, (char**)r.rcx, (char**)r.rdx);
			current_PCB = pcbe;
			switchToRing3(pcbe);
			break;
			
		case SYS_EXIT:
			sys_exit(r);
			
		case SYS_READLINE:
			syscall_readline(r);
			break;
			
		case SYS_PS:
			ps();
			break;
			
		case SYS_WAITID:
			syscall_waitpid(r);
			break;
	}
}

void syscall_readline(registers_t r) {
	uint64_t ret = read_file_per_line(r.rbx);
	strcpy((char*)r.rcx, (char*)ret);
	kfree(ret, (uint64_t*)(current_PCB->virtualPML4));
	__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)0));
}

void ps(){
	PCB* temp = current_PCB;
	char * state =(char*) malloc(20);
	if(temp->state==ZOMBIE)
	{
		strcpy(state,"ZOMBIE");
	}
	else if(temp->state==READY)
	{
		strcpy(state,"READY");
	}
	else if(temp->state==RUNNING)
	{
		strcpy(state,"RUNNING");
	}
	else if(temp->state==KILLED)
		{
			strcpy(state,"KILLED");
		}
	if(temp->state==ZOMBIE || temp->state==RUNNING || temp->state==READY)
		kprintf("\nPID : %d   Process Name %s   State: %s",temp->pid, temp->p_name, state);
	temp = temp->next;
	while(temp!=current_PCB){
		if(temp->state==ZOMBIE)
		{
			strcpy(state,"ZOMBIE");
		}
		else if(temp->state==READY)
		{
			strcpy(state,"READY");
		}
		else if(temp->state==RUNNING)
		{
			strcpy(state,"RUNNING");
		}
		else if(temp->state==KILLED)
				{
					strcpy(state,"KILLED");
				}
		if(temp->state==ZOMBIE || temp->state==RUNNING || temp->state==READY)
			kprintf("\nPID : %d   Process Name %s   State: %s",temp->pid, temp->p_name, state);
		temp = temp->next;
	}
	if(zombieList!=NULL){
		temp = zombieList;
		if(temp->state==ZOMBIE)
		{
			strcpy(state,"ZOMBIE");
		}
		else if(temp->state==READY)
		{
			strcpy(state,"READY");
		}
		else if(temp->state==RUNNING)
		{
			strcpy(state,"RUNNING");
		}
		else if(temp->state==KILLED)
				{
					strcpy(state,"KILLED");
				}
		if(temp->state==ZOMBIE || temp->state==RUNNING || temp->state==READY)
			kprintf("\nPID : %d   Process Name %s   State: %s",temp->pid, temp->p_name, state);
		temp = temp->next;
		while(temp!=NULL && temp!=zombieList){
			if(temp->state==ZOMBIE)
			{
				strcpy(state,"ZOMBIE");
			}
			else if(temp->state==READY)
			{
				strcpy(state,"READY");
			}
			else if(temp->state==RUNNING)
			{
				strcpy(state,"RUNNING");
			}
			else if(temp->state==KILLED)
			{
				strcpy(state,"KILLED");
			}
			if(temp->state==ZOMBIE || temp->state==RUNNING || temp->state==READY)
				kprintf("\nPID : %d   Process Name %s   State: %s",temp->pid, temp->p_name, state);
				temp = temp->next;
			}
	}
}


void syscall_yield()
{
	if(current_PCB->state != ZOMBIE  && current_PCB->state != KILLED){
		current_PCB->state = READY;
	}
	while(current_PCB->next->state == ZOMBIE || current_PCB->next->state == KILLED){
		PCB* temp = current_PCB->next;
		current_PCB->next = temp->next;
		addToZombieList(temp);
	}
	current_PCB->time_slice_left = current_PCB->time_slice;
	load_cr3(current_PCB->next->PML4);

	__asm__ __volatile__("movq %0,%%cr3"::"r"(current_PCB->next->PML4));

	set_tss_rsp((void*)(current_PCB->next->kernelStack));
	PCB* old = current_PCB;
	current_PCB = current_PCB->next;
	current_PCB -> state = RUNNING;

	__asm__ __volatile__("movq %%rsp, %0":"=b"(old ->kernelStackAddress));
	__asm__ __volatile__("movq %0, %%rsp" ::"b"(current_PCB->kernelStackAddress));
	__asm__ __volatile__ ("addq $8, %rsp");
	__asm__ __volatile__ ("movq $0x0, %rax");
	__asm__ __volatile__("retq");
}

void syscall_read(registers_t r) {
	if(r.rbx == 0) {
		scan(r);
	}
	else {
		uint64_t ret = read_file(r.rbx, r.rdx);
		strcpy((char*)r.rcx, (char*)ret);
		kfree(ret, (uint64_t*)(current_PCB->virtualPML4));
		__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)0));
	}
}

void syscall_write(registers_t r) {
	if(r.rbx == 1) {
		kprintf("%s", r.rcx);
		__asm__ __volatile("movq $0, %%rax"::"r"((uint64_t)0));
	}
	else {
		uint64_t ret = write_file(r.rbx, (char*)(r.rcx));
		__asm__ __volatile("movq %0, %%rax"::"r"(ret));
	}
}

void syscall_open(registers_t r) {
	uint64_t ret = open((char*) r.rbx);
	__asm__ __volatile("movq %0, %%rax"::"r"(ret));
}

void syscall_malloc(registers_t r) {
	uint64_t ret = malloc(r.rbx);
	__asm__ __volatile("movq %0, %%rax"::"r"(ret));
}

uint64_t syscall_fork(registers_t t){
	uint64_t ret = createChildProcess();
	load_cr3(current_PCB->PML4);
	return ret;
}

void scan(registers_t r){
	while(scanLock == 1){
		syscall_yield();
	}
		scanLock = 1;
		__asm__ __volatile("sti");
		changeScanValue = 0;
		while(changeScanValue != 1){}

		char* scannedVal = (char*)scanningAddress;
		char * variableAddress = (char*)(r.rcx);
		while(*(char*)scannedVal!='\0'){
			*variableAddress = *scannedVal;
			variableAddress+=1;
			scannedVal+=1;
		}
		*variableAddress = '\0';

		__asm__ __volatile("cli");
		scanLock = 0;
}

void syscall_getdents(registers_t r) {
	uint64_t ret = read_directory(r.rbx);
	strcpy((char*)(r.rcx), (char*)ret);
	kfree(ret, (uint64_t*)(current_PCB->virtualPML4));
	__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)0));
}

void syscall_getcwd(registers_t r) {
	uint64_t ret = get_current_working_dir();
	strcpy((char*)(r.rbx), (char*)ret);
	kfree(ret, (uint64_t*)(current_PCB->virtualPML4));
	__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)0));

}

void syscall_chdir(registers_t r) {
	uint64_t ret = change_directory((char*)(r.rbx));
	strcpy((char*)(r.rcx), (char*)ret);
	kfree(ret, (uint64_t*)(current_PCB->virtualPML4));
	__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)0));
}

uint64_t syscall_cat(registers_t r) {
	uint64_t ret = cat(r.rbx, (char*)r.rcx);
	return ret;
}

void syscall_close(registers_t r) {
	uint64_t ret = close(r.rbx);
	__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)ret));
}

void syscall_getpid(registers_t r) {
	uint64_t ret = getpid();
	__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)ret));
}

void syscall_sleep(registers_t r) {
	__asm__ __volatile("sti");
	sleep((int)(r.rbx), (char*)(r.rcx));
	__asm__ __volatile("cli");
	__asm__ __volatile("movq %0, %%rax"::"r"((uint64_t)0));
}

void syscall_waitpid(registers_t r){
	current_PCB->waitPid = r.rbx;
	while(current_PCB->waitPid != -1){
		syscall_yield();
	}
}
void updateParentOfkilledProcess(PCB* PCBToDelete)
{
	PCB* temp = PCBToDelete->next;
	while(temp->pid != PCBToDelete->ppid && temp != PCBToDelete){
		temp = temp->next;
	}
	if(temp->pid==PCBToDelete->ppid){
		temp->time_slice = temp->time_slice * 2;
		if(temp->waitPid == PCBToDelete->pid){
			temp->waitPid = -1;
		}
	}
}

void updateParent()
{
	PCB* temp = current_PCB->next;
	while(temp->pid != current_PCB->ppid && temp != current_PCB){
		temp = temp->next;
	if(temp->pid==current_PCB->ppid){
		temp->time_slice = temp->time_slice * 2;
		if(temp->waitPid == current_PCB->pid){
			current_PCB->state = KILLED;
			temp->waitPid = -1;
		}
	}
}
void sys_exit(registers_t r)
{
	current_PCB->state = ZOMBIE;
	updateParent();
	syscall_yield();
}

void syscall_kill(registers_t r){
	int pidTodelete = r.rbx;
	if(pidTodelete == 0){
		kprintf("\nCannot delete init");
	}
	else{
		PCB* temp = current_PCB->next;
		while(temp->pid != pidTodelete && temp!= current_PCB){
			temp = temp->next;
		}
		if(temp->pid == pidTodelete){
			updateParentOfkilledProcess(temp);
			temp->state = KILLED;
			kprintf("\n%s task with pid %d killed",temp->p_name, temp->pid);
		}
		else{
			kprintf("\nNo process with pid %d",pidTodelete);
		}
	}
}
