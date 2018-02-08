#include "sys/vmem.h"
#include "sys/pmem.h"
#include "sys/defs.h"
#include "sys/kprintf.h"
#include "sys/process.h"
#include "sys/irq.h"
#include "sys/outin.h"
#include "sys/tarfs.h"
#include "sys/syscall.h"

extern uint64_t* MMAPAdress;
extern char kernmem, physbase;
extern char* videoMemLocation;
int pageTableUpdated = 0;

extern uint64_t maxPhysicalAddress;
uint64_t * cr3;
uint64_t * virtual_PML4;


void memcpy(char* addrSrc, char* addrDest, int size)
{
	int i = 0;
	while(i<size){
		*addrDest = *addrSrc;
		i++;
		addrDest+=1;
		addrSrc+=1;
	}
}
uint64_t physicalToVirtual(uint64_t pAddress,uint64_t vAddress, uint64_t * PML4)
{
	uint64_t* PDTE;
	uint64_t* PDT;
	uint64_t* PT;
	int PML4Offset = getPLM4Offset((uint64_t)vAddress);
	int PDTEOffset = getPDTEOffset((uint64_t)vAddress);
	int PDTOffset = getPDTOffset((uint64_t)vAddress);
	int PTOffset = getPageTableOffset((uint64_t)vAddress);
	uint64_t PDTEVal = PML4[PML4Offset];
	if((PDTEVal & 0x1)>0){
		PDTE =  (uint64_t*)((uint64_t)PDTEVal & ~(0xFFF));
	}
	else{
		PDTE = allocatePage();
		PML4[PML4Offset] = (uint64_t)((uint64_t)PDTE | 0x3ull);
		physicalToVirtual((uint64_t)PDTE,(uint64_t)PDTE, PML4);
	}
	uint64_t PDTVal = PDTE[PDTEOffset];
	if((PDTVal & 0x1)>0){
		PDT =  (uint64_t*)((uint64_t)PDTVal & ~(0xFFF));
	}
	else{
		PDT = allocatePage();
		PDTE[PDTEOffset] = (uint64_t)((uint64_t)PDT | 0x3ull);
		physicalToVirtual((uint64_t)PDT,(uint64_t)PDT, PML4);
	}
	uint64_t PTVal = PDT[PDTOffset];
	if((PTVal & 0x1)>0){
		PT =  (uint64_t*)((uint64_t)PTVal & ~(0xFFF));
	}
	else{
		PT = allocatePage();
		PDT[PDTOffset] = (uint64_t)((uint64_t)PT | 0x3ull);
		physicalToVirtual((uint64_t)PT,(uint64_t)PT, PML4);
	}
	PT[PTOffset] = (uint64_t)(pAddress | 0x3ull);
	return vAddress;
}


uint64_t virtualToPhysical(uint64_t vAddress, uint64_t * PML4)
{
	int PML4Offset = getPLM4Offset((uint64_t)vAddress);
	int PDTEOffset = getPDTEOffset((uint64_t)vAddress);
	int PDTOffset = getPDTOffset((uint64_t)vAddress);
	int PTOffset = getPageTableOffset((uint64_t)vAddress);
/*---------------------------------------------------------------------------------------*/
	uint64_t PDTE_val = (uint64_t)PML4[PML4Offset];
	uint64_t* PDTE;
	if((PDTE_val & PG_P)==0){
		return 0;
	}
	else{
		PDTE = (uint64_t*)((PDTE_val& ~(0xFFF)) | 0xffffffff80000000);
	}
/*---------------------------------------------------------------------------------------*/
	uint64_t* PDT ;
	uint64_t PDT_val = (uint64_t)PDTE[PDTEOffset] ;
	if((PDT_val & PG_P)==0){
		return 0;
	}
	else{
		PDT = (uint64_t*)((PDT_val& ~(0xFFF)) | 0xffffffff80000000);
	}
/*---------------------------------------------------------------------------------------*/
	uint64_t* PT;
	uint64_t PT_val = (uint64_t)PDT[PDTOffset];
	if((PT_val & PG_P)==0){
		return 0;
	}
	else{
		PT = (uint64_t*)((PT_val& ~(0xFFF)) | 0xffffffff80000000);
	}
/*---------------------------------------------------------------------------------------*/
	uint64_t* address = (uint64_t*)((uint64_t)PT[PTOffset]);
	if(((uint64_t)address  & PG_P)==0){
		return 0;
	}
	else{
		return (uint64_t)address;
	}
}

void load_cr3(uint64_t PML4)
{
	currentPML4 = PML4 | 0xffffffff80000000ull;
	__asm volatile("movq %0, %%cr3":: "b"(PML4));
}

void identityPagingInKernel(uint64_t bottom, uint64_t top)
{
	uint64_t * PML4 = cr3;
	while(bottom<0x60DA000){
		physicalToVirtual(bottom,(uint64_t) bottom | 0xffffffff80000000ull ,PML4);
		bottom+=0x1000ul;
	}
}

void createAndInsertVMA(PCB* pcb, uint64_t startAddress, uint64_t endAddress,int flags, int type ){
	VMA* newVMA = (VMA*)kmalloc();
	newVMA->next = NULL;
	newVMA->start = startAddress;
	newVMA->end = endAddress;
	newVMA->flags = flags;

	newVMA->type = type;
	if(type == TEXT){
//		kprintf("TEXT WAS LOADED\n");
	}
	else{
	if(type == DATA){
//			kprintf("DATA WAS LOADED\n");
		}
	}
	mm_struct * mem  = pcb->mem;
	if(mem->firstVMA == NULL){
		mem->firstVMA = newVMA;
		if(mem){}
	}
	else{
		VMA* current = mem->firstVMA;
		while(current->next != NULL){
			current = current->next;
		}
		current->next = newVMA;
	}
	return;
}



void walkVMA(PCB* pcb){
	mm_struct * mem  = pcb->mem;
	if(mem->firstVMA == NULL){
		return;
	}
	VMA* current = mem->firstVMA;
	while(current!=NULL){
		current = current->next;
	}
	return;
}

void copyVMAStructure(PCB* child, PCB* parent){
	mm_struct * parentMem  = parent->mem;
	mm_struct * childMem  = child->mem;

	VMA* currentParentVMA = parentMem->firstVMA;

	VMA* newVMA = (VMA*)kmalloc();
	newVMA->start = currentParentVMA->start;
	newVMA->end = currentParentVMA->end;
	newVMA->flags = currentParentVMA->flags;
	newVMA->type = currentParentVMA->type;
	childMem->firstVMA = newVMA;
	VMA* last = newVMA;
	currentParentVMA = currentParentVMA->next;

	while(currentParentVMA!=NULL)
	{
		VMA* newVMA = (VMA*)kmalloc();
		newVMA->next = NULL;
		last->next = newVMA;
		last = newVMA;
		newVMA->start = currentParentVMA->start;
		newVMA->end = currentParentVMA->end;
		newVMA->flags = currentParentVMA->flags;
		newVMA->type = currentParentVMA->type;
		currentParentVMA = currentParentVMA->next;
	}


}

int getPLM4Offset(uint64_t p){
	return ((p>>39) & 0x1FF);
}


int getPDTEOffset(uint64_t p){
	return ((p>>30) & 0x1FF);
}


int getPDTOffset(uint64_t p){
	return ((p>>21) & 0x1FF);
}


int getPageTableOffset(uint64_t p){
	return ((p>>12) & 0x1FF);
}


void deletePageTables(PCB* pcb)
{
	uint64_t* pml4 = (uint64_t*)(pcb->virtualPML4);
	int pml4Index = 0;
	for(pml4Index = 0; pml4Index <=	 510;pml4Index++){
		if((pml4[pml4Index] & PG_P)>0){
			uint64_t* pdte = (uint64_t*)((pml4[pml4Index] & ~0xFFF) | 0xffffffff80000000);
			int pdteIndex = 0;
			for(pdteIndex=0; pdteIndex<=511; pdteIndex++){
				if((pdte[pdteIndex] &PG_P)>0)
				{
					uint64_t* pdt = (uint64_t*)((pdte[pdteIndex] & ~0xFFF) | 0xffffffff80000000);
					int pdtIndex = 0;
					for(pdtIndex=0; pdtIndex<=511; pdtIndex++){
						if((pdt[pdtIndex] &PG_P)>0)
						{
							uint64_t* pt = (uint64_t*)((pdt[pdtIndex] & ~0xFFF) | 0xffffffff80000000);
							int ptIndex = 0;
							for(ptIndex=0;ptIndex<=511;ptIndex++)
							{
								if((pt[ptIndex] & PG_P)>0)
								{
									uint64_t value = pt[ptIndex];
									if((value & PG_R)>0)
									{
//										kprintf("page :%p\t",value & (0xffff000));
										value = value & (0xfffffff);
										mmapSetBit(value/0x1000);
									}
								}
							}
							uint64_t value = (uint64_t)pt & 0xfffffff;
							if((value & PG_R)>0)
							{
								mmapSetBit(value/0x1000);
//								kprintf("%p\t",value & (0xfffffff));
							}
						}
					}
					uint64_t value = (uint64_t)pdt & 0xfffffff;
					if((value & PG_R)>0)
					{
						mmapSetBit(value/0x1000);
//						kprintf("%p\t",value & (0xfffffff));
					}

				}
			}
			uint64_t value = (uint64_t)pdte & 0xfffffff;
			if((value & PG_R)>0)
			{
				mmapSetBit(value/0x1000);
//				kprintf("%p\t",value & (0xfffffff));
			}
		}
	}
	uint64_t value = (uint64_t)pml4 & 0xfffffff;
	if((value & PG_R)>0)
	{
		mmapSetBit(value/0x1000);
//		kprintf("%p\t",value & (0xfffffff));
	}
}

uint64_t kmalloc()
{
		uint64_t page = (uint64_t)allocatePage2();
		page = page | 0xffffffff80000000ull;
		updatePageEntriesToZero((uint64_t)page);
		return page ;
}


void userPhysicalToVirtual(uint64_t pAddress,uint64_t vAddress, uint64_t *PML4)
{

	uint64_t* PDTE;
//	uint64_t* PDTE
	uint64_t* PDT;
	uint64_t* PT;
	int PML4Offset = getPLM4Offset((uint64_t)vAddress);
	int PDTEOffset = getPDTEOffset((uint64_t)vAddress);
	int PDTOffset = getPDTOffset((uint64_t)vAddress);
	int PTOffset = getPageTableOffset((uint64_t)vAddress);

	uint64_t PDTEVal = PML4[PML4Offset]  | 0xffffffff80000000;
	if((PDTEVal & 0x1)>0)
	{
		PDTE =  (uint64_t*)((uint64_t)PDTEVal & ~(0xFFF));
	}
	else{
		PDTE = (uint64_t*)kmalloc();
		PML4[PML4Offset] = ((uint64_t)PDTE & 0xfffffff) | 0x7ull;
	}

	uint64_t PDTVal = PDTE[PDTEOffset] | 0xffffffff80000000;
	if((PDTVal & 0x1)>0){
		PDT =  (uint64_t*)((uint64_t)PDTVal & ~(0xFFF));
	}
	else{
		PDT = (uint64_t*)kmalloc();
		PDTE[PDTEOffset] = ((uint64_t)PDT & 0xfffffff) | 0x7ull;
	}

	uint64_t PTVal = PDT[PDTOffset] | 0xffffffff80000000;
	if((PTVal & 0x1)>0){
		PT =  (uint64_t*)((uint64_t)PTVal & ~(0xFFF));
	}
	else{
		PT = (uint64_t*)kmalloc();
		PDT[PDTOffset] = ((uint64_t)PT & 0xfffffff) | 0x7ull;
	}
	PT[PTOffset] = (uint64_t)(pAddress | 0x7ull);
	//kprintf("\n\n%p\n",PT[510]);
}



void pageWalk(uint64_t* pml4)
{
	int count = 0;
	int pml4Index = 0;
	for(pml4Index = 0; pml4Index <= 510;pml4Index++){
		if((pml4[pml4Index] & PG_P)>0){
			uint64_t* pdte = (uint64_t*)((pml4[pml4Index] & ~0xFFF) | 0xffffffff80000000);
			int pdteIndex = 0;
			for(pdteIndex=0; pdteIndex<=511; pdteIndex++){
				if((pdte[pdteIndex] &PG_P)>0)
				{
					uint64_t* pdt = (uint64_t*)((pdte[pdteIndex] & ~0xFFF) | 0xffffffff80000000);
					int pdtIndex = 0;
					for(pdtIndex=0; pdtIndex<=511; pdtIndex++){
						if((pdt[pdtIndex] &PG_P)>0)
						{
							uint64_t* pt = (uint64_t*)((pdt[pdtIndex] & ~0xFFF) | 0xffffffff80000000);
							int ptIndex = 0;
							for(ptIndex=0;ptIndex<=511;ptIndex++)
							{
								if((pt[ptIndex] &PG_P)>0)
								{
									count++;
								}
							}
						}
					}
				}
			}
		}
	}
}




void pageFault_handler_code(registers_t r){
	volatile uint64_t address;
		__asm__ __volatile__(
					"movq %%cr2, %0;"\
					:"=r"(address)
					:
					);

	PCB* pcb = current_PCB;
	mm_struct * mem = pcb->mem;
	VMA* currentVMA = mem->firstVMA;
	while(currentVMA!=NULL && !(address >= currentVMA->start && address<currentVMA->end))
	{
		currentVMA = currentVMA->next;
	}
	if(currentVMA != NULL){
		uint64_t pageAlignedAddress = address & ~(0xFFF);
		uint64_t pt_entry = virtualToPhysical(pageAlignedAddress,(uint64_t*)(pcb->virtualPML4));
		uint64_t currentCR3;
		__asm__ __volatile__("movq %%cr3, %0":"=b"(currentCR3):);
		uint64_t virtualPML4 = currentCR3 | 0xffffffff80000000ull;
		if((pt_entry & PG_P) == 0x0)
		{
			uint64_t page = kmalloc();
			userPhysicalToVirtual((uint64_t)page & 0xfffffff,pageAlignedAddress, (uint64_t*)virtualPML4);
		}
		else
		{
			if((pt_entry & PG_R) == 0 )
			{
				if((pt_entry & PG_COW) > 0 )
				{
					if(currentVMA->type == STACK || currentVMA->type == HEAP || currentVMA->type == DATA)
					{
						uint64_t newPage = kmalloc();
						uint64_t newPagePhysAddress = newPage & 0xfffffff;
						memcpy((char*)pageAlignedAddress, (char*)newPage,0x1000);
						userPhysicalToVirtual(newPagePhysAddress,pageAlignedAddress,(uint64_t*)virtualPML4);
						__asm__ __volatile__ ("movq %0, %%cr3" ::"r"(virtualPML4&0xfffffff));
					}
					else{
						kprintf("\nTrying to access data section!!!!!!!!!!");
						sys_exit(r);
					}
				}
				else{
					kprintf("\nSEGMENTATION FAULT!!");
					sys_exit(r);
				}
			}
		}

	}
	else{
		kprintf("\nSEGMENTATION FAULT ON ACCESSING ADDRESSS %p\n",address);
		sys_exit(r);
	}
	outb(0x20, 0x20);
}


void identityPaging(uint64_t bottom, uint64_t top)
{
	uint64_t * PML4 = cr3;
	while(bottom<top){
		userPhysicalToVirtual2(bottom,bottom| 0xffffffff80000000 ,PML4);
		bottom+=0x1000ul;
	}
}


void init_paging(uint64_t physbase, uint64_t physfree, uint64_t max_start, uint64_t max_end){
	uint64_t * PML4 =virtualizeKernMem(physbase,physfree);
	userPhysicalToVirtual2((uint64_t)0xb8000,(uint64_t)0xb8000 | 0xffffffff80000000, PML4);
	virtualToPhysical((uint64_t)0xb8000 | 0xffffffff80000000, PML4);
	identityPaging(0,max_end);
	userPhysicalToVirtual((uint64_t)MMAPAdress,(uint64_t)MMAPAdress | 0xffffffff80000000, PML4);
	MMAPAdress = (uint64_t*)((uint64_t)MMAPAdress | 0xffffffff80000000);
	load_cr3((uint64_t)PML4);
	virtual_PML4 = (uint64_t*)((uint64_t)PML4 | 0xffffffff80000000ull);
	if(PML4){}
}



uint64_t * virtualizeKernMem(uint64_t physfree,uint64_t physbase)
{

	uint64_t * PML4 = allocatePage();
	cr3 = PML4;
	uint64_t * PDTE = allocatePage();
	uint64_t * PDT  = allocatePage();
	uint64_t * PT   = allocatePage();
	int PML4Offset = getPLM4Offset((uint64_t)&kernmem);
	int PDTEOffset = getPDTEOffset((uint64_t)&kernmem);
	int PDTOffset = getPDTOffset((uint64_t)&kernmem);
	int PTOffset = getPageTableOffset((uint64_t)&kernmem);
	PML4[PML4Offset] = (uint64_t)PDTE | 0x3ull;
	PDTE[PDTEOffset] = (uint64_t)PDT | 0x3ull;
	PDT[PDTOffset] = (uint64_t)PT | 0x3ull;

	uint64_t kernelStart = (uint64_t)physbase;
	uint64_t kernelEnd = (uint64_t)physfree;
	while(kernelStart <kernelEnd){

		PT[PTOffset] = ((uint64_t)kernelStart & 0xfffffff)| 0x3ull;
		kernelStart+=0x1000;
		PTOffset+=1;
	}

	userPhysicalToVirtual((uint64_t)PML4,(uint64_t)PML4 | 0xffffffff80000000ull,PML4);
	userPhysicalToVirtual((uint64_t)PDTE,(uint64_t)PDTE | 0xffffffff80000000ull,PML4);
	userPhysicalToVirtual((uint64_t)PDT,(uint64_t)PDT | 0xffffffff80000000ull,PML4);
	userPhysicalToVirtual((uint64_t)PT,(uint64_t)PT | 0xffffffff80000000ull,PML4);
	return PML4;
}




void userPhysicalToVirtual2(uint64_t pAddress,uint64_t vAddress, uint64_t *PML4)
{
	uint64_t* PDTE;
	uint64_t* PDT;
	uint64_t* PT;
	int PML4Offset = getPLM4Offset((uint64_t)vAddress);
	int PDTEOffset = getPDTEOffset((uint64_t)vAddress);
	int PDTOffset = getPDTOffset((uint64_t)vAddress);
	int PTOffset = getPageTableOffset((uint64_t)vAddress);

	uint64_t PDTEVal = PML4[PML4Offset];
	if((PDTEVal & 0x1)>0)
	{
		PDTE =  (uint64_t*)((uint64_t)PDTEVal & ~(0xFFF));
	}
	else{
		PDTE = (uint64_t*)allocatePage();
		PML4[PML4Offset] = ((uint64_t)PDTE & 0xfffffff) | 0x3ull;

	}
	uint64_t PDTVal = PDTE[PDTEOffset];
	if((PDTVal & 0x1)>0){
		PDT =  (uint64_t*)((uint64_t)PDTVal & ~(0xFFF));
	}
	else{
		PDT = (uint64_t*)allocatePage();
		PDTE[PDTEOffset] = ((uint64_t)PDT & 0xfffffff) | 0x3ull;
	}
	uint64_t PTVal = PDT[PDTOffset];
	if((PTVal & 0x1)>0){
		PT =  (uint64_t*)((uint64_t)PTVal & ~(0xFFF));
	}
	else{
		PT = (uint64_t*)allocatePage();
		PDT[PDTOffset] = ((uint64_t)PT & 0xfffffff) | 0x3ull;
	}
	PT[PTOffset] = (uint64_t)(pAddress | 0x3ull);
}



uint64_t gettingVirtualAddressFromOffsets(uint64_t PML4Index,uint64_t pdteIndex, uint64_t pdtIndex, uint64_t ptIndex){
	uint64_t virtualAddress = (((((((PML4Index<<9)|pdteIndex)<<9)|pdtIndex)<<9)|ptIndex)<<12);
	if(PML4Index > 255){
		virtualAddress |= 0xFFFF000000000000;
	}
	return virtualAddress;
}


void copyParentPageTablesToChild(uint64_t* parentPML4, uint64_t* childPML4)
{
	int pml4Index = 0;
	for(pml4Index =0;pml4Index < 510; pml4Index++)
	{
		if((parentPML4[pml4Index] & PG_P )>0){
			uint64_t* childPDTE = (uint64_t*)kmalloc();
			childPML4[pml4Index] = ((uint64_t)childPDTE & 0xfffffff)| 0x7;
			uint64_t* parentpdte = (uint64_t*)(((uint64_t)parentPML4[pml4Index] & ~(0xfff)) | 0xffffffff80000000ull);
			if(parentpdte){}
			int pdteIndex = 0;
			for(pdteIndex = 0; pdteIndex <= 511; pdteIndex++){
				if((parentpdte[pdteIndex] & PG_P )>0){
					uint64_t* childPDT = (uint64_t*)kmalloc();
					childPDTE[pdteIndex] = ((uint64_t)childPDT & 0xfffffff) | 0x7;
					uint64_t* parentpdt = (uint64_t*)(((uint64_t)parentpdte[pdteIndex] & ~(0xfff)) | 0xffffffff80000000ull);
					int pdtIndex = 0;
					for(pdtIndex = 0; pdtIndex <= 511; pdtIndex++){
						if((parentpdt[pdtIndex] & PG_P )>0){
							uint64_t* childPT = (uint64_t*)kmalloc();
							childPDT[pdtIndex] = ((uint64_t)childPT &0xfffffff) | 0x7;
							uint64_t* parentpt = (uint64_t*)(((uint64_t)parentpdt[pdtIndex] & ~(0xfff)) | 0xffffffff80000000ull);
							int ptIndex = 0;
							for(ptIndex = 0; ptIndex <= 511; ptIndex++){
								if((parentpt[ptIndex] & PG_P )>0){
									parentpt[ptIndex] = parentpt[ptIndex] & ~(0xfff);
									parentpt[ptIndex] = parentpt[ptIndex] | 0x5ull | PG_COW ;
									childPT[ptIndex] = parentpt[ptIndex] ;
								}
							}
						}
					}
				}
			}
		}
	}
}
