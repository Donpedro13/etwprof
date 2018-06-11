#include "ProgressFeedback.hpp"

#include "OS/Stream/GlobalStreams.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

ProgressFeedback::ProgressFeedback (const std::wstring& process,
                                    const std::wstring& detail,
                                    State initialState /*= State::Idle*/):
    m_process (process),
    m_detail (detail),
    m_state (initialState)
{

}

void ProgressFeedback::PrintProgress () const
{
    COut () << L'\r' << ColorReset << m_process;

    if (!m_detail.empty ()) {
        COut () << L" " << FgColorWhite << m_detail;
    }

    COut () << L'\t' << FgColorCyan << L"[ ";

    switch (m_state) {
        case State::Idle:
            COut () << FgColorGray << L"IDLE";
            break;
        case State::Running: {
            static uint8_t counter;
            static wchar_t progressMap[] = { L'–', L'\\', L'|', L'/' };

            ++counter;

            COut () << FgColorMagenta << progressMap[counter % 4];
        }
            break;
        case State::Finished:
            COut () << FgColorGreen << L"FINISHED";
            break;
        case State::Error:
            COut () << FgColorRed << L"ERROR";
            break;
        case State::Aborted:
            COut () << FgColorYellow << L"ABORTED";
            break;
        default:
            ETWP_DEBUG_BREAK_STR (L"Invalid ProgressFeedback state!");
            break;
    }


    COut () << FgColorCyan << L" ]" << ColorReset;
}

void ProgressFeedback::PrintProgressLine () const
{
    PrintProgress ();
    COut () << Endl;
}

void ProgressFeedback::SetState (State newState)
{
    m_state = newState;
}

}   // namespace ETWP