#include <stdio.h>
#include <unistd.h>

void libcall(void);

#ifdef TEST_LIB

void libcall2(void);
void libcall1(void);
void libcall(void);
void xlibcall(void);

void
libcall2(void)
{
	xlibcall();
	printf("-\n");
}

void
libcall1(void)
{
	libcall2();
}

void
libcall(void)
{
	libcall1();
}

void
xlibcall(void)
{
}

#else

static int
a()
{
	libcall();
}

static int
b()
{
	for (int i = 0; i < 10; i++)
		a();
}

static int
c()
{
	for (int i = 0; i < 10; i++) {
		a();
		b();
	}
}

static int
d()
{
	libcall();
	for (int i = 0; i < 10; i++) {
		a();
		b();
		c();
	}
}

int
main()
{
	d();
	printf("test done\n");
}

#endif