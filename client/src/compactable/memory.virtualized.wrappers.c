#include "../memory.h"
int tigress_virtualized_allocate_test_memory(TestMemory *, size_t);
int tigress_virtualized_copy_test_data(TestMemory *, const char *, size_t);
void tigress_virtualized_inspect_memory(const TestMemory *, size_t);
void tigress_virtualized_free_test_memory(TestMemory *);
void inspect_memory(const TestMemory *m, size_t n) { tigress_virtualized_inspect_memory(m, n); }
