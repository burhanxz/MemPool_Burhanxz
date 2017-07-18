#pragma once
#include <string>
#include <memory>

#if defined(_WIN64) 
typedef __int64 INT_PTR; 
#else 
typedef int INT_PTR;
#endif

using namespace std;
const int MAX_ALLOC_FROM_PAGE = 2048; //critical allocation size more than which we must be allocated from large memory space
const int IS_NOT_IN_MEMPOOL = -1; //indicate that this memory block is not in mempool but in mempage
const int PAGE_POSTFIX_SIG = 0; //indicate this postfix is belong to a mempage
/* prefix is set to indicate the next, prev, postfix and relative memory page    */
_declspec(align(16)) struct PrefixNest
{
	Prefix *pPrev;
	Prefix *pNext;
	Postfix *pPostfix;
	MemPageHeaderNest *pMemPage;
};
typedef struct tagPERFIX
{
	PrefixNest d;
	char *pAddr;
}Prefix;
/* Postfix is used to indicate the prefix and their linklist's number */
_declspec(align(16)) struct Postfix
{
	Prefix *pPrefix;
	int nBlockPoolIndex;
	unsigned nActualSize;
	unsigned nSig;
};
/* MemPageHeader is set to manage the allocated page */
_declspec(align(16)) struct MemPageHeaderNest
{
	char *pLast;
	char *pEnd;
	MemPageHeader *pNext;
	unsigned nSize;
};
typedef struct tagPAGEHEADER
{
	MemPageHeaderNest d;
	Postfix postf;
}MemPageHeader;
