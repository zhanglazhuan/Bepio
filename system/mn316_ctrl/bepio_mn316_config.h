#ifndef BEPIO_MN316_CONFIG_H_
#define BEPIO_MN316_CONFIG_H_

/* Public broker/identity/routing rules. Secrets live only in emqx_local.h. */
#define BEPIO_MN316_EMQX_HOST "p235d1e1.ala.cn-hangzhou.emqxsl.cn"
#define BEPIO_MN316_EMQX_PORT 8883
#define BEPIO_MN316_TOPIC_PREFIX "pager/demo/"
#define BEPIO_MN316_TEST_PEER "100000000000002"
#define BEPIO_MN316_TEST_INBOX BEPIO_MN316_TOPIC_PREFIX BEPIO_MN316_TEST_PEER "/inbox"

#endif
