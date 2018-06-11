#ifndef ETWP_IPROFILER_HPP
#define ETWP_IPROFILER_HPP

#include <string>

#include "Utility/Exception.hpp"
#include "Utility/Macros.hpp"

namespace ETWP {

// Interface for thread-safe profiler objects
class IProfiler {
public:
    ETWP_DISABLE_COPY_AND_MOVE (IProfiler);

    class InitException : public Exception {
    public:
        InitException (const std::wstring& msg);
        virtual ~InitException () = default;
    };

    enum class ResultCode {
        Unstarted,
        Running,
        Finished,
        Stopped,
        Aborted,
        Error
    };

    IProfiler () = default;    // Might throw InitException
    virtual ~IProfiler () = default;

    virtual bool Start (std::wstring* pErrorOut) = 0;
    virtual void Stop () = 0;
    virtual void Abort () = 0;

    // If the ResultCode is Error, an error message will be placed in pErrorOut
    virtual bool IsFinished (ResultCode* pResultOut, std::wstring* pErrorOut) = 0;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_IPROFILER_HPP