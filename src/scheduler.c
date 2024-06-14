#include "header/scheduler/scheduler.h"
#include "header/cpu/portio.h"

#define PIT_MAX_FREQUENCY   1193182
#define PIT_TIMER_FREQUENCY 1000
#define PIT_TIMER_COUNTER   (PIT_MAX_FREQUENCY / PIT_TIMER_FREQUENCY)

#define PIT_COMMAND_REGISTER_PIO          0x43
#define PIT_COMMAND_VALUE_BINARY_MODE     0b0
#define PIT_COMMAND_VALUE_OPR_SQUARE_WAVE (0b011 << 1)
#define PIT_COMMAND_VALUE_ACC_LOHIBYTE    (0b11  << 4)
#define PIT_COMMAND_VALUE_CHANNEL         (0b00  << 6) 
#define PIT_COMMAND_VALUE (PIT_COMMAND_VALUE_BINARY_MODE | PIT_COMMAND_VALUE_OPR_SQUARE_WAVE | PIT_COMMAND_VALUE_ACC_LOHIBYTE | PIT_COMMAND_VALUE_CHANNEL)

#define PIT_CHANNEL_0_DATA_PIO 0x40

void activate_timer_interrupt(void) {
    __asm__ volatile("cli");
    // Setup how often PIT fire
    uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
    out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t)(pit_timer_counter_to_fire & 0xFF));
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t)((pit_timer_counter_to_fire >> 8) & 0xFF));

    // Activate the interrupt
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));
}

static int32_t current_running_process_index = -1;

int32_t get_next_process_index(void) {
    return (current_running_process_index + 1) % process_manager_state.active_process_count;
}

void scheduler_init(void) {
    activate_timer_interrupt();
    process_manager_state.active_process_count = 1;
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock* current_pcb = process_get_current_running_pcb_pointer();
    if (current_pcb != NULL) {
        current_pcb->context = ctx;
    }
}

void scheduler_switch_to_next_process(void) {
    // struct ProcessControlBlock* current_pcb = process_get_current_running_pcb_pointer();
    // scheduler_save_context_to_current_running_pcb(current_pcb->context);

    int32_t next_running_process_index = get_next_process_index();
    struct ProcessControlBlock* next_pcb = &(_process_list[next_running_process_index]);
    current_running_process_index = next_running_process_index;
    paging_use_page_directory(next_pcb->context.page_directory_virtual_addr);
    process_context_switch(next_pcb->context);
}