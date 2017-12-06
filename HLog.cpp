//os include
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <iostream>
#include <string>

//C and C++ include
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//inner dependent include
#include "utils/HLog.h"

#ifdef USE_LOG4CPP
//log4cpp
#include "log4cpp/Portability.hh"
#include "log4cpp/Category.hh"
#include "log4cpp/Appender.hh"
#include "log4cpp/FileAppender.hh"
#include "log4cpp/OstreamAppender.hh"
#include "log4cpp/PatternLayout.hh"
#include "log4cpp/PropertyConfigurator.hh"
#include "log4cpp/Layout.hh"
#include "log4cpp/BasicLayout.hh"
#include "log4cpp/Priority.hh"
#include "log4cpp/NDC.hh"
#endif

#ifdef COLOR_LOG
    #define COL(x)  "\033[1;" #x ";40m"
    #define COLOR_END    "\033[0m"
#else
    #define COL(x)
    #define COLOR_END
#endif

#define RED     COL(31)
#define GREEN   COL(32)
#define YELLOW  COL(33)
#define BLUE    COL(34)
#define MAGENTA COL(35)
#define CYAN    COL(36)
#define WHITE   COL(0)

//#define CONFIG_FILE_PATH

//#define USE_LOG4CPP

#ifdef USE_LOG4CPP
static bool logInitilized = 0;

int initLog4cpp(std::string configPath)
{
    bool exists;
    struct stat buffer;
    exists = (stat (configPath.c_str(), &buffer) == 0);

    if(exists) {
        log4cpp::PropertyConfigurator::configure(configPath.c_str());
        return 0;
    } else {
        std::string hddlInstallDir = getEnvVar("HDDL_INSTALL_DIR");
        if (hddlInstallDir.empty()) {
            fprintf(stderr, "Warning: HDDL_INSTALL_DIR is not specified. Cannot find apilog.config.");
            return 0;
        }
        std::string insPath = hddlInstallDir + std::string("/config/hddlapilog.config");
        exists = (stat (insPath.c_str(), &buffer) == 0);
        if (exists) {
            printf("Use config file in install path %s\n", insPath.c_str());
            log4cpp::PropertyConfigurator::configure(insPath.c_str());
            return 0;
        } else {
            printf("Can not find default configure file %s\n", insPath.c_str());
        }
        return -1;
    }
}


int HLogInner(int flag, const char* file, const char* func, const long line, const char* fmt, ...)
{
    if (!logInitilized) {
        int ret = initLog4cpp("~/.hddlapilog.config");
        if (ret < 0) {
            return -1;
        }
        logInitilized = 1;
    }
    std::string fn = std::string(file);
    std::string filename = fn.substr(fn.rfind("/") + 1, (fn.rfind(".") - fn.rfind("/") - 1 ) );
    //printf("log file %s name = %s\n", fn.c_str(), filename.c_str());
    log4cpp::Category& lgrf = log4cpp::Category::getInstance(filename);

    va_list args;
    char buffer[4096] = {0};
    snprintf(buffer, sizeof(buffer), ":%s:%ld: ", func, line);
    int offset = strlen(buffer);
    int nMaxLogSize = 4096 - offset - 1;
    va_start(args, fmt);
    vsnprintf(buffer + offset, nMaxLogSize, fmt, args);
    va_end(args);

    log4cpp::Category::getInstance(filename);

    switch (flag){
    case N_DEBUG:
        lgrf.debug(buffer);
        break;

    case N_INFO:
        lgrf.info(buffer);
        break;

    case N_WARN:
        lgrf.warn(buffer);
        break;

    case N_ERROR:
        lgrf.error(buffer);
        break;

    case N_FATAL:
        lgrf.fatal(buffer);
        break;
    }

    return 0;
}
#else
static int nDebugLevel =  N_DEBUG | N_INFO| N_ERROR | N_FATAL;
int HLogInner(int flag, const char* file, const char* func, const long line, const char* fmt, ...)
{
    struct timespec tp;
    struct tm infoTime;
    struct timespec* currentTime = &tp;

    clock_gettime(CLOCK_REALTIME, currentTime);
    localtime_r(&currentTime->tv_sec, &infoTime); //thread safe.
    long usec = currentTime->tv_nsec/100000;
    int sec = infoTime.tm_sec;
    int min = infoTime.tm_min;
    int hour = infoTime.tm_hour;
    int month = infoTime.tm_mon;
    int day   = infoTime.tm_mday;
    int year  = infoTime.tm_year + 1900;

    va_list args;
    char buffer[4096] = {0};
    pid_t tid;
    tid = syscall(SYS_gettid);

    std::string filename = std::string(file);
    filename = filename.substr(filename.rfind("/") + 1);

    switch (flag & nDebugLevel){
    case N_DEBUG:
        snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d:%04ld][tid %d][" BLUE "DEBUG" COLOR_END "][%s:%s:%ld] ", year, month, day, hour, min, sec, usec, tid, filename.c_str(), func, line);
        break;

    case N_INFO:
        snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d:%04ld][tid %d][" WHITE "INFO" COLOR_END "][%s:%s:%ld] ", year, month, day, hour, min, sec, usec, tid, filename.c_str(), func, line);
        break;

    case N_WARN:
        snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d:%04ld][tid %d][" YELLOW "WARN" COLOR_END "][%s:%s:%ld] ", year, month, day, hour, min, sec, usec, tid, filename.c_str(), func, line);
        break;

    case N_ERROR:
        snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d:%04ld][tid %d][" RED "ERROR" COLOR_END "][%s:%s:%ld] ", year, month, day, hour, min, sec, usec, tid, filename.c_str(), func, line);
        break;

    case N_FATAL:
        snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d:%04ld][tid %d][" RED "FATAL" COLOR_END "][%s:%s:%ld] ", year, month, day, hour, min, sec, usec, tid, filename.c_str(), func, line);
        break;
    }

    if(flag & nDebugLevel) {
        int offset = strlen(buffer);
        int nMaxLogSize = 4096 - offset - 1;
        va_start(args, fmt);
        vsnprintf(buffer + offset, nMaxLogSize, fmt, args);
        va_end(args);
        fprintf(stdout, "%s\n", buffer);
    }

    return 0;
}
#endif

std::string getEnvVar(const char* var)
{
    std::string ret;
    char* envvar = NULL;

    envvar = getenv(var);
    if (envvar != NULL) {
        ret = envvar;
    }

    return ret;
}



