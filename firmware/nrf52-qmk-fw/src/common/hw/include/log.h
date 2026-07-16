#ifndef LOG_H_
#define LOG_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_LOG

#define LOG_CH            HW_LOG_CH
#define LOG_BOOT_BUF_MAX  HW_LOG_BOOT_BUF_MAX
#define LOG_LIST_BUF_MAX  HW_LOG_LIST_BUF_MAX


bool logInit(void);
void logEnable(void);
void logDisable(void);
bool logOpen(uint8_t ch, uint32_t baud);
void logBoot(uint8_t enable);
void logPrintf(const char *fmt, ...);

#else

// 로그 비활성 빌드에서도 호출부가 그대로 컴파일되도록 no-op 으로 만든다.
// (logPrintf 는 드라이버/앱 곳곳에서 쓰이므로 호출부마다 #ifdef 를 두지 않는다)
#define logInit()             (true)
#define logEnable()           ((void)0)
#define logDisable()          ((void)0)
#define logOpen(ch, baud)     (true)
#define logBoot(enable)       ((void)0)
#define logPrintf(...)        ((void)0)

#endif

#ifdef __cplusplus
}
#endif



#endif