#! /bin/sh
#
ulimit -c unlimited
# start_onebox.sh

# first start zookeeper
IP=127.0.0.1

ZK_CLUSTER=$IP:6181
NS1=$IP:9622
NS2=$IP:9623
NS3=$IP:9624
TABLET1=$IP:9520
TABLET2=$IP:9521
TABLET3=$IP:9522
BLOB1=$IP:9720

RAMBUILD_PREFIX=/tmp/rambuild
../build/bin/rtidb --db_root_path=${RAMBUILD_PREFIX}/tablet0-binlogs \
                   --recycle_bin_root_path=${RAMBUILD_PREFIX}/recycle_bin0 \
                   --endpoint=${TABLET1} --role=tablet \
                   --binlog_notify_on_put=true\
                   --enable_distsql=true\
                   --zk_cluster=${ZK_CLUSTER}\
                   --zk_keep_alive_check_interval=100000000\
                   --zk_root_path=/cluster> tablet0.log 2>&1 &

# start tablet1
../build/bin/rtidb --db_root_path=${RAMBUILD_PREFIX}/tablet1-binlogs \
                   --recycle_bin_root_path=${RAMBUILD_PREFIX}/recycle_bin1 \
                   --endpoint=${TABLET2} --role=tablet \
                   --zk_cluster=${ZK_CLUSTER}\
                   --binlog_notify_on_put=true\
                   --enable_distsql=true\
                   --zk_keep_alive_check_interval=100000000\
                   --zk_root_path=/cluster > tablet1.log 2>&1 &

# start tablet2
../build/bin/rtidb --db_root_path=${RAMBUILD_PREFIX}/tablet2-binlogs \
                   --recycle_bin_root_path=${RAMBUILD_PREFIX}/recycle_bin2 \
                   --endpoint=${TABLET3} --role=tablet \
                   --binlog_notify_on_put=true\
                   --enable_distsql=true\
                   --zk_cluster=${ZK_CLUSTER}\
                   --zk_keep_alive_check_interval=100000000\
                   --zk_root_path=/cluster > tablet2.log 2>&1 &

# start ns1
../build/bin/rtidb --endpoint=${NS1} --role=nameserver \
                   --zk_cluster=${ZK_CLUSTER}\
                   --tablet_offline_check_interval=1\
                   --tablet_heartbeat_timeout=1\
                   --request_timeout_ms=100000\
                   --zk_root_path=/cluster > ns1.log 2>&1 &
sleep 2

# start ns2
../build/bin/rtidb --endpoint=${NS2} --role=nameserver \
                   --zk_cluster=${ZK_CLUSTER}\
                   --tablet_offline_check_interval=1\
                   --tablet_heartbeat_timeout=1\
                   --request_timeout_ms=100000\
                   --zk_root_path=/cluster > ns2.log 2>&1 &
sleep 2

# start ns3
../build/bin/rtidb --endpoint=${NS3} --role=nameserver \
                   --tablet_offline_check_interval=1\
                   --tablet_heartbeat_timeout=1\
                   --request_timeout_ms=100000\
                   --zk_cluster=${ZK_CLUSTER}\
                   --zk_root_path=/cluster > ns3.log 2>&1 &

sleep 5

echo "start all ok"

