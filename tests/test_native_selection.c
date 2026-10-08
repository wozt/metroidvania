#include "core/native_selection.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
int main(void)
{
    NativeMap m = {0};
    m.width[0] = 5; m.height[0] = 3;
    m.blocks[0][0] = 11; m.blocks[0][1] = 12;
    m.blocks[0][5] = 21; m.blocks[0][6] = 22;
    assert(native_selection_move(&m, 0, 0, 0, 1, 1, 2, 1));
    assert(m.blocks[0][7] == 11 && m.blocks[0][8] == 12);
    assert(m.blocks[0][12] == 21 && m.blocks[0][13] == 22);
    assert(m.blocks[0][0] == 0 && m.blocks[0][6] == 0);
    NativeMap prior = m;
    assert(!native_selection_move(&m, 0, 2, 1, 3, 2, 2, 0));
    assert(!native_selection_move(&m, 0, -1, 0, 1, 1, 1, 0));
    assert(!native_selection_move(&m, 2, 0, 0, 1, 1, 1, 0));
    assert(!native_selection_move(&m, 0, 0, 0, 1, 1, 0, 0));
    assert(!native_selection_move(&m, 0, 0, 0, 1, 1, INT_MAX, 0));
    assert(memcmp(&prior, &m, sizeof(m)) == 0);
    m.blocks[0][0] = 7; m.blocks[0][1] = 8;
    assert(native_selection_move(&m, 0, 0, 0, 1, 0, 1, 0));
    assert(m.blocks[0][1] == 7 && m.blocks[0][2] == 8);
    return 0;
}
