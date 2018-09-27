#pragma once
#include "DefaultIgnore.h"

namespace slv2
{

    class HandleRTState final : public DefaultIgnore
    {
    public:

        ~HandleRTState() override
        {}

        void beforeOn(SolverContext& context) override;

        const char * name() const override
        {
            return "Handle RT";
        }
    };

} // slv2
