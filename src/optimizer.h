#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "ast.h"
#include <memory>

class Optimizer {
public:
    Optimizer() = default;
    void optimize(const std::unique_ptr<CompUnit>& unit);

private:
    void constantFold(const std::unique_ptr<CompUnit>& unit);
    void deadCodeElimination(const std::unique_ptr<CompUnit>& unit);
};

#endif
