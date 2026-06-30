#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "ir.h"

class Optimizer {
public:
    Optimizer() = default;
    IRList optimize(const IRList& ir);

private:
    IRList copyPropagation(const IRList& ir);
    IRList constantFolding(const IRList& ir);
    IRList simplifyJumps(const IRList& ir);
    IRList deadCodeElimination(const IRList& ir);
};

#endif
