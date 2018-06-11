#ifndef ETWP_PROGRESS_FEEDBACK_HPP
#define ETWP_PROGRESS_FEEDBACK_HPP

#include <string>

#include "Utility/Macros.hpp"

namespace ETWP {

class ProgressFeedback final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (ProgressFeedback);

    enum class State {
        Idle,
        Running,
        Finished,
        Error,
        Aborted
    };

    ProgressFeedback (const std::wstring& process,
                      const std::wstring& detail,
                      State initialState = State::Idle);

    void PrintProgress () const;
    void PrintProgressLine () const;

    void SetState (State newState);

private:
    std::wstring m_process;
    std::wstring m_detail;

    State m_state;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_PROGRESS_FEEDBACK_HPP