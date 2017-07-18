#include "mem_pool.h"
#include <memory>
#include <iostream>
/*construct*/
MemPool::MemPool()
{
	m_pFirstPage = nullptr;
	m_pLarge = new Prefix;
	memset(m_pLarge,0,sizeof(Prefix));

	m_pCleanups = nullptr;

	for (int i = 0;i != 20;i++)
	{
		m_ppBlockPools[i] = new Prefix;
		memset(m_ppBlockPools[i],0,sizeof(Prefix));
	}
	memset(m_pnBlocksAllocated,0,sizeof(int) * 20);
}
/*deconstruct*/
MemPool::~MemPool()
{
	for (Cleanup *p = m_pCleanups;p != nullptr;p = p->pNext)
	{
		if(p->pHandler)
			p->pHandler(p->pParam);
	}

	while (m_pCleanups!= nullptr)
	{
		Cleanup *p = m_pCleanups;
		m_pCleanups = m_pCleanups->pNext;
		delete p;
	}

	while (m_pFirstPage != nullptr)
	{
		MemPageHeader *p = m_pFirstPage;
		m_pFirstPage = m_pFirstPage->d.pNext;
		_aligned_free(p);
	}

	Prefix *p = m_pLarge;
	m_pLarge = m_pLarge->d.pNext;
	delete p;
	while (m_pLarge != nullptr)
	{
		p = m_pLarge;
		m_pLarge = m_pLarge->d.pNext;
		_aligned_free(p);
	}

	for(int i = 0;i != 20;i++)
	{
		delete m_ppBlockPools[i];
	}
}
/* get instance of mempool */
MemPool& MemPool::GetInstance()
{
	return m_heap;
}
/*the access to allcate memory*/
void* MemPool::Alloc(int nSize,const char *const szFileName,int nLine,bool bIsArray)
{
	if(nSize < MAX_ALLOC_FROM_PAGE)
	{
		int nMinPower2 = 16; 
		int i = 0;
		for (;nMinPower2 < nSize;nMinPower2 = nMinPower2 << 1,i ++);
		
//		SYNCHRONIZATION();
		/*chech if relative memory pool (linklist) have memory blocks or not*/
		if(m_ppBlockPools[i]->d.pNext != nullptr)
		{
			Prefix *pPrefix = m_ppBlockPools[i]->d.pNext;

			/* divide this memory block from linklist */
			m_ppBlockPools[i]->d.pNext = pPrefix->d.pNext;
			if(pPrefix->d.pNext != nullptr)
				pPrefix->d.pNext->d.pPrev = m_ppBlockPools[i];
			pPrefix->d.pNext = pPrefix->d.pPrev = nullptr;

			pPrefix->d.pPostfix->nBlockPoolIndex = IS_NOT_IN_MEMPOOL;

			pPrefix->d.pPostfix->nActualSize = nSize;
			++m_pnBlocksAllocated[i];
			return pPrefix->pAddr;
		}
		/* allocate from page */
		const int nAllocSize = nMinPower2;
		do 
		{
			for (MemPageHeader *pPage = m_pFirstPage;pPage != nullptr;pPage = pPage->d.pNext)
			{
				if(pPage->d.pEnd >= pPage->d.pLast + sizeof(Prefix) + nAllocSize + sizeof(Postfix) )
				{
					/* allocate a page and set their prefix and postfix */
					Prefix *pPrefix = (Prefix*) pPage->d.pLast;
					pPrefix->d.pPostfix = (Postfix*) (pPrefix->pAddr + nAllocSize);
					pPrefix->d.pPostfix->pPrefix = pPrefix;
					pPrefix->d.pPostfix->nBlockPoolIndex = IS_NOT_IN_MEMPOOL;

					pPrefix->d.pPostfix->nActualSize = nMinPower2;
					pPrefix->d.pPrev = pPrefix->d.pNext = nullptr;
					pPrefix->d.pMemPage = pPage;
					pPage->d.pLast += sizeof(Prefix) + nAllocSize + sizeof(Postfix);

					++m_pnBlocksAllocated[i];
					return pPrefix->pAddr;
				}
			}
		} while (AllocPage());

		/*can't allocate any memory space*/
		std::cerr << "Memory allocation failure" << std::endl;
		return nullptr;
	}

	/* when apply for large memory space*/
	return AllocLarge(nSize,szFileName,nLine,bIsArray);
}
/* allocate page */
bool MemPool::AllocPage()
{
	/* apply for a page in size of 2048 * 2 */
	MemPageHeader *p = (MemPageHeader*) _aligned_malloc(2 * MAX_ALLOC_FROM_PAGE, 16);
	if(p == nullptr)
		return false;
	
	/* set this page's parameters */
	p->d.pLast = (char *) p + sizeof(MemPageHeader); // page's last avilable space excludes MemPage's Header
	p->d.pEnd = (char *) p + MAX_ALLOC_FROM_PAGE * 2;
	p->d.nSize = MAX_ALLOC_FROM_PAGE * 2;

	/* the postfix belong to mempage is particularly set */
	p->postf.pPrefix = nullptr;
	p->postf.nSig = PAGE_POSTFIX_SIG;

	/* set this mempage as the first mempage */
	p->d.pNext = m_pFirstPage;
	m_pFirstPage = p;

	return true;
}

void *MemPool::AllocLarge(int nSize,const char *const szFileName, int nLine, bool bIsArray)
{
	/* apply for a memory space */
	Prefix *pPrefix = (Prefix *) _aligned_malloc(sizeof(Prefix) + nSize + sizeof(Postfix), 16);
	if(pPrefix != nullptr)
	{
		/* set the prefix */
		pPrefix->d.pPostfix = (Postfix *) (pPrefix->pAddr + nSize);
		pPrefix->d.pPostfix->pPrefix = pPrefix;
		pPrefix->d.pMemPage = nullptr;

		/* insert this memory block to space between the large block linklist's head and its first elememt */
		pPrefix->d.pNext = m_pLarge->d.pNext;
		if (m_pLarge->d.pNext != nullptr)
			m_pLarge->d.pNext->d.pPrev = pPrefix;
		m_pLarge->d.pNext = pPrefix;
		pPrefix->d.pPrev = m_pLarge;

		return pPrefix->pAddr;
	}

	std::cerr << "Memory allocation failure" << std::endl;
	return nullptr;
}

void MemPool::Free(void *pMem)
{
//	SYNCHRONIZATION();

	Prefix *pPrefix = (Prefix *)pMem - 1;
	/* if allocated from page */
	if (pPrefix->d.pMemPage != nullptr)
	{
		/* work out the actual size we are allocated and find out the relative index */
		int nSize = (int)((INT_PTR) pPrefix->d.pPostfix - (INT_PTR) pPrefix->pAddr);
		int nMinPower2 = 16;
		int index = 0;
		for(; nMinPower2 < nSize; nMinPower2 = nMinPower2 << 1, index++ );
		/* relative block's number minus 1 */
		--m_pnBlocksAllocated[index];

		MemPageHeader *pPage = pPrefix->d.pMemPage;
		if (pPage->d.pLast == (char *)(pPrefix->d.pPostfix + 1)) // if this block is adjacent to the last position of page
		{
			pPage->d.pLast = (char *) pPrefix;
			do 
			{
				Postfix *pPostfix = (Postfix *)pPage->d.pLast - 1;
				/*  */
				if(pPostfix->pPrefix == nullptr && pPostfix->nSig == PAGE_POSTFIX_SIG)
					break;

				pPrefix->d.pPostfix->pPrefix = pPrefix;
				if(isInBlockPool(pPrefix))
				{
					Prefix *p1 = pPrefix->d.pPrev, *p2 = pPrefix->d.pNext;
					p1->d.pNext = p2;
					if(p2 != nullptr)
						p2->d.pPrev = p1;

					pPage->d.pLast = (char *) pPrefix;
				}
				else 
					break;

			} while (true);
		}
		else 
		{
			pPrefix->d.pNext = m_ppBlockPools[index]->d.pNext;
			if(m_ppBlockPools[index]->d.pNext != nullptr)
				m_ppBlockPools[index]->d.pNext->d.pPrev  = pPrefix;
			m_ppBlockPools[index]->d.pNext = pPrefix;
			pPrefix->d.pPrev = m_ppBlockPools[i];

			pPrefix->d.pPostfix->nBlockPoolIndex = index;
		}
	}
	/* if allocated from large memory */
	else 
	{
		Prefix *p1 = pPrefix->d.pPrev, *p2 = pPrefix->d.pNext;
		p1->d.pNext = p2;
		if(p2 != nullptr)
			p2->d.pPrev = p1;
		_aligned_free(pPrefix);
	}
}
/**/
void MemPool::AddCleanup(CleanupHandler pHandler,void *pParam)
{
	Cleanup *p = new Cleanup();
	p->pHandler = pHandler;
	p->pParam = pParam;
	p->pNext = m_pCleanups;
	m_pCleanups = p;
}
