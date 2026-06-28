// Minimal E2E setup stub.
//
// The vicius updater's silent-update path launches this as a child process and
// waits for it to exit, then checks the exit code against the manifest's
// ExitCode.SuccessCodes list.  Exiting with 0 maps to success (NV_SUCCESS_EXIT_CODE = 0).
//
// The exit code can be overridden for negative-test variants by setting the
// E2E_EXIT_CODE environment variable before launching the updater.

int exitCode = int.TryParse(Environment.GetEnvironmentVariable("E2E_EXIT_CODE"), out int c) ? c : 0;
Environment.Exit(exitCode);
