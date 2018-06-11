#include "Application/Application.hpp"
#include "Application/Error.hpp"

int wmain (int argc, wchar_t* argv[], wchar_t* /*envp[]*/)
{
    ETWP::Application::ArgumentVector arguments;
    for (int i = 0; i < argc; ++i)
        arguments.push_back (argv[i]);

    if (!ETWP::Application::Instance ().Init (arguments))
        return int (ETWP::GlobalErrorCodes::InitializationError);

    return ETWP::Application::Instance ().Run ();
}