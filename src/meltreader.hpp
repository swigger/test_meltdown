#pragma once


class CMeltReader
{
protected:
	char * m_probe_array;
	long m_th1, m_th2;
	
public:
	static void dump_hex(uintptr_t pos, const void * buf, size_t len, FILE * fp);
	
	CMeltReader();
	~CMeltReader();
	int read_byte(uintptr_t addr);

private:
	void find_cache_hit_threshold();
};
