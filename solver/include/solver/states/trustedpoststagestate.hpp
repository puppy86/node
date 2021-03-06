#pragma once
#include <timeouttracking.hpp>
#include "defaultstatebehavior.hpp"

namespace cs {
/**
 * @class   TrustedPostStageState
 *
 * @brief   TODO:
 *
 * @author  Alexander Avramenko
 * @date    09.10.2018
 *
 * @sa  T:TrustedState
 *
 * ### remarks  Aae, 30.09.2018.
 */

class TrustedPostStageState final : public DefaultStateBehavior {
public:
    ~TrustedPostStageState() override {
    }

    void on(SolverContext& context) override;

    void off(SolverContext& context) override;

    Result onStage3(SolverContext& context, const cs::StageThree& stage) override;

    const char* name() const override {
        return "Trusted-Z";
    }
    void request_stages(SolverContext& context);
    void request_stages_neighbors(SolverContext& context);
    void mark_outbound_nodes(SolverContext& context);

protected:
    size_t cnt_recv_stages;

    TimeoutTracking timeout_request_stage;
    TimeoutTracking timeout_request_neighbors;
    TimeoutTracking timeout_force_transition;
};

}  // namespace cs
