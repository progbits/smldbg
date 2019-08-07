#include <iostream>
#include <vector>

#include "solver.h"

int main(int argc, char** argv) {
    std::vector<solver::Item> items = {
        {.value = 60, .weight = 5},
        {.value = 50, .weight = 3},
        {.value = 70, .weight = 4},
        {.value = 30, .weight = 2},
    };

    std::cout << "Items: \n";
    for (const auto& item : items) {
        std::cout << "Value: " << item.value;
        std::cout << " Weight: " << item.weight << "\n";
    }

    int weight_limit = 9;
    int price = solver::knapsack(items, weight_limit);
    std::cout << "Best price for weight limit " << weight_limit << " = "
              << price << "\n";

    return 0;
}
