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

    enum class Style {
        Static,
        Animated
    };

    ProgressFeedback (const std::wstring& operation,
                      const std::wstring& detail,
                      Style style,
                      State initialState = State::Idle);

    void PrintProgress ();
    void PrintProgressLine ();

    void SetState (State newState);

    void SetOperationString (const std::wstring& operation);
    void SetDetailString (const std::wstring& detail);

private:
    std::wstring m_operation;
    std::wstring m_detail;

    bool m_animated;

    State   m_state;
    bool    m_currentStatePrinted;
    uint8_t m_progressCounter;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_PROGRESS_FEEDBACK_HPP