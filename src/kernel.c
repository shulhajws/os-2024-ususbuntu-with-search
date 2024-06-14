#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/cpu/portio.h"
#include "header/driver/framebuffer.h"
#include "header/driver/keyboard.h"
#include <stdbool.h>
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"
#include "header/driver/disk.h"
#include "header/filesystem/fat32.h"
#include "header/memory/paging.h"
#include "header/process/process.h"
#include "header/scheduler/scheduler.h"
#include "header/clock.h"


void kernel_setup(void)
{
  load_gdt(&_gdt_gdtr);
  pic_remap();
  activate_keyboard_interrupt();
  initialize_idt();
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);
  keyboard_state_activate();
  initialize_filesystem_fat32();

  gdt_install_tss();
  set_tss_register();

  // Allocate first 4 MiB virtual memory
  paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*)0);

  // Write shell into memory
  struct FAT32DriverRequest request = {
      .buf = (uint8_t*)0,
      .name = "shell",
      .ext = "\0\0\0",
      .parent_cluster_number = ROOT_CLUSTER_NUMBER,
      .buffer_size = 0x100000,
  };
  read(request);

  // Set TSS $esp pointer and jump into shell
  set_tss_kernel_current_stack();

  read_rtc();

  // Create & execute process 0
  // process_create_user_process(request);
  // paging_use_page_directory(_process_list[0].context.page_directory_virtual_addr);
  // kernel_execute_user_program((void*)0x0);

  // Create init process and execute it
  process_create_user_process(request);
  scheduler_init();
  scheduler_switch_to_next_process();
}
