# tagged-arena-allocator
A very simple implementation of Naughty Dog's tagged heap concept talked about here: https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine

## How it works
 * A set of tags exist representing different stages of the game loop (e.g. game, rendering, gpu)
 * For each tag a set of 2MiB blocks of memory are allocated from the system. By default only
     one is allocated per tag. Each block is essentially its own arena allocator.
 * Allocations are made by passing in a tag and the allocation size. The allocated memory is
     grabbed from the tag's most recently allocated block. If the block is full, a new one is
     allocated and then used.
 * You cannot free individual pieces of memory under a tag. You must free all of the memory
     associated with that tag at the same time.

## Where the implementation differs from Naughty Dog
These were excluded for simplicity:
 * cannot allocate >2MiB
 * no multi-threading support or fiber support

## Possible optimizations/improvements
 * reuse of ArenaBlocks by adding/pulling them from a pool
 * allow for >2MiB allocations