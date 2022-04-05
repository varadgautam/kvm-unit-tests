
#include <libcflat.h>
#include "processor.h"
#include "atomic.h"
#include "smp.h"
#include "apic.h"
#include "fwcfg.h"
#include "desc.h"

#define IPI_VECTOR 0x20

typedef void (*ipi_function_type)(void *data);

static struct spinlock ipi_lock;
static volatile ipi_function_type ipi_function;
static void *volatile ipi_data;
static volatile int ipi_done;
static volatile bool ipi_wait;
static int _cpu_count;
static atomic_t active_cpus;
extern u8 sipi_entry;
extern u8 sipi_end;
volatile unsigned cpu_online_count = 1;

static __attribute__((used)) void ipi(void)
{
    void (*function)(void *data) = ipi_function;
    void *data = ipi_data;
    bool wait = ipi_wait;

    if (!wait) {
	ipi_done = 1;
	apic_write(APIC_EOI, 0);
    }
    function(data);
    atomic_dec(&active_cpus);
    if (wait) {
	ipi_done = 1;
	apic_write(APIC_EOI, 0);
    }
}

asm (
     "ipi_entry: \n"
     "   call ipi \n"
#ifndef __x86_64__
     "   iret"
#else
     "   iretq"
#endif
     );

int cpu_count(void)
{
    return _cpu_count;
}

int smp_id(void)
{
    unsigned id;

    asm ("mov %%gs:0, %0" : "=r"(id));
    return id;
}

static void setup_smp_id(void *data)
{
    asm ("mov %0, %%gs:0" : : "r"(apic_id()) : "memory");
}

static void __on_cpu(int cpu, void (*function)(void *data), void *data,
                     int wait)
{
    unsigned int target = id_map[cpu];

    spin_lock(&ipi_lock);
    if (target == smp_id())
	function(data);
    else {
	atomic_inc(&active_cpus);
	ipi_done = 0;
	ipi_function = function;
	ipi_data = data;
	ipi_wait = wait;
	apic_icr_write(APIC_INT_ASSERT | APIC_DEST_PHYSICAL | APIC_DM_FIXED
                       | IPI_VECTOR, target);
	while (!ipi_done)
	    ;
    }
    spin_unlock(&ipi_lock);
}

void on_cpu(int cpu, void (*function)(void *data), void *data)
{
    __on_cpu(cpu, function, data, 1);
}

void on_cpu_async(int cpu, void (*function)(void *data), void *data)
{
    __on_cpu(cpu, function, data, 0);
}

void on_cpus(void (*function)(void *data), void *data)
{
    int cpu;

    for (cpu = cpu_count() - 1; cpu >= 0; --cpu)
        on_cpu_async(cpu, function, data);

    while (cpus_active() > 1)
        pause();
}

int cpus_active(void)
{
    return atomic_read(&active_cpus);
}

void smp_init(void)
{
    int i;
    void ipi_entry(void);

    setup_idt();
    init_apic_map();
    set_idt_entry(IPI_VECTOR, ipi_entry, 0);

    setup_smp_id(0);
    for (i = 1; i < cpu_count(); ++i)
        on_cpu(i, setup_smp_id, 0);

    atomic_inc(&active_cpus);
}

static void do_reset_apic(void *data)
{
    reset_apic();
}

void smp_reset_apic(void)
{
    int i;

    reset_apic();
    for (i = 1; i < cpu_count(); ++i)
        on_cpu(i, do_reset_apic, 0);

    atomic_inc(&active_cpus);
}

void ap_init(void)
{
    u8 *dst_addr = 0;
    size_t sipi_sz = (&sipi_end - &sipi_entry) + 1;

    asm volatile("cld");

    /* Relocate SIPI vector to dst_addr so it can run in 16-bit mode. */
    memcpy(dst_addr, &sipi_entry, sipi_sz);

    /* INIT */
    apic_icr_write(APIC_DEST_ALLBUT | APIC_DEST_PHYSICAL | APIC_DM_INIT | APIC_INT_ASSERT, 0);

    /* SIPI */
    apic_icr_write(APIC_DEST_ALLBUT | APIC_DEST_PHYSICAL | APIC_DM_STARTUP, 0);

    _cpu_count = fwcfg_get_nb_cpus();

    while (_cpu_count != cpu_online_count) {
        ;
    }
}
