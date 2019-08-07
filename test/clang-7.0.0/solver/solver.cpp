#include <vector>

namespace solver {

struct Item {
    int value;
    int weight;
};

int knapsack_impl(std::vector<std::vector<int>>& cache,
                  const std::vector<Item>& items, int k, int weight) {
    if (k > items.size() - 1)
        return 0;

    if (cache[k][weight] > 0)
        return cache[k][weight];

    const auto& [item_price, item_weight] = items[k];
    const int price_with =
        weight >= item_weight ? item_price + knapsack_impl(cache, items, k + 1,
                                                           weight - item_weight)
                              : 0;
    const int price_without = knapsack_impl(cache, items, k + 1, weight);

    cache[k][weight] = std::max(price_with, price_without);
    return cache[k][weight];
}

int knapsack(const std::vector<Item>& items, int weight_limit) {
    using Cache = std::vector<std::vector<int>>;
    Cache cache(items.size(), std::vector<int>(weight_limit + 1, -1));

    return knapsack_impl(cache, items, 0, weight_limit);
}

} // namespace solver
