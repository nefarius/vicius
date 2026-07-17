// Minimal E2E setup stub.
//
// The vicius updater's silent-update path launches this as a child process and
// waits for it to exit, then checks the exit code against the manifest's
// ExitCode.SuccessCodes list.  Exiting with 0 maps to success (NV_SUCCESS_EXIT_CODE = 0).
//
// The exit code can be overridden for negative-test variants by setting the
// E2E_EXIT_CODE environment variable before launching the updater.
//
// E2E_EXIT_DELAY_MS optionally sleeps before exiting, used by the async-shutdown
// lifecycle tests to keep "setup is running" true long enough for the harness to
// close the updater window mid-install (CreateProcess inherits the parent's
// environment by default, so setting this on the updater process before launch
// propagates to this child).
int delayMs = int.TryParse(Environment.GetEnvironmentVariable("E2E_EXIT_DELAY_MS"), out int d) ? d : 0;
if (delayMs > 0)
    Thread.Sleep(delayMs);

// Process-argument round-trip lifecycle test: when E2E_ARGS_LOG_FILE is set (CreateProcess
// inherits the parent's environment by default, so setting this on the updater process
// before launch propagates all the way down to this child), write each argv element
// received (i.e. exactly what CreateProcessA's own argv-splitting rules produced from the
// manifest's launchArguments, appended verbatim by ExecuteSetup) to that file, one per
// line, so the harness can assert spaces/quotes/trailing backslashes survived intact.
string? argsLogFile = Environment.GetEnvironmentVariable("E2E_ARGS_LOG_FILE");
if (!string.IsNullOrEmpty(argsLogFile))
{
    string[] receivedArgs = Environment.GetCommandLineArgs().Skip(1).ToArray();
    File.WriteAllLines(argsLogFile, receivedArgs);
}

int exitCode = int.TryParse(Environment.GetEnvironmentVariable("E2E_EXIT_CODE"), out int c) ? c : 0;
Environment.Exit(exitCode);
