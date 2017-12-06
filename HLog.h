#ifndef __HDDL_UTILS_HDDL_HLOG_H__
#define __HDDL_UTILS_HDDL_HLOG_H__

#include <string>

typedef enum {
    N_DEBUG = 0x01,
    N_INFO = 0x02,
    N_WARN = 0x04,

    N_ERROR = 0x40,
    N_FATAL = 0x80
} HLogLevel;

#define HDebug(...) HLogInner(N_DEBUG, __FILE__, __func__, __LINE__, __VA_ARGS__);

#define HInfo(...) HLogInner(N_INFO, __FILE__, __func__, __LINE__, __VA_ARGS__);

#define HWarn(...) HLogInner(N_WARN, __FILE__, __func__, __LINE__, __VA_ARGS__);

#define HError(...) HLogInner(N_ERROR, __FILE__, __func__, __LINE__, __VA_ARGS__);

#define HFatal(...) HLogInner(N_FATAL, __FILE__, __func__, __LINE__, __VA_ARGS__);

#define HLog(flag, ...) HLogInner(flag, __FILE__, __func__, __LINE__, __VA_ARGS__);

int HLogInner(int flag, const char* file, const char* func, const long line, const char* fmt, ...);

int initLog4cpp(std::string configPath);

std::string getEnvVar(const char* var);
#endif

