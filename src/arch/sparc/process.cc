/*
 * Copyright (c) 2003-2004 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "arch/sparc/process.hh"

#include "arch/sparc/asi.hh"
#include "arch/sparc/handlers.hh"
#include "arch/sparc/isa_traits.hh"
#include "arch/sparc/registers.hh"
#include "arch/sparc/types.hh"
#include "base/loader/elf_object.hh"
#include "base/loader/object_file.hh"
#include "base/logging.hh"
#include "cpu/thread_context.hh"
#include "debug/Stack.hh"
#include "mem/page_table.hh"
#include "params/Process.hh"
#include "sim/aux_vector.hh"
#include "sim/process_impl.hh"
#include "sim/syscall_return.hh"
#include "sim/system.hh"

using namespace std;
using namespace SparcISA;

const std::vector<int> SparcProcess::SyscallABI::ArgumentRegs = {
    INTREG_O0, INTREG_O1, INTREG_O2, INTREG_O3, INTREG_O4, INTREG_O5
};

SparcProcess::SparcProcess(ProcessParams *params,
                           ::Loader::ObjectFile *objFile, Addr _StackBias)
    : Process(params,
              new EmulationPageTable(params->name, params->pid, params->system, PageBytes),
              objFile),
      StackBias(_StackBias)
{
    fatal_if(params->useArchPT, "Arch page tables not implemented.");
    // Initialize these to 0s
    fillStart = 0;
    spillStart = 0;
}

void
SparcProcess::handleTrap(int trapNum, ThreadContext *tc, Fault *fault)
{
    PCState pc = tc->pcState();
    switch (trapNum) {
      case 0x01: // Software breakpoint
        warn("Software breakpoint encountered at pc %#x.\n", pc.pc());
        break;
      case 0x02: // Division by zero
        warn("Software signaled a division by zero at pc %#x.\n", pc.pc());
        break;
      case 0x03: // Flush window trap
        flushWindows(tc);
        break;
      case 0x04: // Clean windows
        warn("Ignoring process request for clean register "
                "windows at pc %#x.\n", pc.pc());
        break;
      case 0x05: // Range check
        warn("Software signaled a range check at pc %#x.\n", pc.pc());
        break;
      case 0x06: // Fix alignment
        warn("Ignoring process request for os assisted unaligned accesses "
                "at pc %#x.\n", pc.pc());
        break;
      case 0x07: // Integer overflow
        warn("Software signaled an integer overflow at pc %#x.\n", pc.pc());
        break;
      case 0x32: // Get integer condition codes
        warn("Ignoring process request to get the integer condition codes "
                "at pc %#x.\n", pc.pc());
        break;
      case 0x33: // Set integer condition codes
        warn("Ignoring process request to set the integer condition codes "
                "at pc %#x.\n", pc.pc());
        break;
      default:
        panic("Unimplemented trap to operating system: trap number %#x.\n", trapNum);
    }
}

void
SparcProcess::initState()
{
    Process::initState();

    ThreadContext *tc = system->getThreadContext(contextIds[0]);
    // From the SPARC ABI

    // Setup default FP state
    tc->setMiscRegNoEffect(MISCREG_FSR, 0);

    tc->setMiscRegNoEffect(MISCREG_TICK, 0);

    /*
     * Register window management registers
     */

    // No windows contain info from other programs
    tc->setIntReg(INTREG_OTHERWIN, 0);
    // There are no windows to pop
    tc->setIntReg(INTREG_CANRESTORE, 0);
    // All windows are available to save into
    tc->setIntReg(INTREG_CANSAVE, NWindows - 2);
    // All windows are "clean"
    tc->setIntReg(INTREG_CLEANWIN, NWindows);
    // Start with register window 0
    tc->setMiscReg(MISCREG_CWP, 0);
    // Always use spill and fill traps 0
    tc->setIntReg(INTREG_WSTATE, 0);
    // Set the trap level to 0
    tc->setMiscRegNoEffect(MISCREG_TL, 0);
    // Set the ASI register to something fixed
    tc->setMiscReg(MISCREG_ASI, ASI_PRIMARY);

    // Set the MMU Primary Context Register to hold the process' pid
    tc->setMiscReg(MISCREG_MMU_P_CONTEXT, _pid);

    /*
     * T1 specific registers
     */
    // Turn on the icache, dcache, dtb translation, and itb translation.
    tc->setMiscRegNoEffect(MISCREG_MMU_LSU_CTRL, 15);
}

void
Sparc32Process::initState()
{
    SparcProcess::initState();

    ThreadContext *tc = system->getThreadContext(contextIds[0]);
    // The process runs in user mode with 32 bit addresses
    PSTATE pstate = 0;
    pstate.ie = 1;
    pstate.am = 1;
    tc->setMiscReg(MISCREG_PSTATE, pstate);

    argsInit(32 / 8, PageBytes);
}

void
Sparc64Process::initState()
{
    SparcProcess::initState();

    ThreadContext *tc = system->getThreadContext(contextIds[0]);
    // The process runs in user mode
    PSTATE pstate = 0;
    pstate.ie = 1;
    tc->setMiscReg(MISCREG_PSTATE, pstate);

    argsInit(sizeof(RegVal), PageBytes);
}

template<class IntType>
void
SparcProcess::argsInit(int pageSize)
{
    int intSize = sizeof(IntType);

    std::vector<AuxVector<IntType>> auxv;

    string filename;
    if (argv.size() < 1)
        filename = "";
    else
        filename = argv[0];

    // Even for a 32 bit process, the ABI says we still need to
    // maintain double word alignment of the stack pointer.
    uint64_t align = 16;

    enum hardwareCaps
    {
        M5_HWCAP_SPARC_FLUSH = 1,
        M5_HWCAP_SPARC_STBAR = 2,
        M5_HWCAP_SPARC_SWAP = 4,
        M5_HWCAP_SPARC_MULDIV = 8,
        M5_HWCAP_SPARC_V9 = 16,
        // This one should technically only be set
        // if there is a cheetah or cheetah_plus tlb,
        // but we'll use it all the time
        M5_HWCAP_SPARC_ULTRA3 = 32
    };

    const int64_t hwcap =
        M5_HWCAP_SPARC_FLUSH |
        M5_HWCAP_SPARC_STBAR |
        M5_HWCAP_SPARC_SWAP |
        M5_HWCAP_SPARC_MULDIV |
        M5_HWCAP_SPARC_V9 |
        M5_HWCAP_SPARC_ULTRA3;

    // Setup the auxilliary vectors. These will already have endian conversion.
    // Auxilliary vectors are loaded only for elf formatted executables.
    auto *elfObject = dynamic_cast<::Loader::ElfObject *>(objFile);
    if (elfObject) {
        // Bits which describe the system hardware capabilities
        auxv.emplace_back(M5_AT_HWCAP, hwcap);
        // The system page size
        auxv.emplace_back(M5_AT_PAGESZ, SparcISA::PageBytes);
        // Defined to be 100 in the kernel source.
        // Frequency at which times() increments
        auxv.emplace_back(M5_AT_CLKTCK, 100);
        // For statically linked executables, this is the virtual address of
        // the program header tables if they appear in the executable image
        auxv.emplace_back(M5_AT_PHDR, elfObject->programHeaderTable());
        // This is the size of a program header entry from the elf file.
        auxv.emplace_back(M5_AT_PHENT, elfObject->programHeaderSize());
        // This is the number of program headers from the original elf file.
        auxv.emplace_back(M5_AT_PHNUM, elfObject->programHeaderCount());
        // This is the base address of the ELF interpreter; it should be
        // zero for static executables or contain the base address for
        // dynamic executables.
        auxv.emplace_back(M5_AT_BASE, getBias());
        // This is hardwired to 0 in the elf loading code in the kernel
        auxv.emplace_back(M5_AT_FLAGS, 0);
        // The entry point to the program
        auxv.emplace_back(M5_AT_ENTRY, objFile->entryPoint());
        // Different user and group IDs
        auxv.emplace_back(M5_AT_UID, uid());
        auxv.emplace_back(M5_AT_EUID, euid());
        auxv.emplace_back(M5_AT_GID, gid());
        auxv.emplace_back(M5_AT_EGID, egid());
        // Whether to enable "secure mode" in the executable
        auxv.emplace_back(M5_AT_SECURE, 0);
        // The address of 16 "random" bytes.
        auxv.emplace_back(M5_AT_RANDOM, 0);
    }

    // Figure out how big the initial stack needs to be

    // The unaccounted for 8 byte 0 at the top of the stack
    int sentry_size = 8;

    // This is the name of the file which is present on the initial stack
    // It's purpose is to let the user space linker examine the original file.
    int file_name_size = filename.size() + 1;

    const int numRandomBytes = 16;
    int aux_data_size = numRandomBytes;

    int env_data_size = 0;
    for (int i = 0; i < envp.size(); ++i) {
        env_data_size += envp[i].size() + 1;
    }
    int arg_data_size = 0;
    for (int i = 0; i < argv.size(); ++i) {
        arg_data_size += argv[i].size() + 1;
    }

    // The info_block.
    int base_info_block_size =
        sentry_size + file_name_size + env_data_size + arg_data_size;

    int info_block_size = roundUp(base_info_block_size, align);

    int info_block_padding = info_block_size - base_info_block_size;

    // Each auxilliary vector is two words
    int aux_array_size = intSize * 2 * (auxv.size() + 1);

    int envp_array_size = intSize * (envp.size() + 1);
    int argv_array_size = intSize * (argv.size() + 1);

    int argc_size = intSize;
    int window_save_size = intSize * 16;

    // Figure out the size of the contents of the actual initial frame
    int frame_size =
        aux_array_size +
        envp_array_size +
        argv_array_size +
        argc_size +
        window_save_size;

    // There needs to be padding after the auxiliary vector data so that the
    // very bottom of the stack is aligned properly.
    int aligned_partial_size = roundUp(frame_size, align);
    int aux_padding = aligned_partial_size - frame_size;

    int space_needed =
        info_block_size +
        aux_data_size +
        aux_padding +
        frame_size;

    memState->setStackMin(memState->getStackBase() - space_needed);
    memState->setStackMin(roundDown(memState->getStackMin(), align));
    memState->setStackSize(memState->getStackBase() - memState->getStackMin());

    // Allocate space for the stack
    memState->mapRegion(roundDown(memState->getStackMin(), pageSize),
                        roundUp(memState->getStackSize(), pageSize), "stack");

    // map out initial stack contents
    IntType sentry_base = memState->getStackBase() - sentry_size;
    IntType file_name_base = sentry_base - file_name_size;
    IntType env_data_base = file_name_base - env_data_size;
    IntType arg_data_base = env_data_base - arg_data_size;
    IntType aux_data_base = arg_data_base - info_block_padding - aux_data_size;
    IntType auxv_array_base = aux_data_base - aux_array_size - aux_padding;
    IntType envp_array_base = auxv_array_base - envp_array_size;
    IntType argv_array_base = envp_array_base - argv_array_size;
    IntType argc_base = argv_array_base - argc_size;
#if TRACING_ON
    IntType window_save_base = argc_base - window_save_size;
#endif

    DPRINTF(Stack, "The addresses of items on the initial stack:\n");
    DPRINTF(Stack, "%#x - sentry NULL\n", sentry_base);
    DPRINTF(Stack, "filename = %s\n", filename);
    DPRINTF(Stack, "%#x - file name\n", file_name_base);
    DPRINTF(Stack, "%#x - env data\n", env_data_base);
    DPRINTF(Stack, "%#x - arg data\n", arg_data_base);
    DPRINTF(Stack, "%#x - auxv array\n", auxv_array_base);
    DPRINTF(Stack, "%#x - envp array\n", envp_array_base);
    DPRINTF(Stack, "%#x - argv array\n", argv_array_base);
    DPRINTF(Stack, "%#x - argc \n", argc_base);
    DPRINTF(Stack, "%#x - window save\n", window_save_base);
    DPRINTF(Stack, "%#x - stack min\n", memState->getStackMin());

    assert(window_save_base == memState->getStackMin());

    // write contents to stack

    // figure out argc
    IntType argc = argv.size();
    IntType guestArgc = htobe(argc);

    // Write out the sentry void *
    uint64_t sentry_NULL = 0;
    initVirtMem->writeBlob(sentry_base, &sentry_NULL, sentry_size);

    // Write the file name
    initVirtMem->writeString(file_name_base, filename.c_str());

    // Fix up the aux vectors which point to data.
    for (auto &aux: auxv) {
        if (aux.type == M5_AT_RANDOM)
            aux.val = aux_data_base;
    }

    // Copy the aux stuff
    Addr auxv_array_end = auxv_array_base;
    for (const auto &aux: auxv) {
        initVirtMem->write(auxv_array_end, aux, GuestByteOrder);
        auxv_array_end += sizeof(aux);
    }

    // Write out the terminating zeroed auxilliary vector
    const AuxVector<IntType> zero(0, 0);
    initVirtMem->write(auxv_array_end, zero);
    auxv_array_end += sizeof(zero);

    copyStringArray(envp, envp_array_base, env_data_base,
                    BigEndianByteOrder, *initVirtMem);
    copyStringArray(argv, argv_array_base, arg_data_base,
                    BigEndianByteOrder, *initVirtMem);

    initVirtMem->writeBlob(argc_base, &guestArgc, intSize);

    // Set up space for the trap handlers into the processes address space.
    // Since the stack grows down and there is reserved address space abov
    // it, we can put stuff above it and stay out of the way.
    fillStart = memState->getStackBase();
    spillStart = fillStart + sizeof(MachInst) * numFillInsts;

    ThreadContext *tc = system->getThreadContext(contextIds[0]);
    // Set up the thread context to start running the process
    // assert(NumArgumentRegs >= 2);
    // tc->setIntReg(ArgumentReg[0], argc);
    // tc->setIntReg(ArgumentReg[1], argv_array_base);
    tc->setIntReg(StackPointerReg, memState->getStackMin() - StackBias);

    // %g1 is a pointer to a function that should be run at exit. Since we
    // don't have anything like that, it should be set to 0.
    tc->setIntReg(1, 0);

    tc->pcState(getStartPC());

    // Align the "stack_min" to a page boundary.
    memState->setStackMin(roundDown(memState->getStackMin(), pageSize));
}

void
Sparc64Process::argsInit(int intSize, int pageSize)
{
    SparcProcess::argsInit<uint64_t>(pageSize);

    // Stuff the trap handlers into the process address space
    initVirtMem->writeBlob(fillStart,
            fillHandler64, sizeof(MachInst) * numFillInsts);
    initVirtMem->writeBlob(spillStart,
            spillHandler64, sizeof(MachInst) *  numSpillInsts);
}

void
Sparc32Process::argsInit(int intSize, int pageSize)
{
    SparcProcess::argsInit<uint32_t>(pageSize);

    // Stuff the trap handlers into the process address space
    initVirtMem->writeBlob(fillStart,
            fillHandler32, sizeof(MachInst) * numFillInsts);
    initVirtMem->writeBlob(spillStart,
            spillHandler32, sizeof(MachInst) *  numSpillInsts);
}

void Sparc32Process::flushWindows(ThreadContext *tc)
{
    RegVal Cansave = tc->readIntReg(INTREG_CANSAVE);
    RegVal Canrestore = tc->readIntReg(INTREG_CANRESTORE);
    RegVal Otherwin = tc->readIntReg(INTREG_OTHERWIN);
    RegVal CWP = tc->readMiscReg(MISCREG_CWP);
    RegVal origCWP = CWP;
    CWP = (CWP + Cansave + 2) % NWindows;
    while (NWindows - 2 - Cansave != 0) {
        if (Otherwin) {
            panic("Otherwin non-zero.\n");
        } else {
            tc->setMiscReg(MISCREG_CWP, CWP);
            // Do the stores
            RegVal sp = tc->readIntReg(StackPointerReg);
            for (int index = 16; index < 32; index++) {
                uint32_t regVal = tc->readIntReg(index);
                regVal = htobe(regVal);
                if (!tc->getVirtProxy().tryWriteBlob(
                        sp + (index - 16) * 4, (uint8_t *)&regVal, 4)) {
                    warn("Failed to save register to the stack when "
                            "flushing windows.\n");
                }
            }
            Canrestore--;
            Cansave++;
            CWP = (CWP + 1) % NWindows;
        }
    }
    tc->setIntReg(INTREG_CANSAVE, Cansave);
    tc->setIntReg(INTREG_CANRESTORE, Canrestore);
    tc->setMiscReg(MISCREG_CWP, origCWP);
}

void
Sparc64Process::flushWindows(ThreadContext *tc)
{
    RegVal Cansave = tc->readIntReg(INTREG_CANSAVE);
    RegVal Canrestore = tc->readIntReg(INTREG_CANRESTORE);
    RegVal Otherwin = tc->readIntReg(INTREG_OTHERWIN);
    RegVal CWP = tc->readMiscReg(MISCREG_CWP);
    RegVal origCWP = CWP;
    CWP = (CWP + Cansave + 2) % NWindows;
    while (NWindows - 2 - Cansave != 0) {
        if (Otherwin) {
            panic("Otherwin non-zero.\n");
        } else {
            tc->setMiscReg(MISCREG_CWP, CWP);
            // Do the stores
            RegVal sp = tc->readIntReg(StackPointerReg);
            for (int index = 16; index < 32; index++) {
                RegVal regVal = tc->readIntReg(index);
                regVal = htobe(regVal);
                if (!tc->getVirtProxy().tryWriteBlob(
                        sp + 2047 + (index - 16) * 8, (uint8_t *)&regVal, 8)) {
                    warn("Failed to save register to the stack when "
                            "flushing windows.\n");
                }
            }
            Canrestore--;
            Cansave++;
            CWP = (CWP + 1) % NWindows;
        }
    }
    tc->setIntReg(INTREG_CANSAVE, Cansave);
    tc->setIntReg(INTREG_CANRESTORE, Canrestore);
    tc->setMiscReg(MISCREG_CWP, origCWP);
}
