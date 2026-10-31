# apps

业务应用按 app/model/view/controller 分离。

`settings/` 为第一个应用，默认在 app_main 中显式注册，并由 main 打开。
应用通过 `system/appmgr/bepio_app_manager.h` 接入生命周期；描述符须为静态存储期，
回调在调用方线程同步执行。新增应用需在根 CMakeLists.txt 注册组件，并在 app_main 中注册。

`contacts/` 为第二个应用，提供联系人列表、详情、新增、修改和删除。
通过 contacts shell 命令打开，需先关闭当前应用；数据暂存 RAM。

`messages/` 为第三个应用，提供收发箱、草稿编辑、确认发送与删除，
通过 `system/services/messaging/` 管理消息。联系人通过只读快照接口访问，不必切换应用。
真实服务端未接入时普通消息保留在队列，独立演示构建可显式模拟收发。
