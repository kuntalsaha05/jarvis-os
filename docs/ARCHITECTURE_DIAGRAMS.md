# JARVIS-OS — System Architecture Diagrams

This document contains editable, source-controlled Mermaid diagrams representing the exact architectural implementation of JARVIS-OS across its bare-metal x86-64 kernel and host-side AI assistant companion.

---

## 1. Overall System Architecture

The following diagram illustrates the relationship between the bare-metal x86-64 kernel environment (executing inside QEMU / hardware) and the host-side local AI companion service, bridged via serial line transport (COM1 / `0x3F8`):

```mermaid
flowchart TD
    subgraph HostSide["Host Workstation Environment (Python 3.12)"]
        CLI["Host CLI / REPL (jarvis/main.py)"]
        Agent["Autonomous Agent (JarvisAgent 8-Step Loop)"]
        LLM_Mgr["LLM Manager Facade (LLMManager)"]
        subgraph Backends["Replaceable LLM Backends (LLMInterface)"]
            Mock["MockLLMBackend\n(Deterministic CI)"]
            Llama["LlamaCppBackend\n(Local Quantized GGUF)"]
            AirLLM["AirLLMBackend\n(Low-VRAM Streaming)"]
        end
        Registry["Tool Registry (ToolRegistry)\n4-Tier Security & 10s Timeouts"]
        SerialTransport["Serial Transport Bridge (SerialBridgeTransport)\nProtocol v1.0 JSON Line-Delimited"]
    end

    subgraph Communication["Hardware / Virtual Communication Layer"]
        COM1["COM1 Serial Line (0x3F8) / Virtual PTY\nBaud: 115200, UTF-8 Delimited JSON"]
    end

    subgraph KernelSpace["Bare-Metal x86-64 Kernel (Ring 0)"]
        subgraph Bootloader["Boot & Memory Initialization"]
            GRUB["GRUB / Multiboot (0x1BADB002)"]
            PML4["4-Level Paging (boot.S)\nIdentity Mapped 2 MiB Huge Page"]
            EarlyDebug["Early Serial Debug (debug.c)"]
        end

        subgraph CoreSubsystems["Core Kernel Subsystems"]
            IDT["IDT (256 Gates) & 8259 PIC Remap"]
            PIT["PIT Timer 100 Hz (IRQ0)"]
            KBD["PS/2 Keyboard Driver & Buffer (IRQ1)"]
            ProcessMgr["Process & Thread Manager (TCBs)"]
            Scheduler["Preemptive Scheduler (Round Robin, 5-tick)"]
            MemAlloc["Physical Page Allocator (64 frames) & Heap"]
            Syscall["System Call Dispatcher (int $0x80)"]
        end

        subgraph StorageLayer["Storage & Filesystem"]
            BlockDev["Block Device Abstraction (block.c)"]
            RAMDisk["In-Memory RAMDisk (64 KiB, 128 sectors)"]
            JarvisFS["JARVIS-FS Inode Filesystem (0x4A415256)"]
        end

        subgraph AlgSim["Educational Algorithm Simulators"]
            SchedSim["CPU Sched (FCFS, SJF, RR)"]
            PageReplace["Paging (FIFO, LRU, Optimal, Belady)"]
            Banker["Banker's Avoidance & Deadlock Detection"]
            DiskSched["Disk Head (FCFS, SSTF, SCAN, C-SCAN)"]
            MemFit["Memory Fit (First, Best, Worst)"]
        end

        Shell["Interactive Kernel Shell (shell.c)\n32 Built-in Commands + 'ai' Status"]
    end

    CLI --> Agent
    Agent --> LLM_Mgr
    Agent --> Registry
    LLM_Mgr --> Mock
    LLM_Mgr --> Llama
    LLM_Mgr --> AirLLM
    Registry --> SerialTransport
    SerialTransport <--> COM1
    COM1 <--> CoreSubsystems
    GRUB --> PML4 --> EarlyDebug --> IDT
    IDT --> PIT
    IDT --> KBD
    IDT --> Syscall
    PIT --> Scheduler
    Scheduler --> ProcessMgr
    CoreSubsystems --> Shell
    StorageLayer --> Shell
    AlgSim --> Shell
```

---

## 2. Boot and Interrupt Subsystem Flow

This sequence diagram depicts the boot execution path from BIOS/GRUB hand-off to the unmasking of hardware interrupts and shell launch:

```mermaid
sequenceDiagram
    autonumber
    participant BIOS as BIOS / GRUB
    participant Boot as boot.S (Multiboot)
    participant Kernel as kernel.c (kernel_main)
    participant IDT as idt.c (IDT & PIC)
    participant PIT as pit.c (100 Hz)
    participant Process as process.c / scheduler.c
    participant KBD as keyboard.c (PS/2)
    participant Shell as shell.c (shell_run)

    BIOS->>Boot: Multiboot Hand-off (%eax=0x2BADB002, %ebx=mbi)
    Note over Boot: Setup 4-level paging (PML4, PDPT, PD 2MB huge page)<br/>Load 64-bit GDT, enable Long Mode (EFER.LME=1, CR0.PG=1)
    Boot->>Kernel: Call kernel_main(magic, addr)
    Kernel->>Kernel: Early serial debug init on COM1 & port 0xE9
    Kernel->>Kernel: Parse Multiboot memory map (RAM discovery)
    Kernel->>IDT: idt_init() [Populate 256 gates with split 64-bit offsets]
    Kernel->>IDT: pic_init() [Remap Master 0x20-0x27, Slave 0x28-0x2F]
    Kernel->>PIT: pit_init(100) [Configure Channel 0 Mode 3 at 100 Hz]
    Kernel->>Process: process_init() & scheduler_init(RR, quantum=5)
    Note over Kernel,Process: Execute context-switch, stack-integrity, and<br/>involuntary PIT preemption self-tests
    Kernel->>KBD: keyboard_init() [Unmask PIC IRQ1, enable PS/2 interrupt]
    Kernel->>Kernel: Enable interrupts (sti)
    Kernel->>Shell: shell_run() [Dual VGA + Serial interactive prompt]
```

---

## 3. Process Scheduling & Preemptive Context Switching

The context-switching mechanism relies on a 20-quadword stack frame restoring all 15 general-purpose registers and executing an architectural `iretq`:

```mermaid
flowchart TD
    subgraph TimerTrigger["PIT Hardware Interrupt (IRQ0)"]
        PIT_IRQ["PIT fires at 100 Hz\n(Vector 32 / IDT Gate 0x20)"]
        CPU_Save["CPU pushes SS, RSP, RFLAGS, CS, RIP\nonto current thread kernel stack"]
    end

    subgraph AssemblyStub["kernel/interrupts/interrupts.S (timer_stub)"]
        SaveGPRs["SAVE_REGISTERS macro pushes:\nRAX, RBX, RCX, RDX, RBP, RSI, RDI,\nR8, R9, R10, R11, R12, R13, R14, R15\n(Total: 20 quadwords on stack)"]
        CallSched["movq %rsp, %rdi\ncall scheduler_interrupt(current_rsp, is_preempt=1)"]
    end

    subgraph SchedulerDecision["kernel/process/scheduler.c (scheduler_interrupt)"]
        PicEOI["outb(0x20, 0x20) (Master PIC EOI)"]
        TickCount["timer_ticks++\nCheck sleeping tasks (wake_tick)"]
        PreemptCheck{"Running task quantum\nexpired? (ticks >= 5)"}
        YieldTask["Save current_process->saved_rsp\nChange state RUNNING -> READY\nPick next READY task (Round Robin)"]
        KeepTask["Remain in current task"]
        ReturnRSP["Return next_task->saved_rsp in %rax"]
    end

    subgraph RestoreContext["kernel/interrupts/interrupts.S (timer_stub resume)"]
        SwitchStack["movq %rax, %rsp (Switch to next task stack)"]
        PopGPRs["RESTORE_REGISTERS macro pops R15..RAX"]
        IRETQ["iretq (Pops RIP, CS, RFLAGS, RSP, SS)\nExecution resumes in next process"]
    end

    PIT_IRQ --> CPU_Save --> SaveGPRs --> CallSched
    CallSched --> PicEOI --> TickCount --> PreemptCheck
    PreemptCheck -- "Yes (Quantum=5)" --> YieldTask --> ReturnRSP
    PreemptCheck -- "No" --> KeepTask --> ReturnRSP
    ReturnRSP --> SwitchStack --> PopGPRs --> IRETQ
```

---

## 4. Memory Management & Page-Fault Architecture

The memory subsystem cleanly separates physical bitmap frame allocation from identity-mapped virtual paging and safe fault diagnostics:

```mermaid
flowchart TD
    subgraph PhysicalSpace["Physical Memory (Managed via kernel/memory/page.c)"]
        RAM_Start["0x00000000: Bootloader / BIOS Vector Table"]
        KernelBin["0x00100000: 1 MiB Load Address (kernel.bin)"]
        PagePoolBase["0x00120000: 64-Frame Physical Page Pool (256 KiB)"]
        Bitmap["Bitmap Allocator (uint64_t bitmap, 64 bits = 64 frames)\npage_alloc() -> 4096-byte aligned frame\npage_free() -> validates pool range, rejects double-free"]
    end

    subgraph VirtualSpace["x86-64 Virtual Address Space (kernel/arch/x86_64/boot.S)"]
        IdentityMapped["0x00000000 - 0x001FFFFF (2 MiB)\nIdentity Mapped via 2 MiB Huge Page (PDE)\nPML4[0] -> PDPT[0] -> PD[0] (PS=1, RW=1, P=1)"]
        UnmappedSpace[">= 0x00200000 (>= 2 MiB)\nUnmapped Virtual Memory Space"]
    end

    subgraph FaultHandling["Page Fault Architecture (IDT Vector 14)"]
        CPU_Fault["Memory access to unmapped address (e.g. >= 2 MiB)"]
        CPU_Push["CPU records faulting virtual address in CR2\nPushes error code + interrupt frame"]
        PF_Stub["interrupts.S (page_fault_stub)\nSaves 15 GPRs, passes %cr2 in %rdi and error code in %rsi"]
        PF_Handler["idt.c (page_fault_handler)\nDecodes error code bits: Present, Write, User, Reserved, Instruction\nChecks if faulting address >= 2 MiB identity boundary\nDisplays diagnostic without triple-faulting"]
    end

    CPU_Fault --> CPU_Push --> PF_Stub --> PF_Handler
    Bitmap --> PagePoolBase
    VirtualSpace -.-> PhysicalSpace
```

---

## 5. IPC Multi-Queue & Synchronization Primitives

The IPC subsystem implements multi-queue message routing with boundary protection, while synchronization provides counting semaphores and mutexes with ownership tracking:

```mermaid
flowchart TD
    subgraph IPC_Subsystem["IPC Multi-Queue Subsystem (kernel/ipc/ipc.c)"]
        Q0["Queue 0 (PID 1 Inbox)\nCapacity: 16 msgs"]
        Q1["Queue 1 (PID 2 Inbox)\nCapacity: 16 msgs"]
        Q2["Queue 2 (General)\nCapacity: 16 msgs"]
        Q3["Queue 3 (Kernel)\nCapacity: 16 msgs"]
        
        SendOp["ipc_send(qid, msg)\n1. Validate qid < 4 and len <= 64\n2. Check circular count == 16 -> return IPC_ERR_FULL (-1)\n3. Enqueue msg (type, sender_pid, data)\n4. If receiver blocked, unblock receiver"]
        RecvOp["ipc_recv(qid, msg_buf, timeout_ticks)\n1. If count > 0 -> dequeue msg, return IPC_OK\n2. If empty & timeout==0 -> return IPC_ERR_EMPTY (-2)\n3. If empty & timeout>0 -> process_block() with wake_tick\n4. If timer expires -> return IPC_ERR_TIMEOUT (-3)"]
    end

    subgraph Sync_Primitives["Kernel Synchronization (kernel/ipc/semaphore.c, sync_demo.c)"]
        subgraph Mutex["Mutex (mutex_t)"]
            M_Lock["mutex_lock(&m)\nAtomic test-and-set\nRecords owner_pid"]
            M_Recurse["Recursive lock attempt\nowner_pid == current_pid -> MUTEX_ERR_DEADLOCK (-1)"]
            M_Unlock["mutex_unlock(&m)\nValidates owner_pid == current_pid\nNon-owner unlock -> MUTEX_ERR_NOT_OWNER (-2)"]
        end

        subgraph Semaphore["Counting Semaphore (semaphore_t)"]
            S_Wait["semaphore_wait(&s)\nAtomic count decrement\nIf count < 0: add PID to wait_queue (max 8) & block"]
            S_Post["semaphore_post(&s)\nAtomic count increment\nIf waiters exist: dequeue PID & process_unblock()"]
        end
    end

    SendOp --> Q0
    Q0 --> RecvOp
```

---

## 6. Storage & Resilient Filesystem (JARVIS-FS)

JARVIS-FS provides a hardened inode-based filesystem built on top of a clean block-device abstraction:

```mermaid
flowchart TD
    subgraph BlockAbstraction["Block Device Layer (kernel/storage/block.c)"]
        BlockInterface["block_read(lba, count, buf)\nblock_write(lba, count, buf)\n- Checks device ready (BLOCK_ERR_NOT_READY)\n- Validates non-null buffer (BLOCK_ERR_INVALID_PARAM)\n- Validates LBA range and integer overflow (BLOCK_ERR_OUT_OF_BOUNDS)"]
        RAMDiskDriver["kernel/storage/ramdisk.c (RAMDisk Driver)\n128 sectors * 512 bytes = 65,536 bytes (64 KiB capacity)"]
    end

    subgraph JARVIS_FS_Geometry["JARVIS-FS Disk Geometry (64 KiB = 128 Blocks)"]
        B0["Block 0: Superblock (16 bytes)\nMagic: 0x4A415256 ('JARV')\nTotal Blocks: 128 | Free Data Blocks: 120 | Inodes: 16"]
        B1_2["Blocks 1-2: Inode Table (16 Inodes * 68 bytes)\nFields: filename[28], size, direct_blocks[8], is_used"]
        B3["Block 3: Block Allocation Bitmap (512 bytes)\nBits 0..119 track free/allocated state of data blocks 4..123"]
        B4_123["Blocks 4-123: Data Blocks (120 Blocks * 512 bytes = 60 KiB payload)"]
    end

    subgraph FS_Operations["Filesystem Operations (kernel/storage/fs.c)"]
        Op_Create["fs_create(name)\nAllocates free inode slot, marks used"]
        Op_Write["fs_write(fd, buf, len)\nAllocates data blocks via bitmap\nAtomic rollback: clears newly allocated blocks if disk runs out!"]
        Op_Trunc["fs_truncate(fd, new_len)\nFrees unneeded blocks or expands capacity"]
        Op_Delete["fs_delete(name)\nFrees all 8 direct blocks in bitmap, clears inode, invalidates open fds"]
    end

    BlockInterface --> RAMDiskDriver
    RAMDiskDriver --> B0
    RAMDiskDriver --> B1_2
    RAMDiskDriver --> B3
    RAMDiskDriver --> B4_123
    Op_Create --> B1_2
    Op_Write --> B3
    Op_Write --> B4_123
    Op_Trunc --> B3
    Op_Delete --> B1_2
    Op_Delete --> B3
```

---

## 7. AI Companion, Tool Registry & Eight-Step Agent Loop

The host-side AI assistant executes an 8-step agentic cycle mediated by a 4-tier security allowlist:

```mermaid
sequenceDiagram
    autonumber
    actor User as User / Host CLI
    participant Agent as JarvisAgent (jarvis/agent/agent.py)
    participant Registry as ToolRegistry (jarvis/tools/registry.py)
    participant LLM as LLMManager / Active Backend
    participant Bridge as SerialBridgeTransport (Protocol v1.0)
    participant Guest as Bare-Metal Kernel Shell

    User->>Agent: Step 1: Submit user query (e.g. "What is memory usage?")
    Agent->>Agent: Step 2: Build system context (active tools, metadata)
    Agent->>Agent: Step 3: Determine required tool ("memory.get_stats")
    Agent->>Registry: Step 4: Validate tool name & argument schema
    alt Tool is PRIVILEGED or DESTRUCTIVE
        Agent->>User: Step 5: Prompt user for administrative approval
        User-->>Agent: User responds: [y/N]
    end
    Agent->>Registry: Step 6: Dispatch tool execution (10.0s deadline)
    alt Live Serial Mode Connected
        Registry->>Bridge: Send BridgeRequest JSON line (action: tool_call)
        Bridge->>Guest: Transmit over COM1 (0x3F8)
        Guest-->>Bridge: Return BridgeResponse JSON line
        Bridge-->>Registry: Return response (telemetry_source: LIVE_KERNEL)
    else Standalone / Offline Simulation
        Registry-->>Agent: Step 7: Return tool telemetry (telemetry_source: OFFLINE_SIMULATION)
    end
    Registry->>Registry: Record sub-millisecond audit trail log
    Agent->>LLM: Step 8: Synthesize final prompt with context & data provenance
    LLM-->>Agent: Generate AI response text
    Agent-->>User: Deliver final response disclosing telemetry source truthfully
```

