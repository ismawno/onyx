// A crashing test must end its process, not wait on a dialog. On Windows an assert, an abort or an unhandled exception
// can open a message box: Windows Error Reporting, or the debug CRT's "Debug Assertion Failed" and "abort() has been
// called". On a CI runner nobody closes it, so the worker hangs until the watchdog ends it and the crash reads as a
// timeout. This file keeps windows.h away from the Onyx headers, whose names its macros could rename. Elsewhere it
// does nothing.

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#    include <crtdbg.h>
#    include <cstdlib>
#endif

namespace OnyxTest
{
void SilenceCrashDialogs()
{
#ifdef _WIN32
    // no Windows Error Reporting box for a crash, and no box for a missing file or drive. child processes inherit it
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    // abort() ends the process quietly: no message, no fault report
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    // the debug CRT reports its asserts and errors in a message box: send them to stderr, which the supervisor reads.
    // without _DEBUG these are macros that expand to nothing
    (void)_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    (void)_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    (void)_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    (void)_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
}
} // namespace OnyxTest
