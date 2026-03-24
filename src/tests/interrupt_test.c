#include <tests/__test.h>
#include <interrupts/idt.h>
#include <process/process.h>
#include <memory/vmm.h>
#include <memory/physical/pmm.h>
#include <string.h>

static volatile int soft_interrupt_fired = 0;

__attribute__((interrupt))
static void test_soft_interrupt_handler(struct cpu_interrupt_frame *frame)
{
    (void)frame;
    soft_interrupt_fired = 1;
}

void interrupt_tests(void)
{
    TEST_BEGIN("Interrupts");

    /* Register a custom CPU-managed soft interrupt handler at vector 50 */
    idt_set_gate(50, (uint64_t)test_soft_interrupt_handler, 0x08, IDT_GATE_INTERRUPT);

    /* Trigger the interrupt via software to ensure the CPU branches correctly */
    __asm__ volatile("int $50");

    TEST_ASSERT_EQ(soft_interrupt_fired, 1, "cpu managed handler works natively without switch-case");

    /* Test Division by Zero Exception in User Process */
    int status;
    int pid1 = process_create(0x40000000, "divzero_usr", PRIORITY_NORMAL, PROCESS_USER);
    TEST_ASSERT(pid1 > 0, "Creation of a div zero user process");

    /* Map a user page and inject div-by-zero instruction */
    process_t *p1 = process_get(pid1);
    uint64_t phys1 = pmm_alloc_frame();
    vmm_map_page(p1->addr_space, 0x40000000, phys1, VMM_PRESENT | VMM_WRITE | VMM_USER);
    
    uint8_t *code1 = (uint8_t *)phys_to_virt(phys1);
    code1[0] = 0x31; code1[1] = 0xc0; // xor eax, eax
    code1[2] = 0xf7; code1[3] = 0xf0; // div eax
    code1[4] = 0xeb; code1[5] = 0xfe; // jmp $

    int child1 = process_wait(&status);
    TEST_ASSERT_EQ(child1, pid1, "Waited for div zero process");
    TEST_ASSERT_EQ(status, 1, "Div zero process killed by exception handler with status 1");

    /* Test Page Fault Exception in User Process */
    int pid2 = process_create(0xDEADBEEF, "pf_usr", PRIORITY_NORMAL, PROCESS_USER);
    TEST_ASSERT(pid2 > 0, "Creation of page fault user process");

    int child2 = process_wait(&status);
    TEST_ASSERT_EQ(child2, pid2, "Waited for page fault process");
    TEST_ASSERT_EQ(status, 2, "Page fault process killed by exception handler with status 2");

    TEST_END("Interrupts");
}
