"""Restore cancellation signals before exec without a Python preexec_fn."""
import os
import signal
import sys


if __name__ == '__main__':
    signal.pthread_sigmask(signal.SIG_UNBLOCK, {signal.SIGTERM, signal.SIGINT})
    os.execvpe(sys.argv[1], sys.argv[1:], os.environ)
