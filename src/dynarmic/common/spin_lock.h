/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

namespace oaknut {
struct ExternalAllocator;
}

namespace Dynarmic {

/// Must be called before any SpinLock is used, if the platform requires
/// an external allocator for executable memory (e.g. iOS 26 TXM).
void SetSpinLockAllocator(const oaknut::ExternalAllocator* alloc);

struct SpinLock {
    void Lock();
    void Unlock();

    volatile int storage = 0;
};

}  // namespace Dynarmic
