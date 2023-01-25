/**
 * A very simple implementation of Naughty Dog's tagged heap concept talked about
 * here: https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 * 
 * Here's how it works:
 *  * A set of tags exist representing different stages of the game loop (e.g. game, rendering, gpu)
 *  * For each tag a set of 2MiB blocks of memory are allocated from the system. By default only
 *      one is allocated per tag. Each block is essentially its own arena allocator.
 *  * Allocations are made by passing in a tag and the allocation size. The allocated memory is
 *      grabbed from the tag's most recently allocated block. If the block is full, a new one is
 *      allocated and then used.
 *  * You cannot free individual pieces of memory under a tag. You must free all of the memory
 *      associated with that tag at the same time.
 * 
 * I renamed from "tagged heap" to "tagged arena" since it's more accurate.
 * 
 * I took some liberties while implementing for simplicity sakes,
 *  * cannot allocate >2MiB
 *  * no multi-threading support or fiber support
 * 
 * Possible optimizations/improvements:
 *  * reuse of ArenaBlocks by adding/pulling them from a pool
 *  * allow for >2MiB allocations
 */

#include <iostream>

enum class ArenaTag : uint8_t {
    SHARED,
    GAME,
    RENDERING,
    GPU,
    COUNT
};

class TaggedArena {
public:
    static constexpr size_t BLOCK_SIZE = 8 * 1024 * 1024 * 2; // 2MiB

    class ArenaBlock {
    private:
        uint8_t* data;
        size_t offset = 0;

    public:
        ArenaBlock() {
            this->data = new uint8_t[BLOCK_SIZE];
        }

        ~ArenaBlock() {
            delete[] this->data;
        }

        void* alloc(size_t size, size_t alignment) {
            if (size + alignment + offset > BLOCK_SIZE)
                return nullptr;

            this->offset += alignment;
            uint8_t* data = &this->data[this->offset];
            this->offset += size;
            return data;
        }
    };

    struct BlockNode {
        ArenaBlock block;
        BlockNode* next;
    };
    // Array of BlockNode* linked lists holding all blocks for each tag
    BlockNode* blocks[static_cast<size_t>(ArenaTag::COUNT) - 1];
    
public:
    TaggedArena() {
        for (uint8_t i = 0; i < static_cast<uint8_t>(ArenaTag::COUNT) - 1; i++) {
            this->blocks[i] = new BlockNode { .block = ArenaBlock(), .next = nullptr };
        }
    }

    ~TaggedArena() {
        for (uint8_t i = 0; i < static_cast<uint8_t>(ArenaTag::COUNT) - 1; i++) {
            this->free(static_cast<ArenaTag>(i));
        }
    }

    /**
     * Allocate however many bytes
     * @param tag Tag to allocate under
     * @param size Amount of bytes to allocate
     * @param alignment Alignment of the allocation
     * @returns allocation
     */
    void* alloc(ArenaTag tag, size_t size, size_t alignment) {
        // Don't allow allocations >2MiB
        if (size > BLOCK_SIZE || tag == ArenaTag::COUNT)
            return nullptr;

        BlockNode* node = this->blocks[static_cast<uint8_t>(tag)];
        void* bytes = node->block.alloc(size, alignment);
        if (!bytes) {
            // Block ran out of space, allocate a new one.
            // Potential problem: it is possible that a block does actually have space left,
            //   the allocation was just too big for it.
            node = new BlockNode { .block = ArenaBlock(), .next = node };
            bytes = node->block.alloc(size, alignment);
            this->blocks[static_cast<uint8_t>(tag)] = node;
        }

        return bytes;
    }

    /**
     * Allocate a type
     * @param tag Tag to allocate under
     * @param alignment Overwrite whatever alignment the type has for this allocation
     * @returns allocation
     */
    template<typename T>
    T* alloc(ArenaTag tag, size_t alignment = alignof(T)) {
        T* data = reinterpret_cast<T*>(this->alloc(tag, sizeof(T), alignment));
        new(data) T;
        return data;
    }

    /**
     * Free all allocations associated with a specific tag
     * @param tag Tag to free
     */
    void free(ArenaTag tag) {
        if (tag == ArenaTag::COUNT)
            return;

        BlockNode* node = this->blocks[static_cast<uint8_t>(tag)];
        while (node) {
            BlockNode* next = node->next;
            delete node;
            node = next;
        }
    }
};

int main() {
    TaggedArena arena;
    auto byte = arena.alloc<uint8_t>(ArenaTag::GAME);
    *byte = 10;

    struct TestingObject {
        int32_t a = 10;
        const char* asd = "123";
    };
    auto object = arena.alloc<TestingObject>(ArenaTag::GAME);
    object->a *= 2;

    arena.free(ArenaTag::GAME);

    std::cin.get();
}