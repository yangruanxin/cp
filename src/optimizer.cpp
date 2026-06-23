#include "optimizer.h"

void Optimizer::optimize(const std::unique_ptr<CompUnit>& unit) {
    constantFold(unit);
    deadCodeElimination(unit);
}

void Optimizer::constantFold(const std::unique_ptr<CompUnit>& unit) {
    (void)unit;
    // TODO: implement constant propagation and folding
}

void Optimizer::deadCodeElimination(const std::unique_ptr<CompUnit>& unit) {
    (void)unit;
    // TODO: implement dead code elimination
}
