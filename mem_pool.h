#pragma once
#include "mem_base.h"

typedef void (*CleanupHandler)(void *param);

class MemPool
{
private:
	MemPool();

	bool AllocPage();
	void *AllocLarge(int, const char *const, int, bool);
	bool isInBlockPool(Prefix *pPrefix) const 
	{
		return pPrefix->d.pPostfix->nBlockPoolIndex >= 0;
	}
public:
	~MemPool();
	static MemPool& GetInstance();
	void *Alloc(int,const char *const,int,bool);
	void Free(void *);
	void AddCleanup(CleanupHandler, void *);
private:
	MemPageHeader *m_pFirstPage;
	Prefix *m_pLarge;
	Prefix *m_ppBlockPools[20];
	int m_pnBlocksAllocated[20];

	struct Cleanup 
	{
		CleanupHandler pHandler;
		void *pParam;
		Cleanup *pNext;
	} *m_pCleanups;

	static MemPool m_heap;
//	Crticalsection m_cs;
};
