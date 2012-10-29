/* "Exception calls" with GCC/x86; analogous to call/cc in a functional
 * language.  Includes implementation of classic call/cc test, "amb".
 *
 * .......yeaaaaaaaaaaahhhhhhhhhh.
 *
 * "I made a fail!"
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _XOPEN_SOURCE 500

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ucontext.h>

#define EXNCALL_STACK_SIZE 65536

void *current_stack;

struct exn {
	ucontext_t saved_ctx;
	void *saved_stack;
	int jumped;
};

void *allocate_stack()
{
	void *stack = malloc(EXNCALL_STACK_SIZE);
	memset(stack, 0xCC, EXNCALL_STACK_SIZE);
	return stack;
}

void copy_stack(void *dest, void *src)
{
	memcpy(dest, src, EXNCALL_STACK_SIZE);
}

int exnset(struct exn *e, int rv)
{
	e->jumped = 0;
	getcontext(&e->saved_ctx);

	/* Are we returning after an exncall()? */
	if (e->jumped)
		return rv;

	/* Allocate up a new stack and save our state there. */
	e->saved_stack = allocate_stack();
	copy_stack(e->saved_stack, current_stack);

	e->jumped = 1;
	return 0;
}

ucontext_t stack_copy_context;
char stack_copy_stack[EXNCALL_STACK_SIZE] __attribute__((aligned(16)));

void __attribute__((noreturn)) exncall(struct exn *e)
{
	/* Swap to temporary stack to blast over our current one */
	getcontext(&stack_copy_context);
	stack_copy_context.uc_stack.ss_size = sizeof(stack_copy_stack);
	stack_copy_context.uc_stack.ss_sp = stack_copy_stack;
	stack_copy_context.uc_link = &e->saved_ctx;

	/* Bounce over to the temporary stack, restore the stack that
	 * exnset() saved, and then return to it */
	makecontext(&stack_copy_context, (void(*)())copy_stack, 2,
		current_stack, e->saved_stack);
	setcontext(&stack_copy_context);

	/* setcontext() should really be marked __attribute__((noreturn)),
	 * but it isn't on some systems. */
	assert(!"setcontext() returned?");
	while(1);
}

void exnfree(struct exn *e)
{
	free(e->saved_stack);
}

struct exn e;

int later()
{
	int rv;
	printf("Entering later().\n");
	rv = exnset(&e, 1);
	printf("later() returning %d\n", rv);
	return rv;
}

void trashlater()
{
	char suq[2048];
	memset(suq, 0, 2048);
	exncall(&e);
}

/* AMB in C.  You have to monomorphize the void * yourself */
static struct exn ambexn;

void *amb(void **list)
{
	struct exn oldexn;
	
	memcpy(&oldexn, &ambexn, sizeof(struct exn));
	
	while (*list)
	{
		void *relt = *list;
		if (exnset(&ambexn, 1))
		{
			list++;
			continue;
		}
		return relt;
	}
	exnfree(&ambexn);
	memcpy(&ambexn, &oldexn, sizeof(struct exn));
	exncall(&ambexn);
	return NULL;
}

void ambfail() __attribute__ ((noreturn));

void ambfail()
{
	exncall(&ambexn);
}

int ambstart()	/* Returns 1 if we came here because amb failed. */
{
	return exnset(&ambexn, 1);
}

void *l1[] = {(void*)1, (void*)2, (void*)3, (void*)4, NULL};
void *l2[] = {(void*)4, (void*)5, (void*)3, (void*)4, NULL};
void *l3[] = {(void*)6, (void*)7, (void*)8, (void*)9, NULL};

void test()
{
	int rv, r1, r2;
	
	printf("Testing exception call.\n");
	if (later() == 0)
	{
		printf("later() returned zero, going to do something that would trash the stack that later() expected...\n");
		trashlater();
	} else
		printf("later() returned nonzero\n");
	
	printf("\nTesting amb.\n");
	rv = ambstart();
	printf(" - ambstart() returned %d\n", rv);
	if (rv != 0)
		return;
	r1 = (int)(uintptr_t)amb(l1);
	printf("   - amb(l1) returned %d\n", r1);
	r2 = (int)(uintptr_t)amb(l2);
	printf("     - amb(l2) returned %d\n", r2);
	if (r1 != r2)
	{
		printf("       - r1 != r2; failing\n");
		ambfail();
	}
	printf("found success!\n");
	
	printf("\nOK, now testing a failing amb.\n");
	rv = ambstart();
	printf(" - ambstart() returned %d\n", rv);
	if (rv != 0)
	{
		printf("amb search failed!\n");
		return;
	}
	r1 = (int)(uintptr_t)amb(l2);
	printf("   - amb(l2) returned %d\n", r1);
	r2 = (int)(uintptr_t)amb(l3);
	printf("     - amb(l3) returned %d\n", r2);
	if (r1 != r2)
	{
		printf("       - r1 != r2; failing\n");
		ambfail();
	}
	printf("found success! (TYPE A?)\n");
}

int main()
{
	ucontext_t main_ctx;
	ucontext_t test_ctx;

	getcontext(&test_ctx);
	test_ctx.uc_stack.ss_size = EXNCALL_STACK_SIZE;
	test_ctx.uc_stack.ss_sp = allocate_stack();
	test_ctx.uc_link = &main_ctx;
	makecontext(&test_ctx, test, 0);

	current_stack = test_ctx.uc_stack.ss_sp;
	swapcontext(&main_ctx, &test_ctx);
	return 0;
}
