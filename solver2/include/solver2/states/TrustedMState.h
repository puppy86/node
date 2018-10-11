#pragma once
#include "TrustedState.h"

namespace slv2
{
    /**
     * @class   TrustedMState
     *
     * @brief   A trusted node state with matrices completed, but vectors still not. This class
     *          cannot be inherited
     *
     * @author  Alexander Avramenko
     * @date    09.10.2018
     *
     * @sa  T:TrustedState  
     *
     * ### remarks  Aae, 30.09.2018.
     */

    class TrustedMState final : public TrustedState
    {
    public:

        ~TrustedMState() override
        {}

        void on(SolverContext& context) override;

        // onVector() behaviour is completely implemented in TrustesState

        Result onMatrix(SolverContext& context, const Credits::HashMatrix& matr, const PublicKey& sender) override;

        const char * name() const override
        {
            return "TrustedM";
        }

    };

} // slv2