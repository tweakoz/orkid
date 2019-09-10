#include "Config.h"
#include "TestReporterStdout.h"
#include <cstdio>

#include "TestDetails.h"

#ifdef UNITTEST_NDS
# define PRINTF	OS_Printf
#else
# define PRINTF std::printf
#endif

namespace UnitTest {

void TestReporterStdout::ReportFailure(TestDetails const& details, char const* failure)
{
#ifdef __APPLE__
    char const* const errorFormat = "%s:%d: error: Failure in %s: %s\n";
#else
    char const* const errorFormat = "%s(%d): error: Failure in %s: %s\n";
#endif
    PRINTF(errorFormat, details.filename, details.lineNumber, details.testName, failure);
}

void TestReporterStdout::ReportTestStart(TestDetails const& /*test*/)
{
}

void TestReporterStdout::ReportTestFinish(TestDetails const& /*test*/, float)
{
}

void TestReporterStdout::ReportSummary(int const totalTestCount, int const failedTestCount,
                                       int const failureCount, float secondsElapsed)
{
    if (failureCount > 0)
        PRINTF("FAILURE: %d out of %d tests failed (%d failures).\n", failedTestCount, totalTestCount, failureCount);
    else
        PRINTF("Success: %d tests passed.\n", totalTestCount);
    PRINTF("Test time: %.2f seconds.\n", secondsElapsed);
}

}
