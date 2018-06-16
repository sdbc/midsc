	.file	"set_sp.c"
	.text
	.p2align 4,,15
/*************************************************
 * char * set_sp(char *last_fiber_stack)
 * last_fiber_stack  -> %rdi
 ************************************************/

.globl set_sp
	.type	set_sp, @function
set_sp:
.LFB0:
	.cfi_startproc
//���淵ַ
	movq	(%rsp),%rax
	movq	%rax,-8(%rdi)
//����rbp��rsp�ľ���
	movq	%rbp,%rax
	subq	%rsp,%rax
//�趨sp��fiberջ
	movq	%rdi,%rbp
	subq	$8,%rbp
	movq	%rbp,%rsp
//�µ�rbp������rsp��ԭ����ͬ
	addq	%rax,%rbp
	movq	%rdi, %rax
	ret
	.cfi_endproc
.LFE0:
	.size	set_sp, .-set_sp
	.p2align 4,,15
.globl restore_sp
/***********************************************************************
 * char *restore_sp(char *to,char *from,unsigned long BP,unsigned ling size)
 * to -> %rdi,from -> %rsi, BP->%rdx(����),size->%rcx
 * to,������ͬһ���̣߳��������ײ�������Ӧ�õ�ջ����
 ************************************************************************/
	.type	restore_sp, @function
restore_sp:
.LFB1:
	.cfi_startproc
//��8�ֽڷ��򿽱�����
	subq	$8,%rdi
	subq	$8,%rsi
	shrq	$3,%rcx
	std
	rep	movsq
//%rdiӦ���������Ԫ֮��
	cld
	movq	%rdi,%rax
//��ַ�趨
	movq	(%rsp),%rbx
	movq	%rbx,(%rdi)
//�趨rbp
	subq	%rsp,%rbp
	addq	%rdi,%rbp
//rsp�趨����λ��
	movq	%rdi,%rsp
//����ջ��
	ret
	.cfi_endproc
.LFE1:
	.size	restore_sp, .-restore_sp
	.ident	"GCC: (GNU) 4.4.7 20120313 (Red Hat 4.4.7-3)"
	.section	.note.GNU-stack,"",@progbits
