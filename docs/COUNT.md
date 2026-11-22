# The call counters (SH2O_COUNT)

This guide tells what the call counters count, how to build them, how a consumer reads them for a census, and how a new entry adds its counter.

**Status: off by default.** The CMake option `SH2O_COUNT` (default `OFF`) adds the counters. Without the option, sh2o has no counter and no counting code: each SH-2 object, the archive and each lane ELF are the same bytes as before the counters. `make cross` proves this against `tests/sh2/objects.sha256`.

## Build with the option

```sh
make test SH2O_COUNT=ON           # build/<cc>-count (with SH2O_ZAM=ON: build/<cc>-zam-count)
make cross SH2O_COUNT=ON          # build/sh2-count-native, build/sh2-count-portable
```

The option defines `SH2O_COUNT=1` for `sh2o` as PRIVATE. `make cross SH2O_COUNT=ON` builds the library and the suites for the SH-2 and checks that they need no C library function. It does not do the static checks, the lane ELFs or the manifest check, because these apply to the build without the counters.

The header `sh2o/count.h` is installed in all builds, but only a library built with the option defines its functions. If a program calls them with a library built without the option, the link fails.

## What is counted

`sh2o/count.h` gives `SH2O_COUNT_N` counters. Each counter is a `uint32_t` with an index and a name:

| Counters | Index | Names | Counts |
|---|---|---|---|
| Commands by opcode | `SH2O_COUNT_OP` + op, 64 slots | `op_00` to `op_3f` | Each GTE command that runs, on every route: a direct call of a command entry, `sh2o_gte_command`, an item of `sh2o_gte_mvmva_list`, and a command in a PSY-Q body. `sh2o_gte_command` also counts the opcode of a command that it returns -1 for. A command entry counts its own opcode. |
| Command entries | `SH2O_COUNT_GTE_COMMAND`, `SH2O_COUNT_GTE_MVMVA` to `SH2O_COUNT_GTE_PREPARED_LOAD` | the function names | A call from outside sh2o only. |
| PSY-Q bodies | `SH2O_COUNT_PSYQ_SETROTMATRIX` to `SH2O_COUNT_PSYQ_RATAN2` (19) | the function names | A call from outside sh2o only. |
| The MVMVA list | `SH2O_COUNT_LIST_CALLS`, `SH2O_COUNT_LIST_ITEMS` | `sh2o_gte_mvmva_list`, `list_items` | Calls from outside sh2o; n items for each call. |
| The prepared MVMVA list | `SH2O_COUNT_LIST_PREPARED_CALLS` | `sh2o_gte_mvmva_list_prepared` | Calls from outside sh2o. Each call also adds n to `list_items` and to `op_12`, on the kernel path and on the fallback, and nothing to `sh2o_gte_mvmva_list` or `sh2o_gte_prepared_load`. |
| Register writes by group | `SH2O_COUNT_WRITE_RT`, `_LLM`, `_LCM`, `_TR`, `_BK`, `_FC`, `_OTHER` | `write_rt`, `write_llm`, `write_lcm`, `write_tr`, `write_bk`, `write_fc`, `write_other` | Each `sh2o_gte_write` and each register that a PSY-Q body writes directly into the image, on every route, by register: rotation 32-36, light 40-44, colour 48-52, translation 37-39, back colour 45-47, far colour 53-55, and all other registers. |

"From outside sh2o" means that sh2o does not count its own calls of its entries. For example, `sh2o_psyq_ApplyMatrix` adds 1 to its own counter, 1 to `op_12` and 7 to the write counters, but nothing to `sh2o_gte_command`. A depth word does this: sh2o increases it while it calls its own entries.

The functions:

- `uint32_t sh2o_count_get(unsigned i)`: the value of counter i, 0 when i >= `SH2O_COUNT_N`.
- `const char *sh2o_count_name(unsigned i)`: the name of counter i, NULL when i >= `SH2O_COUNT_N`.
- `void sh2o_count_reset(void)`: sets all counters to 0.

The indices can change when a counter is added. A census reads the counters by name, or uses the constants of the header it compiles with.

## A census

A census counts the calls of each frame. At the end of each frame, it reads all counters and then resets them:

```c
for(unsigned i = 0; i < SH2O_COUNT_N; i++)
    if(sh2o_count_get(i))
        record(frame,sh2o_count_name(i),sh2o_count_get(i));
sh2o_count_reset();
```

The MVMVA calls for each load of the rotation matrix: `op_12` divided by (`write_rt` / 5), because one full load writes the five registers 32-36. Use the census build only to count. The counters add a write to memory at each count point, so do not use cycle figures from this build.

## A second CPU

The counters are one array of plain `uint32_t` values and one depth word, with no lock. If two CPUs (for example the two SH-2 CPUs of the Saturn) call sh2o at the same time, these problems occur:

- Each CPU reads and writes the same counter, so an increment can be lost.
- One CPU can see the depth word of the other CPU. Then a call from outside sh2o is not counted by name, or a call from inside sh2o is counted by name.
- Each SH-2 has its own cache, so a CPU can read old values unless it reads through the cache-through address range.

Count on one CPU only. A census on the host has one CPU.

## Add a counter for a new entry

A new command entry, PSY-Q body or list (for example `sh2o_gte_nrtps`) adds its counter in the same commit:

1. `include/sh2o/count.h`: one constant at the end of its group (before the next group's first constant).
2. `src/sh2o.c`: its name in the name table, at the same position.
3. Its body: one count line from `src/sh2o_count_internal.h`. A command entry uses `SH2OI_COUNT_CMD(index, op)`, a body uses `SH2OI_COUNT_NAME(index)`. If the body calls other sh2o entries, it puts `SH2OI_COUNT_IN()` and `SH2OI_COUNT_OUT()` around these calls. Each macro is `((void)0)` without the option.
4. `tests/test_count.c`: a call on the new route, and the expected values for its counters.
5. This guide: its row in the table, if it adds a group.

Then `make cross` must pass with the manifest unchanged, and `make test SH2O_COUNT=ON` with gcc and clang.
