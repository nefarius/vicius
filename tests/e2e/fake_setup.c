/* Minimal Win32 PE that exits with code 0.
   Compiled in CI with: cl /nologo /MT fake_setup.c /Fe:fake_setup.exe
   The updater calls GetBinaryTypeA() then CreateProcessA() on this file.
   An exit code of 0 maps to success in InstanceConfig::InvokeSetup(). */
int main(void) { return 0; }
