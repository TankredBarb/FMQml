#include "FilePanelLoadMorePolicy.h"
bool dispatchLoadMoreRequest(int row, bool loading, const std::function<void(int)> &dispatch)
{
    if (row < 0 || loading || !dispatch) return false;
    dispatch(row);
    return true;
}
