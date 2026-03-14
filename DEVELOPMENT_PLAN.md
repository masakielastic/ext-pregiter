# ext-pregiter Development Plan

## 1. Goal and Constraints

`SPEC.md` asks for an external extension prototype that provides a lazy iterator API for PCRE matches instead of the eager array materialization done by `preg_match_all()` ([SPEC.md](/home/masakielastic/develop/ext-pregiter/SPEC.md#L5)). The hard constraints are:

- buildable via `phpize && ./configure && make`
- no dependency on `ext/pcre` private/static helpers
- careful `zval` ownership and `zend_object` lifecycle
- correct handling of zero-length matches
- `rewind()` must rescan from the beginning

The provided `php_pcre_match_impl.c` is useful as behavioral reference, especially for flag handling and zero-length progress rules ([php_pcre_match_impl.c](/home/masakielastic/develop/ext-pregiter/php_pcre_match_impl.c#L36), [php_pcre_match_impl.c](/home/masakielastic/develop/ext-pregiter/php_pcre_match_impl.c#L229)). The provided `php_pcre.h` shows which `ext/pcre` entry points are actually exported as `PHPAPI` and therefore usable from an external extension ([php_pcre.h](/home/masakielastic/develop/ext-pregiter/php_pcre.h#L49), [php_pcre.h](/home/masakielastic/develop/ext-pregiter/php_pcre.h#L64)).

## 2. Chosen Strategy

### 2.1 API shape

Prototype with:

- function: `preg_iter_ex(string $pattern, string $subject, int $flags = 0): Iterator`
- internal class: `PregIterator`

Reason for `preg_iter_ex()`: avoid collision with a possible future core function named `preg_iter()`. The internal class can remain `PregIterator`, because the class name is scoped to this extension and is less likely to conflict with an eventual core procedural API.

### 2.2 Matching backend

Use public `ext/pcre` API plus direct `pcre2_*` calls:

- `pcre_get_compiled_regex_cache()` to compile/cache the pattern
- `php_pcre_pce_re()` to obtain `pcre2_code *`
- `php_pcre_pce_incref()` / `php_pcre_pce_decref()` for cache entry lifetime
- `php_pcre_create_match_data()` / `php_pcre_free_match_data()` for `match_data`
- `php_pcre_mctx()` for match context
- `pcre2_pattern_info()` and `pcre2_get_ovector_pointer()` for capture metadata and match spans

Do not build the iterator on top of `php_pcre_match_impl()` itself. It is exported, but it is optimized for `preg_match()`/`preg_match_all()` return-value construction and hides too much iterator state. In particular, lazy progression, `rewind()`, and zero-length retry logic are cleaner if the extension owns the loop.

### 2.3 Behavioral target

Match shaping should mimic `preg_match()` output for a single match:

- numeric keys `0..n`
- named captures duplicated under string keys
- `PREG_OFFSET_CAPTURE` supported
- `PREG_UNMATCHED_AS_NULL` supported
- `PREG_PATTERN_ORDER` / `PREG_SET_ORDER` rejected as unsupported for this prototype

## 3. Reference-Derived Requirements

The following parts of `php_pcre_match_impl()` should be mirrored intentionally:

- invalid flag rejection before matching starts ([php_pcre_match_impl.c](/home/masakielastic/develop/ext-pregiter/php_pcre_match_impl.c#L36))
- zero-length retry rule using `PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED` ([php_pcre_match_impl.c](/home/masakielastic/develop/ext-pregiter/php_pcre_match_impl.c#L232))
- advancing by one logical subject unit when the retry yields `PCRE2_ERROR_NOMATCH` ([php_pcre_match_impl.c](/home/masakielastic/develop/ext-pregiter/php_pcre_match_impl.c#L245))
- distinguishing normal no-match from internal PCRE execution error ([php_pcre_match_impl.c](/home/masakielastic/develop/ext-pregiter/php_pcre_match_impl.c#L261))

The following internal helpers must not be reused and need local replacements:

- `ensure_subpats_table()`
- `populate_subpat_array()`
- `populate_match_value()`
- `add_offset_pair()`
- internal access to `pce->capture_count`, `pce->name_count`, `pce->compile_options`

Those will be replaced with `pcre2_pattern_info()` queries and extension-local array builders.

## 4. Planned Directory Layout

```text
config.m4
php_pregiter.h
pregiter.c
pregiter_iterator.h
pregiter_iterator.c
pregiter.stub.php
pregiter_arginfo.h
tests/
  001_basic_iteration.phpt
  002_named_captures.phpt
  003_offset_capture.phpt
  004_unmatched_as_null.phpt
  005_zero_length_progress.phpt
  006_rewind.phpt
  007_invalid_flags.phpt
  008_pattern_error.phpt
```

Notes:

- `pregiter.stub.php` is the source of truth for user-visible signatures.
- `pregiter_arginfo.h` should be committed, not generated at build time, because `phpize` builds do not always ship the stub generator.
- Split the iterator object into its own translation unit so lifecycle and matching logic remain readable.

## 5. Internal Design

### 5.1 `PregIterator` object state

Suggested object layout:

```c
typedef struct _pregiter_object {
    zend_object std;
    zend_string *pattern;
    zend_string *subject;
    zend_long flags;

    pcre_cache_entry *pce;
    pcre2_code *re;
    pcre2_match_data *match_data;

    uint32_t capture_count;
    uint32_t name_count;
    uint32_t compile_options;

    PCRE2_SIZE cursor;
    PCRE2_SIZE next_cursor;
    zend_ulong current_key;

    zval current_match;

    bool initialized;
    bool valid;
    bool finished;
} pregiter_object;
```

Design choices:

- keep `pattern` and `subject` as owned `zend_string *`
- keep one compiled regex cache entry per iterator instance
- keep one reusable `match_data` buffer per iterator instance
- store `compile_options` from `pcre2_pattern_info()` once, because zero-length progression needs UTF awareness
- keep `current_match` as a persistent `zval` array that is destroyed/rebuilt on each successful step

### 5.2 Helper responsibilities

`pregiter_create_object()`

- allocate object
- initialize `current_match` to `UNDEF`

`pregiter_free_object()`

- `zval_ptr_dtor(&current_match)` if defined
- `php_pcre_free_match_data(match_data)`
- `php_pcre_pce_decref(pce)`
- release `pattern` and `subject`
- call standard object destructor

`pregiter_reset_state()`

- reset cursor to `0`
- reset key to `0`
- clear `current_match`
- clear `initialized`, `valid`, `finished`

`pregiter_fetch_next()`

- run one PCRE execution from `cursor`
- build `current_match`
- update `next_cursor`
- set `valid` / `finished`

`pregiter_build_match_array()`

- numeric captures
- named captures
- offset pairs when requested
- `NULL` or empty string for unmatched captures depending on `PREG_UNMATCHED_AS_NULL`

`pregiter_advance_unit()`

- advance one byte in non-UTF mode
- advance one UTF-8 code point in UTF mode
- keep behavior explicit as a compatibility approximation to `ext/pcre`

## 6. Iterator Method Semantics

### 6.1 Factory function

`preg_iter_ex()` will:

1. parse parameters
2. validate supported flags
3. obtain a compiled regex cache entry
4. allocate `PregIterator`
5. retain pattern/subject/cache entry
6. fetch compile metadata via `pcre2_pattern_info()`
7. allocate `match_data`
8. return the object

Recommended supported flags mask in v1:

```c
PREG_OFFSET_CAPTURE | PREG_UNMATCHED_AS_NULL
```

Any other bit should raise `zend_argument_value_error()`.

### 6.2 `rewind()`

- always reset object state
- attempt first match immediately
- after `rewind()`, `valid()` reflects whether any match exists

This makes the iterator safe for repeated `foreach` passes and avoids requiring special-case lazy initialization in `current()`.

### 6.3 `next()`

- require initialized state; if not initialized, behave like `rewind()` or fetch first match directly
- move `cursor = next_cursor`
- increment key only after a successful transition to the next match
- if already finished, remain a no-op

### 6.4 `valid()`, `current()`, `key()`

- `valid()` returns cached boolean
- `current()` returns a copy of `current_match`
- `key()` returns `current_key`

`current()` must not expose the internal `zval` by alias; return through normal Zend return-value copy semantics.

## 7. Zero-Length Match Rule

This is the most important behavioral point from the reference implementation.

Planned algorithm after a successful match:

1. set `next_cursor` to `ovector[1]`
2. if `ovector[0] != ovector[1]`, stop
3. retry from the same offset with `PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED`
4. if retry matches, use that match instead of advancing
5. if retry returns `PCRE2_ERROR_NOMATCH`, advance by one subject unit
6. if already at subject end, mark iteration finished
7. on any other PCRE error, throw internal error and finish

Compatibility note to document in code comments:

- core `ext/pcre` has access to internal helpers for subject-unit advancement and UTF validity hints
- this extension will reproduce the same high-level rule, but may differ on malformed UTF-8 input because it cannot reuse all private optimizations

## 8. Error Model

### 8.1 Invalid flags

- throw `ValueError`

### 8.2 Pattern compilation failure

- if `pcre_get_compiled_regex_cache()` returns `NULL`, surface failure to the caller
- preserve the warning emitted by the PCRE layer
- if no exception is active, throw a generic `Error` with an explicit extension-local message

### 8.3 Match-time internal failure

- convert PCRE execution errors other than `PCRE2_ERROR_NOMATCH` into `Error`
- include numeric error code in the message
- mark iterator finished after the error to avoid undefined reuse

## 9. Implementation Phases

### Phase 1: Bootstrap

- add `config.m4`
- add module entry and `MINIT`
- register `PregIterator`
- add `preg_iter_ex()` stub and arginfo

Exit criteria:

- `phpize && ./configure && make` succeeds
- module loads and function/class symbols are visible

### Phase 2: Object lifecycle

- implement allocation/free handlers
- wire function factory to create initialized object state
- store retained strings and PCRE cache entry safely

Exit criteria:

- no leaks in trivial create/destroy paths
- invalid construction paths clean up correctly

### Phase 3: Single-step matching

- create `match_data`
- execute one match from cursor `0`
- implement `rewind()`, `valid()`, `current()`, `key()`

Exit criteria:

- one-pass iteration works for ordinary non-empty matches
- current match array shape is compatible with `preg_match()`

### Phase 4: Progression and rewind

- implement `next()`
- implement zero-length retry rule
- implement repeated `rewind()`

Exit criteria:

- no infinite loop on `/(?:)/`, `/\b/`, `/^/m`, `/x*/`
- second `foreach` over the same iterator returns the same sequence

### Phase 5: Match shaping

- add named captures
- add `PREG_OFFSET_CAPTURE`
- add `PREG_UNMATCHED_AS_NULL`

Exit criteria:

- output matches the documented prototype behavior for the supported flags

### Phase 6: Tests and documentation

- add PHPT coverage
- document unsupported modes and known divergences
- add build instructions and prototype limitations to `README`

Exit criteria:

- all PHPT tests pass
- unsupported flags fail predictably

## 10. Test Plan

Minimum PHPT coverage:

- simple global iteration over multiple matches
- named capture duplication under string keys
- unmatched optional group with empty string default
- unmatched optional group with `PREG_UNMATCHED_AS_NULL`
- `PREG_OFFSET_CAPTURE` tuple layout
- zero-length cases: `/(?:)/`, `/\b/`, `/^/m`, `/x*/`
- `rewind()` idempotence across two `foreach` loops
- unsupported flags rejection
- invalid regex compilation failure

Useful stretch tests:

- UTF-8 subject with `/u`
- empty subject
- match at end-of-string
- duplicate named captures if supported by the target PCRE configuration

## 11. Known Constraints of the Prototype

- no support for `preg_match_all()` aggregation modes
- no support for start-offset parameter in v1 API
- behavior on malformed UTF-8 may differ slightly from core `ext/pcre`
- exported `php_pcre.h` availability depends on the installed PHP development headers
- extension now depends on `ext/pcre` at build/load time and should declare that explicitly

## 12. Future Improvements for a php-src Version

If the experiment moves into core `ext/pcre`, the following can be improved:

- reuse internal helpers for exact array shaping parity
- reuse internal UTF-validity caching and unit-length helpers
- avoid repeated object-level metadata discovery
- share preallocated match data like core does
- provide a native `preg_iter()` symbol without prototype suffix
- consider exposing start offset and possibly a no-array low-level iterator API

## 13. Immediate Next Step

Start with Phase 1 plus the object skeleton, but design `pregiter_fetch_next()` before writing method bodies. The zero-length progression rule drives the object state layout, so the iterator state machine should be settled before filling in user-visible methods.
