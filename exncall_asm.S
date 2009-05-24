.globl __exnset
__exnset:
	mov 4(%esp), %eax
	push %ebx
	push %esi
	push %edi
	push %ebp
	mov %esp, 0(%eax)	// OK, old %esp saved, let's rock and roll
	
	mov 4(%eax), %esi
	mov 8(%eax), %ecx
	mov 12(%eax), %edi
	cld
	rep movsb
	
	pop %ebp
	pop %edi
	pop %esi
	pop %ebx
	xor %eax, %eax
	ret
	
.globl __exncall
__exncall:
	mov 4(%esp), %eax
	mov 0(%eax), %esp
	mov 4(%eax), %edi
	mov 8(%eax), %ecx
	mov 12(%eax), %esi
	cld
	rep movsb
	pop %ebp
	pop %edi
	pop %esi
	pop %ebx
	mov 16(%eax), %eax
	ret
