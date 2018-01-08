#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "meltreader.hpp"

static uint32_t __inline cpuid(uint32_t r0, uint32_t sub, uint32_t & a, uint32_t &b, uint32_t & c, uint32_t &d)
{
	__asm__ __volatile__("cpuid\n\t" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "0"(r0),"2"(sub));
	return a;
}

static bool check_tsx()
{
	enum {
		CPUID_RTM = 1 << 11,
		CPUID_HLE = 1 << 4,
	};
	uint32_t a, b, c, d;
	if (cpuid(0, 0, a, b, c, d) >= 7)
	{
		cpuid(7, 0, a, b, c, d);
		return !!(b & CPUID_RTM);
	}
	return false;
}

void sigsegv(int sig, siginfo_t *siginfo, void *context)
{
	ucontext_t *ucontext = (ucontext_t*)context;
#ifdef __linux__
	ucontext->uc_mcontext.gregs[REG_RIP] += 20;
#elif defined(__APPLE__)
	ucontext->uc_mcontext->__ss.__rip += 20;
#else
#error "todo..."
#endif
}

int set_signal(void)
{
	struct sigaction act = {0};
	act.sa_sigaction = sigsegv;
	act.sa_flags = SA_SIGINFO;
	return sigaction(SIGSEGV, &act, NULL);
}

static void load_kernel_mem(void)
{
	int f = open("/proc/version", O_RDONLY);
	if (f>0)
	{
		char buf[100];
		read(f, buf, sizeof(buf));
		close(f);
	}
}

int main(int argc, char ** argv)
{
	const char * testptr = "Hello, it's working.";
	uintptr_t addr = (uintptr_t)testptr;
	
	if (check_tsx())
	{
		printf("CPU has tsx! canbe faster.\n");
	}
	set_signal();
	
	CMeltReader rdr;
	
	uint32_t len = 32;

	if (argc == 3)
	{
		addr = strtoull(argv[1], 0, 16);
		len = (uint32_t)strtoull(argv[2], 0, 0);
	}

	char * buf = (char*)malloc(len);
	memset(buf, 0xff, len);
	uint32_t failed = 0;
	for (uint32_t i=0; i<len; ++i)
	{
		int v = rdr.read_byte(addr+i, load_kernel_mem);
		if (v == -1) ++ failed;
		buf[i] = (char)v;
	}

	if (len == 0)
		return 0;
	else if (failed == len)
	{
		fprintf(stderr, "error: failed to read.\n");
		return 1;
	}
	else if (failed)
	{
		fprintf(stderr, "warning: failed to read %u bytes.\n", failed);
	}
	
	CMeltReader::dump_hex(addr, buf, len, stdout);
	free(buf);
	return 0;
}

