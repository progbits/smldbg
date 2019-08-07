#include <vector>

namespace solver {

struct Item {
    int value;
    int weight;
};

int knapsack_impl(std::vector<std::vector<int>>& cache,
                  const std::vector<Item>& items, int k, int weight);

int knapsack(const std::vector<Item>& items, int weight_limit);

} // namespace solver
