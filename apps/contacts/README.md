# Contacts：触摸通讯录

240×320竖屏、LVGL 9。从 launcher 点击 Contacts 打开，无需串口命令。
浅色列表、姓名字母头像、按姓名排序、顶部搜索框和新增按钮，底部系统 Back。

## 操作

- **查找**：按姓名（ASCII大小写不敏感）或号码片段实时过滤，显示匹配数量。
  搜索框唤起字母键盘，完成键/Back收起；空结果显示提示，清除按钮恢复列表。
- **详情**：点击联系人查看姓名与号码，支持返回列表、编辑、删除。
- **新增/编辑**：姓名和数字设备 ID 输入框。
  Save写入controller/model并进入详情；Cancel或Back在有未保存修改时确认放弃。
- **删除**：在详情页弹出底部确认窗；Keep contact 保留，Delete contact 删除并返回列表。
- **浏览**：列表可上下滚动，每页最多12项，更多联系人提供More/Previous，控制LVGL内存占用。
- **返回**：详情回列表，列表回launcher；编辑页有未保存修改时显示底部确认窗。

当前界面和屏幕键盘为英文/拉丁字符；底层保留既有UTF-8姓名验证，尚无中文输入法和完整中文字库。

## 联系人存储

首次启动会根据账户服务的当前设备 ID 创建一个 `Self` 联系人（例如 `Self` / `100`），用于给自己发送消息检查收发链路；不会注入其他示例联系人。已有 NVS 快照仍会恢复。初始化标记存储在 `bepio_contacts/self_id`：删除 Self 后重启不会重新创建；设备 ID 更改时，仍保留原名和号码的 Self 联系人会同步到新 ID。

增删改和收藏切换成功后立即提交到 Flash NVS（`bepio_contacts/state`）。
重启恢复联系人、稳定 ID 和收藏状态；编辑昵称/号码保留收藏，删除联系人同时删除其收藏。
用户删除全部联系人后重启仍保持空列表。
写入失败会回滚本次内存变更并返回错误，详情页会显示保存失败提示。
读取失败或快照版本/内容非法时返回错误，不擦除 NVS。
原生主机 GUI/单元测试默认不访问 NVS；专门的持久化回归注入内存存储适配器。

持久化回归：`python tests/unittest/contacts_storage/run.py`，覆盖重建模型后恢复、
收藏/取消、编辑保留收藏、删除、稳定 ID、写入失败回滚和损坏快照拒绝。
实板待验证：收藏后断电重启，确认详情与列表标记一致；修改号码/删除后重启验证。
删空后不会自动重新填充。

## 分层与验证

- `app.c`：注册并恢复已保存的联系人。
- `model.c`：CRUD、固定ID、重复号码/容量/输入验证、搜索匹配；model_init仍为空。
- `controller.c`：详情、草稿、保存、删除确认、取消/放弃及返回状态。
- `view.c`：注册独立页面并在退出时释放当前页面上下文；controller 通过 `page_navigator` 导航，草稿和搜索状态保存在 model 的临时 `flow` 中。

姓名非空、最长48字节，不接受控制字符或全空格；设备 ID 最长24字节，只接受数字。
号码必须唯一；不做国家码格式归一化。删除不改变其他ID，同次启动不复用已删除ID。

```powershell
python tests/unittest/contacts/run.py
```

主机回归覆盖CRUD、容量、ID、搜索、草稿校验、保存、放弃和删除确认。
`tests/integration/launcher_preview/contacts_main.c` 使用真实LVGL、实际MVC和应用管理器，
模拟控件事件检查搜索、键盘边界/加号输入、增改删、校验、取消、分页与重开保留数据。
固件构建与主机测试不代替手指点击、触摸坐标和长时间运行的实板检查。

## 实际源码渲染预览

以下由真实LVGL主机渲染，不是实板照片：

| 列表 | 详情 | 编辑 |
|---|---|---|
| ![列表](../../docs/assets/contacts-list.png) | ![详情](../../docs/assets/contacts-detail.png) | ![编辑](../../docs/assets/contacts-edit.png) |

[设备 ID 输入](../../docs/assets/contacts-device-id.png) ·
[搜索](../../docs/assets/contacts-search.png) ·
[校验错误](../../docs/assets/contacts-error.png) ·
[删除确认](../../docs/assets/contacts-delete.png) ·
[放弃草稿](../../docs/assets/contacts-discard.png)

## Page source layout

`view.c` registers page builders with `page_navigator`; each builder creates its own `lv_page` screen and context. The controller selects pages from model state. Each page is compiled separately:

- `pages/contacts_list.c`: list, sorting, pagination and Add action.
- `pages/contacts_detail.c`: Contact Detail, Send Message, Edit/Delete icons.
- `pages/contacts_edit.c`: New/Edit Contact form and keyboard events.
- `pages/contacts_detail.c`: Contact Detail and delete confirmation bottom sheet.
- `pages/contacts_edit.c`: Edit form and discard confirmation bottom sheet.
- `pages/contacts_page_common.c`: shared controls, error display and controller actions.
- `pages/contacts_pages_internal.h`: private UI context and page interfaces, LVGL thread only.

Data changes continue through the controller. Both firmware and LVGL preview builds list these sources explicitly.

Each page has a matching `contacts_*.h` declaring its builder. `contacts_pages_internal.h` declares the page context shape; list and edit controls live in their own page contexts. Shared controls are declared in `contacts_page_common.h`. Controller unit tests use a view test double; `contacts_preview` tests the actual LVGL pages.
