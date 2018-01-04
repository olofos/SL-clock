#ifndef SNTP_H_
#define SNTP_H_

#define SNTP_SERVERS "0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org", "3.pool.ntp.org"
#define SNTP_DELAY   15*60

void sntp_task(void *param);

#endif
