#include "OnExit.hpp"

namespace ETWP {

OnExit::OnExit (const ExitFunction& exitFunc):
    m_exitFunc (exitFunc),
    m_active (true)
{
}

OnExit::~OnExit ()
{
    Trigger ();
}

void OnExit::Deactivate ()
{
    m_active = false;
}

void OnExit::Trigger ()
{
    if (m_active) {
        m_exitFunc ();

        m_active = false;
    }
}

}   // namespace ETWP