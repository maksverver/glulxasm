SECTIONS
{
    . = 0x0a000000;
    .rodata.story   : ALIGN(0x1000) { storycode.o(.rodata) }  /* func_map[] */
    .text.story     : ALIGN(0x1000) { storycode.o(.text) }    /* story code */

    . = 0x0b000000;
    .bss.call_stack : ALIGN(0x1000) { bss_call_stack.o(.bss) }
    . += 0x100000;  /* gap to encourge page faults on overflow */
    .bss.data_stack : ALIGN(0x1000) { bss_data_stack.o(.bss) }
    . += 0x100000;  /* gap to encourge page faults on overflow */
    .bss.mem        : ALIGN(0x1000) { bss_mem.o(.bss) }
}
INSERT AFTER .bss;
