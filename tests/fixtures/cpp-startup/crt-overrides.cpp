// An application or alternate CRT may supply strong definitions instead of
// the startup's weak defaults. Linking this file must not cause duplicates.
extern "C" {
void _init(void) {}
void _fini(void) {}
void *__dso_handle = nullptr;
}
