/* Copyright (C) 2000 Corel Corporation */
	.globl __menter
	.globl __mexit

.text
	.align 4
	.type __menter,@function
__menter:
	pushl %eax
	pushl %ecx
	pushl %edx

	pushl 16(%esp)

	call __menter_internal

	popl %edx
	popl %ecx
	popl %eax

	ret
1:
	.size __menter, 1b-__menter

	.align 4
	.type __mexit,@function
__mexit:
	pushl %eax
	pushl %ecx
	pushl %edx

	call __mexit_internal

	popl %edx
	popl %ecx
	popl %eax
	ret
1:
	.size __mexit, 1b-__mexit

/* gcc-2.95.2 outputs calls to these functions */
	.globl __cyg_profile_func_enter
	.set __cyg_profile_func_enter, __menter

	.globl __cyg_profile_func_exit
	.set __cyg_profile_func_exit, __mexit
