
UAE CPU Tester

I finally wrote utility (This was my "Summer 2019" project) that can be used to verify operation of for example software emulated or FPGA 680x0 CPUs.
It is based on UAE CPU core (gencpu generated special test core). All the CPU logic comes from UAE CPU core.

Verifies:

- All CPU registers (D0-D7/A0-A7, PC and SR/CCR)
- All FPU registers (FP0-FP7, FPIAR, FPCR, FPSR)
- Generated exception and stack frame contents (if any)
- Memory writes, including stack modifications (if any)
- Loop mode for JIT testing. (generates <test instruction>, dbf dn,loop)
- Supports 68000, 68010, 68020, 68030 (only difference between 020 and 030 seems to be data cache and MMU), 68040 and 68060.

Tests executed for each tested instruction:

- Every CCR combination or optionally only all zeros, all ones CCR (32 or 2 tests)
- Every FPU condition combination (4 bits) + 2 precision bits + 2 rounding bits (256 tests)
- Every addressing mode, including optionally 68020+ addressing modes.
- If instruction generated privilege violation exception, extra test round is run in supervisor mode.
- Optionally can do any combination of T0, T1, S and M -bit SR register extra test rounds.
- Every opcode value is tested. Total number of tests per opcode depends on available addressing modes etc. It can be hundreds of thousands or even millions..

Test generation details:

Instruction's effective address is randomized. It is accepted if it points to any of 3 test memory regions. If it points outside of test memory, it will be re-randomized few times. Test will be skipped if current EA makes it impossible to point to any of 3 test regions.
If 68000/68010 and address error testing is enabled: 2 extra test rounds are generated, one with even and another with odd EAs to test and verify address errors.

Notes and limitations:

- Test generator is very brute force based, it should be more intelligent.. Now has optional target src/dst/opcode modes for better bus/address error testing.
- Bus and address error testing is optional, if disabled, generated tests never cause bus/address errors.
- RTE test only tests stack frame types 0 and 2 (if 68020+)
- All tests that would halt or reset the CPU are skipped (RESET in supervisor mode, STOP parameter that would stop the CPU etc)
- Single instruction test set will take long time to run on real 68000. Few minutes to much longer...
- Undefined flags (for example DIV and CHK or 68000/010 bus address error) are also verified. It probably would be good idea to optionally filter them out.
- Instruction cycle order or timing is ignored. It is not possible without extra hardware.
- FPU testing is not yet fully implemented.
- Sometimes reported old and new condition code state does not match error report..

Tester compatibility (integer instructions only):

68000: Complete. Including bus and address error stack frame/register/CCR modification undocumented behavior.
68010: Partially supported.
68020: Almost complete (DIV undocumented behavior is not yet known)
68030: Same as 68020.
68040: Almost complete (Weird unaligned MOVE16 behavior which may be board specific).
68060: Same as 68040.

More CPU details in WinUAE changelog.

Not implemented or only partially implemented:

68010+:

- MOVEC: Most control registers tested: Write all ones, write all zeros, read it back, restore original value. 68040+ TC/TT registers enable bit is always zero.
- RTE: long frames with undefined fields are skipped. Basic frame types 0 and 2 are verified, also unsupported frame types are tested, error is reported if CPU does not generate frame exception.
- 68020+ undefined addressing mode bit combinations are not tested.

All models:

- Interrupts (stack frames and STOP)
- MMU instructions (Not going to happen)
- 68020+ cache related instructions.
- FPU FSAVE/FRESTORE, FPU support also isn't fully implemented yet.

Build instructions:

- buildm68k first (already built if UAE core was previously compiled)
- gencpu with CPU_TEST=1 define. This creates cpuemu_x_test.cpp files (x=90-94), cpustbl_test.cpp and cputbl_test.h
- build cputestgen project.
- build native Amiga project (cputest directory). Assembly files probably only compiles with Bebbo's GCC.


Test generator quick instructions:

Update cputestgen.ini to match your CPU model, memory settings etc.

"Low memory" = memory accessible using absolute word addressing mode, positive value (0x0000 to 0x7fff). Can be larger.
"High memory" = memory accessible using absolute word addressing mode, negative value (0xFFF8000 to 0xFFFFFFFF)

If high memory is ROM space (like on 24-bit address space Amigas), memory region is used for read only tests, use "high_rom=<path to rom image>" to select ROM image. Last 32k of image is loaded.

Use "test_low_memory_start"/"test_high_memory_start" and "test_low_memory_end"/"test_high_memory_end" to restrict range of memory region used for tests, for example if part of region is normally inaccessible.

"test_memory_start"/"test_memory_size" is the main test memory, tested instruction and stack is located here. Must be at least 128k but larger the size, the easier it is for the generator to find effective addresses that hit test memory. This memory space must be free on test target m68k hardware.

All 3 memory regions (if RAM) are filled with pseudo-random pattern and saved as "lmem.dat", "hmem.dat" and "tmem.dat"

Use feature_target_src_ea/feature_target_dst_ea=<one or more addresses separated by a comman> if you want generate test set that only uses listed addresses (of course instructions that can have memory source or destination EA are used). Useful for bus and address errors.

Usage of Amiga m68k native test program:

Copy all memory dat files, test executable compiled for target platform (currently only Amiga is supported) and data/<cpu model> contents to target system, keeping original directory structure.

cputest all = run all tests, in alphabetical order. Stops when mismatch is detected.
cputest tst.b = run tst.b tests only
cputest all tst.b = run tst.b, then tst.w and so on in alphabetical order until end or mismatch is detected.

If mismatch is detected, opcode word(s), instruction disassembly, registers before and after and reason message is shown on screen. If difference is in exception stack frame, both expected and returned stack frame is shown in hexadecimal.
