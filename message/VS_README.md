# NextGen消息系统 - Visual Studio 2022解决方案

这个解决方案文件提供了直接在Visual Studio 2022中编译和运行NextGen消息系统的能力，无需使用CMake。

## 项目结构

解决方案包含以下项目：

1. **MessageCore** - 核心消息基类和类型定义的静态库
   - 包含消息基类、工厂、类型定义等
   - 提供消息系统的基础功能

2. **MessageGenerator** - 消息生成器的静态库
   - 包含模板引擎和代码生成功能
   - 负责从Lua定义生成C++代码

3. **MsgGen** - 消息生成命令行工具（可执行文件）
   - 使用MessageGenerator库实现消息代码生成
   - 可以独立运行，用于生成消息代码

4. **Messages** - 生成的消息类的静态库
   - 包含从Lua定义生成的所有消息类
   - 构建时会自动调用MsgGen工具生成代码

## 使用方法

### 构建解决方案

1. 在Visual Studio 2022中打开 `NextGenMessage.sln`
2. 选择配置（Debug或Release）
3. 点击"生成解决方案"

构建过程会自动：
- 编译MessageCore库
- 编译MessageGenerator库
- 编译MsgGen工具
- 使用MsgGen工具生成消息文件
- 编译生成的消息文件到Messages库

### 依赖项

注意：解决方案依赖于以下外部库：

- **Lua 5.1** - 用于解析消息定义
  - 需要在系统中安装Lua 5.1库，或者修改项目设置指向自定义Lua库位置

### 使用MsgGen工具手动生成消息

构建完成后，可以手动运行MsgGen工具：

```
.\bin\Debug\MsgGen.exe -i .\definition -o .\generated -t .\generator\templates -v
```

参数说明：
- `-i` - 输入目录，包含Lua消息定义
- `-o` - 输出目录，生成的C++文件
- `-t` - 模板目录
- `-v` - 启用详细输出
- `-f` - 仅处理指定的Lua文件（可选）
- `-h` - 显示帮助信息

### 添加新的消息定义

1. 在 `definition` 目录下创建新的Lua文件
2. 按照现有文件的格式定义消息
3. 重新构建解决方案或手动运行MsgGen工具

## 与Phase Two集成

这个消息系统设计为与Phase Two中已实现的组件无缝集成：

- 可以与服务基类（BaseService）的模块系统集成
- 可以利用无锁消息队列实现高性能消息传递
- 支持与UDP服务和会话管理系统集成
- 兼容现有的计时器系统

## 注意事项

- 确保Lua库正确安装并在链接设置中配置
- 生成的代码位于 `generated` 目录，不要手动修改这些文件
- 修改消息定义后需要重新构建Messages项目
