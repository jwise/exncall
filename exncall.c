/* "Exception calls" with GCC/x86; analogous to call/cc in a functional
 * language.  Includes implementation of classic call/cc test, "amb".
 *
 * .......yeaaaaaaaaaaahhhhhhhhhh.
 *
 * "I made a fail!"
 */

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

struct exn {
	uint32_t oldesp;
	uint32_t stklow;
	uint32_t stksize;
	void *stkp;	/* contains stksize bytes to be put back at stklow */
	int rv;
} __attribute__((packed));

int exnset(struct exn *e, int rv)
{
	static int measured = 0;
	static uint32_t stklow, stksize;
	
	if (!measured)
	{
		char buf[32];
		int n;
		FILE *fp;
		uint32_t low, high;
		char name[32];
		char line[1024];
		
		n = snprintf(buf, 32, "/proc/%d/maps", getpid());
		assert(n < 32);
		fp = fopen(buf, "r");
		if (!fp)
			return -1;
		while (fgets(line, 1024, fp) != NULL)
			if (sscanf(line, "%lx-%lx %*s %*s %*s %*s %32s\n", &low, &high, name) != EOF)
			{
				if (!strcmp(name, "[stack]"))
				{
					measured = 1;
					stklow = low;
					stksize = high - low;
					break;
				}
			}
		fclose(fp);
		if (!measured)
		{
			printf("WARNING: exnset could not find stack?\n");
			return -1;
		}
	}
	
	e->stklow = stklow;
	e->stksize = stksize;
	e->stkp = malloc(stksize);
	if (!e->stkp)
		return -1;
	e->rv = rv;
	rv = __exnset(e);
	return rv;
}

void exncall(struct exn *e) __attribute__ ((noreturn));
extern void __exncall(struct exn *e) __attribute__ ((noreturn));

void exncall(struct exn *e)
{
	__exncall(e);
}

void exnfree(struct exn *e)
{
	free(e->stkp);
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
	int rv = 0;
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

int main()
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
	r1 = (int)amb(l1);
	printf("   - amb(l1) returned %d\n", r1);
	r2 = (int)amb(l2);
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
	r1 = (int)amb(l2);
	printf("   - amb(l2) returned %d\n", r1);
	r2 = (int)amb(l3);
	printf("     - amb(l3) returned %d\n", r2);
	if (r1 != r2)
	{
		printf("       - r1 != r2; failing\n");
		ambfail();
	}
	printf("found success! (TYPE A?)\n");
}
