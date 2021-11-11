#ifndef _X86_XSAVE_H_
#define _X86_XSAVE_H_

#define X86_CR4_OSXSAVE			0x00040000
#define XCR_XFEATURE_ENABLED_MASK       0x00000000
#define XCR_XFEATURE_ILLEGAL_MASK       0x00000010

#define XSTATE_FP       0x1
#define XSTATE_SSE      0x2
#define XSTATE_YMM      0x4

int xgetbv_checking(u32 index, u64 *result);
int xsetbv_checking(u32 index, u64 value);
uint64_t get_supported_xcr0(void);

#endif
