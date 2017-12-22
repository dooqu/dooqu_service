#ifndef MYSQL_CONN_POOL_H
#define MYSQL_CONN_POOL_H

#include "../util/utility.h"
#include "../util/tick_count.h"
#include "../util/char_key_op.h"
#include "../util/service_status.h"

namespace dooqu_service
{
namespace data
{
class mysqlconn_pool
{
public:
    mysqlconn_pool();
    virtual ~mysqlconn_pool();

protected:

private:
};
}
}


#endif // MYSQL_CONN_POOL_H
