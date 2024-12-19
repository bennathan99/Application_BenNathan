.text                           # IMPORTANT: subsequent stuff is executable
.global  set_tod_from_ports
        
## ENTRY POINT FOR REQUIRED FUNCTION
set_tod_from_ports:
 
        movl    CLOCK_TIME_PORT(%rip), %ecx    # copy global var to reg ecx
        cmpl    $0, %ecx    #ecx - 0
        jl  .FAIL   #jump if less than 0 
        cmpl    $1382400, %ecx #ecx - 1382400
        jg  .FAIL   #jump if greater than 1382400

        movl    %ecx, %r9d  #r9d is seconds
        sarL    $4, %r9d    #seconds = time >> 4;

        movl    $1, %r8d    #%r8d = 1
        sall    $3, %r8d    #%r8d = 1<<3
        testL   %r8d, %ecx  #time & (1 << 3)
        jz .DONTADDONE
        incL    %r9d #add one to seconds

    .DONTADDONE:

        movl    %r9d, (%rdi)   #tod->day_secs = seconds

        movl    %r9d, %eax  #setting numerator to seconds
        cqto
        movl    $3600, %ecx #ecx = 60*60
        idivl   %ecx    #seconds/60, stored in %eax
        cmpl    $12, %eax
        jge .AFTERNOON
        jl  .MORNING

    .AFTERAMPM:

        cmpl    $0, %eax
        jne .NOTTWELVE
        cmpb    $1, 10(%rdi)
        jne .NOTTWELVE
        movw    $12, 8(%rdi)    #tod->time_hours = 12
        jmp .AFTERHOURS

    .NOTTWELVE:

        cmpl    $12, %eax
        jle .ELSE
        cmpb    $2, 10(%rdi)
        jne .ELSE
        movw    %ax, 8(%rdi)
        subw    $12, 8(%rdi) #tod->time_hours = seconds/(60*60) - 12 
        jmp .AFTERHOURS

    .ELSE:

        movw    %ax, 8(%rdi)

    .AFTERHOURS:

        movl   %edx, %r9d  #seconds %= 60*60

        movl    %r9d, %eax  #setting numerator to seconds
        cqto
        movl    $60, %ecx #ecx = 60
        idivl   %ecx    #seconds/60
        movw    %ax, 6(%rdi)
        movw    %dx, 4(%rdi)

        movl    $0, %eax
        ret

    .AFTERNOON:

        movb    $2, 10(%rdi)    #tod->ampm = 2
        jmp .AFTERAMPM

    .MORNING:

        movb    $1, 10(%rdi)    #tod->ampm = 1
        jmp .AFTERAMPM

    .FAIL: 

        movl    $1, %eax
        ret
       
        ## DON'T FORGET TO RETURN FROM FUNCTIONS

### Change to definint semi-global variables used with the next function 
### via the '.data' directive
.data                           # IMPORTANT: use .data directive for data section
	
thirtytwo:

        .int 0x20 #32 bits

sixteen:

        .int 0x10 #16 bits

eight:

        .int 0b11111111 #8 bits

displays:                       # declare multiple ints sequentially starting at location
        .int 0b1110111
        .int 0b0100100
        .int 0b1011101
        .int 0b1101101
        .int 0b0101110
        .int 0b1101011
        .int 0b1111011
        .int 0b0100101
        .int 0b1111111
        .int 0b1101111                 


.text
.global  set_display_from_tod

## ENTRY POINT FOR REQUIRED FUNCTION
set_display_from_tod:

         subq   $24, %rsp   #get 88 bits of stack space

         movq   $0, %rcx

         movq   %rdx, %r11 #copy address

         movq   %rdi, (%rsp)  #%rsp = tod.day_secs
         andq   $0x20, (%rsp)  #first 32 bits

         #tod.day_secs is now in %rsp

         sarq   $0x20, %rdi #shift first input right by 32 bits
         movq   %rdi, %rax
         andq   $0x10, %rax
         movw   %ax, 4(%rsp)    #4(%rsp) now contains tod.time_secs

         sarq   $0x10, %rdi #total shift is now 48
         movw   %di, 6(%rsp)    #tod.time_mins is now 6(%rsp)

         movw   %si, 8(%rsp)   #tod.time_hours is now 8(%rsp)

         sarq   $0x10, %rsi #shift right by 16
         andq   eight(%rip), %rsi
         movb   %sil, 10(%rsp)  #10(%rsp) now contains tod.ampm

         cmpl   $0, (%rsp) #tod.day_secs < 0
         jl .ERROR
         cmpw   $0, 4(%rsp) #tod.time_secs < 0
         jl .ERROR
         cmpw   $59, 4(%rsp) #tod.time_secs > 59
         jg .ERROR
         cmpw   $0, 6(%rsp) #tod.time_mins < 0
         jl .ERROR
         cmpw   $59, 6(%rsp) #tod.time_mins > 59
         jg .ERROR
         cmpw   $0, 8(%rsp) #tod.time_hours < 0
         jl .ERROR
         cmpw   $12, 8(%rsp) #tod.time_hours > 12
         jg .ERROR
         cmpb   $1, 10(%rsp) #tod.ampm != 1 && tod.ampm != 2
         je .NOTERROR
         cmpb   $2, 10(%rsp)
         je .NOTERROR
         jmp .ERROR #if no jump then above condition is false

    .NOTERROR:

         movw   6(%rsp), %ax #setting numerator to tod.time_mins
         cqto
         movw   $10, %cx #setting denominator to 10
         idivw  %cx #tod.time_mins / 10

         movw   %dx, %si #min_ones is %si 
         movw   %ax, %di #min_tens is %di

         movw   8(%rsp), %ax  #setting numerator to tod.time_hours\
         cqto
         movw   $10, %cx #setting denominator to 10
         idivw  %cx

         movw   %ax, %cx #%cx is hours_tens
         movl   %edx, %r8d   #%r8w is hours_ones

         leaq   displays(%rip), %r9 #r9 points to array
         movl   (%r9, %rsi, 4), %r10d  #finalVal = displays[min_ones]

         movl   (%r9, %rdi, 4), %eax #eax is displays[min_tens]
         sall   $7, %eax #displays[min_tens] << 7
         orl    %eax, %r10d #finalVal |= displays[min_tens] << 7;

         movl   (%r9, %r8, 4), %eax #eax is displays[hours_ones]
         sall   $14, %eax #displays[hours_ones] << 14
         orl    %eax, %r10d #finalVal |= displays[min_tens] << 14;

         cmpw   $0, %cx
         je .ONLYMINS
         movl   (%r9, %rcx, 4), %eax #eax is displays[hours_tens]
         sall   $21, %eax #displays[hours_tens] << 21
         orl    %eax, %r10d #finalVal |= displays[hours_tens] << 21;

    .ONLYMINS:

         cmpb   $1, 10(%rsp)
         jne .OTHERWISE

         movl   $1, %eax
         sall   $28, %eax #1 << 28
         orl    %eax, %r10d #finalVal |= 1 << 28

         jmp .SETDISPLAY

    .OTHERWISE:

         movl   $1, %eax
         sall   $29, %eax #1 << 29
         orl    %eax, %r10d #finalVal |= 1 << 29

    .SETDISPLAY:

         
         movl   %r10d, (%r11) # *display = finalVal;
         addq   $24, %rsp
         movl   $0, %eax
         ret

    .ERROR:

        addq    $24, %rsp
        movl    $1, %eax
        ret

.text
.global clock_update
        
## ENTRY POINT FOR REQUIRED FUNCTION
clock_update:
	
        subq    $24, %rsp

        movq    $0, %rsi

        movq    $0, (%rsp)
        movw    $0, 8(%rsp)
        movb    $0, 10(%rsp)
        movb    $0, 11(%rsp)    
        movl    $0, 12(%rsp) #time

        leaq     (%rsp), %rdi

        call    set_tod_from_ports   

        cmpl    $1, %eax
        je .ERROR1

        
        movq    (%rsp), %rdi    #arg 1
        movl    8(%rsp), %esi   #arg 2
        leaq    12(%rsp), %rdx    #time
    
        call    set_display_from_tod  

        cmpl    $1, %eax
        je .ERROR1

        movl    12(%rsp), %eax
        movl    %eax, CLOCK_DISPLAY_PORT(%rip)

        addq    $24, %rsp
        movl    $0, %eax
        ret

    .ERROR1:

        addq    $24, %rsp
        movl    $1, %eax
        ret