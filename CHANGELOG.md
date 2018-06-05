2018/06/02

1. 修正kqueue api hook 在仅EV_SET操作时的错误；
2. 优化c++11下hook性能；



2017/08/24

1. 修正hook时使用va_list错误；



2017/05/08

1. 同步到同期最新版的CxxJDK；  
2. 添加调度侦听功能（@see：setScheduleCallback）；  
3. 添加多线程调度自定义负载均衡功能（@see：setBalanceCallback）；  
4. 添加对sendfile()的hook支持；  