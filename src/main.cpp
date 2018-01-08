#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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

int main()
{
	const char * testptr = "Hello, it's working.";
	uintptr_t addr = (uintptr_t)testptr;
	
	if (check_tsx())
	{
		printf("CPU has tsx! canbe faster.\n");
	}
	
	CMeltReader rdr;
	
	uint32_t len = 32;
	char * buf = (char*)malloc(len);
	memset(buf, 0xff, len);
	uint32_t failed = 0;
	for (uint32_t i=0; i<len; ++i)
	{
		int v = rdr.read_byte(addr+i);
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

