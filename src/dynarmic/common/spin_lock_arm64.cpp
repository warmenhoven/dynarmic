/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <mutex>

#include <oaknut/code_block.hpp>
#include <oaknut/oaknut.hpp>
#if defined(__APPLE__)
#    include <oaknut/dual_code_block.hpp>
#endif

#include "dynarmic/backend/arm64/abi.h"
#include "dynarmic/common/spin_lock.h"

namespace Dynarmic {

using Backend::Arm64::Wscratch0;
using Backend::Arm64::Wscratch1;
using namespace oaknut::util;

void EmitSpinLockLock(oaknut::CodeGenerator& code, oaknut::XReg ptr) {
    oaknut::Label start, loop;

    code.MOV(Wscratch1, 1);
    code.SEVL();
    code.l(start);
    code.WFE();
    code.l(loop);
    code.LDAXR(Wscratch0, ptr);
    code.CBNZ(Wscratch0, start);
    code.STXR(Wscratch0, Wscratch1, ptr);
    code.CBNZ(Wscratch0, loop);
}

void EmitSpinLockUnlock(oaknut::CodeGenerator& code, oaknut::XReg ptr) {
    code.STLR(WZR, ptr);
}

static const oaknut::ExternalAllocator* s_spin_lock_allocator = nullptr;

namespace {

struct SpinLockImpl {
    void Initialize();

#if defined(__APPLE__)
    std::unique_ptr<oaknut::DualCodeBlock> mem;
#else
    std::unique_ptr<oaknut::CodeBlock> mem;
#endif
    std::unique_ptr<oaknut::CodeGenerator> code;

    void (*lock)(volatile int*) = nullptr;
    void (*unlock)(volatile int*) = nullptr;
};

std::once_flag flag;
SpinLockImpl impl;

void SpinLockImpl::Initialize() {
#if defined(__APPLE__)
    mem = std::make_unique<oaknut::DualCodeBlock>(4096, s_spin_lock_allocator);
    code = std::make_unique<oaknut::CodeGenerator>(mem->wptr(), mem->xptr());
#else
    mem = std::make_unique<oaknut::CodeBlock>(4096);
    mem->unprotect();
    code = std::make_unique<oaknut::CodeGenerator>(mem->ptr(), mem->ptr());
#endif

    lock = code->xptr<void (*)(volatile int*)>();
    EmitSpinLockLock(*code, X0);
    code->RET();

    unlock = code->xptr<void (*)(volatile int*)>();
    EmitSpinLockUnlock(*code, X0);
    code->RET();

#if !defined(__APPLE__)
    mem->protect();
#endif
    mem->invalidate_all();
}

}  // namespace

void SetSpinLockAllocator(const oaknut::ExternalAllocator* alloc) {
    s_spin_lock_allocator = alloc;
}

void SpinLock::Lock() {
    std::call_once(flag, &SpinLockImpl::Initialize, impl);
    impl.lock(&storage);
}

void SpinLock::Unlock() {
    std::call_once(flag, &SpinLockImpl::Initialize, impl);
    impl.unlock(&storage);
}

}  // namespace Dynarmic
