#include "ProgressFeedback.hpp"

#include "OS/Stream/GlobalStreams.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

ProgressFeedback::ProgressFeedback (const std::wstring& process,
                                    const std::wstring& detail,
                                    Style style,
                                    State initialState /*= State::Idle*/):
    m_operation (process),
    m_detail (detail),
    m_animated (style == Style::Animated),
    m_state (initialState),
    m_currentStatePrinted (false),
    m_progressCounter (0)
{

}

void ProgressFeedback::PrintProgress ()
{
    if (m_animated)
        COut () << L'\r';
    else if (m_currentStatePrinted)
        return;

    COut () << ColorReset << m_operation;

    if (!m_detail.empty ())
        COut () << L" " << FgColorWhite << m_detail;

    COut () << L'\t' << FgColorCyan << L"[ ";

    switch (m_state) {
        case State::Idle:
            COut () << FgColorGray << L"IDLE";
            break;
        case State::Running: {
            static wchar_t progressMap[] = { L'–', L'\\', L'|', L'/' };

            COut () << FgColorMagenta;
            if (m_animated)
                COut () << progressMap[m_progressCounter++ % 4];
            else
                COut () << L"...";
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

    if (!m_animated)
        COut () << Endl;

    m_currentStatePrinted = true;
}

void ProgressFeedback::PrintProgressLine ()
{
    PrintProgress ();

    if (m_animated) // In case of Style::Static, a newline was already printed in PrintProgress...
        COut () << Endl;
}

void ProgressFeedback::SetState (State newState)
{
    if (newState != m_state)
        m_currentStatePrinted = false;

    m_state = newState;
}

}   // namespace ETWP