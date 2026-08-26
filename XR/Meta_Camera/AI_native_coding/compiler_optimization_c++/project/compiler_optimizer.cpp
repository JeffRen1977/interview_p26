#include "compiler_optimizer.h"

#include <stdexcept>

Metrics analyze(const std::vector<Instr>& program, const std::map<std::string, int>& costs) {
    (void)program;
    (void)costs;
    // TODO(Phase 2): last-use table, then sweep the instructions.
    throw std::logic_error("Phase 2: implement analyze");
}

std::vector<Instr> eliminate_dead_code(const std::vector<Instr>& program) {
    (void)program;
    // TODO(Phase 3): walk back from kOutput, collect the needed targets, filter.
    throw std::logic_error("Phase 3: implement eliminate_dead_code");
}

std::vector<Instr> fold_constants(const std::vector<Instr>& program) {
    (void)program;
    // TODO(Phase 4): forward pass with a std::map<std::string, long long> of
    // known constants.
    throw std::logic_error("Phase 4: implement fold_constants");
}

std::pair<std::vector<Instr>, Metrics> optimize(const std::vector<Instr>& program,
                                                const std::map<std::string, int>& costs) {
    const std::vector<Instr> folded = fold_constants(program);
    const std::vector<Instr> live = eliminate_dead_code(folded);
    return {live, analyze(live, costs)};
}
