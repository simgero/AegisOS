#include <jni.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

namespace {
termios saved_mode;
volatile sig_atomic_t mode_active;
volatile sig_atomic_t password_mode;
constexpr int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGTSTP};
constexpr unsigned signal_count = sizeof(signals) / sizeof(signals[0]);
struct sigaction saved_actions[signal_count];

void fail(JNIEnv* env, const char* text) {
    env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), text);
}
void restore_at_exit() {
    if (mode_active) {
        // A signal can arrive before the canonical password line has a
        // delimiter. Do not hand that unread input to the parent shell.
        // Raw GNU terminal input has a separate lifecycle and is not flushed.
        if (password_mode) tcflush(STDIN_FILENO, TCIFLUSH);
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_mode);
    }
}
void interrupted(int number) {
    // No allocation, JNI, logging or second reader in a signal handler.
    // Binder death revokes the server channel even if Java cleanup cannot run.
    restore_at_exit();
    _exit(128 + number);
}
bool console() { return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO); }
bool dimensions(int rows, int columns) {
    return rows >= 1 && rows <= 1000 && columns >= 1 && columns <= 1000;
}
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    atexit(restore_at_exit);
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_aegisos_identity_TerminalNative_isConsole(JNIEnv*, jclass) {
    return console();
}

extern "C" JNIEXPORT void JNICALL
Java_org_aegisos_identity_TerminalNative_beginMode(JNIEnv* env, jclass, jint mode) {
    if (!console() || mode_active || (mode != 0 && mode != 1)) {
        fail(env, "Interactive terminal mode is unavailable"); return;
    }
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    for (int signal : signals) sigaddset(&mask, signal);
    if (sigprocmask(SIG_BLOCK, &mask, &old_mask) < 0) {
        fail(env, "Cannot protect terminal transition"); return;
    }
    int installed = 0;
    bool changed = false;
    termios next;
    if (tcgetattr(STDIN_FILENO, &saved_mode) < 0) goto done;
    next = saved_mode;
    if (mode == 1) cfmakeraw(&next);
    else {
        next.c_lflag = (next.c_lflag | ICANON | ISIG) & ~(ECHO | ECHONL);
        next.c_iflag = (next.c_iflag | ICRNL) & ~(INLCR | IGNCR);
    }
    {
        struct sigaction action = {};
        action.sa_handler = interrupted;
        action.sa_mask = mask;
        for (int signal : signals) {
            if (sigaction(signal, &action, &saved_actions[installed]) < 0) goto done;
            ++installed;
        }
    }
    password_mode = mode == 0;
    mode_active = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &next) < 0) { mode_active = 0; goto done; }
    changed = true;
done:
    if (!changed) {
        while (installed) {
            --installed;
            sigaction(signals[installed], &saved_actions[installed], nullptr);
        }
    }
    sigprocmask(SIG_SETMASK, &old_mask, nullptr);
    if (!changed) fail(env, "Cannot change terminal mode");
}

extern "C" JNIEXPORT void JNICALL
Java_org_aegisos_identity_TerminalNative_endMode(JNIEnv* env, jclass) {
    if (!mode_active) return;
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    for (int signal : signals) sigaddset(&mask, signal);
    if (sigprocmask(SIG_BLOCK, &mask, &old_mask) < 0) {
        fail(env, "Cannot protect terminal restoration"); return;
    }
    int result = tcsetattr(STDIN_FILENO, TCSANOW, &saved_mode);
    // If restoration fails, retain the emergency signal/exit restoration.
    if (result == 0) {
        mode_active = 0;
        password_mode = 0;
        for (unsigned i = 0; i < signal_count; ++i) sigaction(signals[i], &saved_actions[i], nullptr);
    }
    sigprocmask(SIG_SETMASK, &old_mask, nullptr);
    if (result < 0) fail(env, "Terminal restoration was not confirmed");
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_org_aegisos_identity_TerminalNative_readInput(JNIEnv* env, jclass, jint maximum, jint timeout) {
    if (!console() || maximum < 1 || maximum > 4096 || timeout < 0 || timeout > 1000) {
        fail(env, "Invalid terminal input request"); return nullptr;
    }
    pollfd pending = {STDIN_FILENO, POLLIN, 0};
    int ready = poll(&pending, 1, timeout);
    if (ready < 0 && errno != EINTR) { fail(env, "Terminal poll failed"); return nullptr; }
    if (ready <= 0) return env->NewByteArray(0);
    if (!(pending.revents & POLLIN)) {
        if (pending.revents & POLLHUP) return nullptr;
        fail(env, "Terminal input is unavailable"); return nullptr;
    }
    jbyte bytes[4096];
    // The CLI has exactly one stdin reader, including password and raw modes.
    // It never mixes java.io.Console's buffered reader with native stdin reads.
    ssize_t count = read(STDIN_FILENO, bytes, static_cast<size_t>(maximum));
    if (count < 0 && errno == EINTR) return env->NewByteArray(0);
    if (count < 0) { fail(env, "Terminal read failed"); return nullptr; }
    if (count == 0) return nullptr;
    jbyteArray result = env->NewByteArray(static_cast<jsize>(count));
    if (result) env->SetByteArrayRegion(result, 0, static_cast<jsize>(count), bytes);
    memset_explicit(bytes, 0, sizeof(bytes));
    return result;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_org_aegisos_identity_TerminalNative_consoleSize(JNIEnv* env, jclass) {
    winsize size = {};
    if (!console() || ioctl(STDIN_FILENO, TIOCGWINSZ, &size) < 0) {
        fail(env, "Cannot read terminal dimensions"); return nullptr;
    }
    jint values[] = {size.ws_row ? size.ws_row : 24, size.ws_col ? size.ws_col : 80};
    if (!dimensions(values[0], values[1])) {
        fail(env, "Terminal dimensions exceed bounds"); return nullptr;
    }
    jintArray result = env->NewIntArray(2);
    if (result) env->SetIntArrayRegion(result, 0, 2, values);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_org_aegisos_identity_TerminalNative_resize(JNIEnv* env, jclass, jint fd, jint rows, jint columns) {
    struct stat st;
    if (!dimensions(rows, columns) || fd < 0 || fstat(fd, &st) < 0
            || !S_ISCHR(st.st_mode) || st.st_rdev != 0x502 || !isatty(fd)) {
        fail(env, "Invalid private terminal"); return;
    }
    winsize size = {};
    size.ws_row = static_cast<unsigned short>(rows);
    size.ws_col = static_cast<unsigned short>(columns);
    if (ioctl(fd, TIOCSWINSZ, &size) < 0) fail(env, "Terminal resize failed");
}
