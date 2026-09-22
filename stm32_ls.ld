/*Specifying the firmware's entry point*/
ENTRY(Reset_Handler) /*This is the entry point of the firmware - first piece of cade that gets executed when the firmware starts*/

/*detilaing avaliable memory*/
MEMORY /*We specify 2 memory regions our flash and sram*/
{
    FLASH(rx):ORIGIN =0x08000000,LENGTH =512K
    SRAM(rwx):ORIGIN =0x20000000,LENGTH =96K
}

/* Symbol creation - estack -> we set this to the end of the SRAM memory region - We use this symbol to initalise the stack pointer */
/* We set it at the end since the stack grows downwards */
_estack = ORIGIN(SRAM)+LENGTH(SRAM);

/*Specifying heap and stack sizes*/
/* Importnt for dynmic mem alloc  */
__max_heap_size = 0x200;
__max_stack_size = 0x400;

/*Defining output scetions*/
SECTIONS
{
    .text : /* Since this is out actual code and compiled code and RO vars we store it in flash, and since the CPU doesnt need any code to tell it where it starts and its already hard
    witred to look at the start addr. we wont need a _stext*/
    {
        . = ALIGN(4); /*Align the start of the .text section on a 4-byte boundary -> enhances memory access effciency -> crucial for processors fetching instructions in word sized chunks*/
        KEEP(*(.isr_vector_tbl)) /*KEEP stops --gc-sections discarding the vector table: nothing in C references it, the hardware just reads it from address 0. Pulls in all sections named .isr_tbl from the input files into the current location in the text section -> since we use the wildcard it gets all of them*/
        *(.text) *(.text*) /*The .text* pattern catches the per-function sections -ffunction-sections emits. Pulls all sections named .text from the input files into the current location in the .text seection*/
        *(.rodata) *(.rodata*) /*Pulls all sections named .rodata from the input files into the current location in the .text section*/
        . = ALIGN(4); /*Again sensure we align to a 4-byte boundary, we use it here to align the end of a section - ensures the next section starts on an aligned boundary*/
        _etext = .; /*We use this symbol as a ptr to the end of the .txt section to mark the end of the .text seciton*/
    }>FLASH /*specify that the .text seciton should be placed in the FLASH memory segment as defined earlier in the MEMMORY block seciton*/
    
    /*This section holds global vars that have starting values and since .data is stored in SRAM
    we need to move it from SRAM to FLASH, but we may need to change it later, and we cannot change vars
    in FLASH on the fly, so to chnage it the code needs to know where is that data stored in the FLASH (_sdata is start) and end of (_etext)*/
    .data :
    {
        . = ALIGN(4); /*Basic align to the 4-byte boundaries*/
        _sdata = .; /*Represent the start of the .data section by setting it to the current location counter*/
        *(.data) *(.data*) /*Pulls all sections named .data from all the input files into the curernt location*/
        . = ALIGN(4); /*Align the end of the section to a 4-byte boundary*/
        _edata = .; /*Similar to sdata, we need to know the end of the that data section which will be moved to flash*/
    } > SRAM AT> FLASH /*>(vma) AT> (lma); >SRAM denotes that the section should be lcoated in the SRAM, AT> FLASH tells the linker that 
    although the seciton is placed in SRAM for exec its inital values should be stored in flash*/
       
       /*Here we use sbss and ebss so that we can calculate the sections size and inital processes*/
        .bss :
        {
            . = ALIGN(4);
            _sbss = .;
            *(.bss) *(.bss*) *(COMMON)
            . = ALIGN(4);
            _ebss = .;
        }>SRAM
}