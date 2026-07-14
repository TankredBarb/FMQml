#pragma once
#include <functional>
bool dispatchLoadMoreRequest(int row, bool loading, const std::function<void(int)> &dispatch);
