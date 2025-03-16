# NextGen 应用层消息系统

NextGen框架的应用层消息系统提供了一种灵活、高效的方式来定义、生成和处理游戏内部通信消息。这个系统完全独立于网络层消息系统，专注于应用逻辑层面的消息传递。

## 设计理念

- **清晰的关注点分离**：网络层负责消息的传输和会话管理，应用层负责具体业务逻辑
- **类型安全**：所有消息字段都是强类型的，编译时即可发现类型错误
- **自动生成**：从Lua定义自动生成C++代码，减少重复劳动
- **向后兼容**：提供与旧消息系统的兼容层，确保平滑过渡
- **高性能**：优化的序列化和反序列化过程，尽量减少内存分配

## 目录结构

```
message/
├── definition/           # 消息定义（Lua格式）
├── generated/            # 自动生成的C++消息类（由生成器生成）
├── generator/            # 消息生成器
│   ├── templates/        # 代码生成模板
│   ├── generator.h       # 生成器接口
│   ├── generator.cpp     # 生成器实现
│   └── template_engine.h # 模板引擎
├── include/              # 公共头文件
│   ├── message_base.h    # 消息基类
│   ├── message_factory.h # 消息工厂
│   ├── types.h           # 类型定义
│   └── legacy/           # 旧系统兼容层
├── tools/                # 工具和实用程序
│   └── msggen.cpp        # 消息生成命令行工具
└── CMakeLists.txt        # 构建配置
```

## 使用指南

### 1. 定义消息

在 `definition/` 目录下创建 Lua 文件来定义消息。例如 `MsgLogin.lua`：

```lua
-- 登录相关消息定义
messages = {
    -- 登录请求消息
    LoginRequest = {
        category = 8,   -- 登录类别
        id = 1,         -- 消息ID
        desc = "客户端向服务器发送的登录请求",
        version = 1,    -- 版本号
        fields = {
            account = {
                type = "string",
                desc = "账号名称",
                required = true
            },
            password = {
                type = "string",
                desc = "密码（已加密）",
                required = true
            }
            -- 其他字段...
        }
    },
    -- 其他消息...
}

return messages
```

### 2. 生成消息类

使用消息生成工具从定义生成C++代码：

```bash
# 生成所有消息
./msggen -i path/to/definition -o path/to/generated -t path/to/templates

# 只生成单个消息文件
./msggen -f MsgLogin.lua -o path/to/generated
```

或者使用CMake目标：

```bash
cmake --build . --target generate_messages
```

### 3. 使用生成的消息类

```cpp
#include "message/generated/LoginRequestMessage.h"
#include "message/include/message_factory.h"

// 创建消息
auto login_request = std::make_unique<next_gen::message::LoginRequestMessage>();
login_request->setAccount("user123");
login_request->setPassword("encrypted_password");
login_request->setClientVersion("1.0.0");

// 序列化消息
next_gen::ByteStream stream;
login_request->serialize(stream);

// 反序列化消息
auto factory = next_gen::message::DefaultMessageFactory::instance();
auto msg = factory.createMessage(8, 1);  // 创建登录请求消息
if (msg) {
    msg->deserialize(stream);
    
    // 类型转换
    auto login_msg = dynamic_cast<next_gen::message::LoginRequestMessage*>(msg.get());
    if (login_msg) {
        std::cout << "Account: " << login_msg->getAccount() << std::endl;
    }
}
```

### 4. 处理消息

```cpp
#include "message/include/message_handler.h"

// 定义消息处理器
class LoginHandler : public next_gen::message::MessageHandler {
public:
    LoginHandler() {}
    
    // 实现消息处理方法
    bool handleMessage(const next_gen::message::MessageBase& message) override {
        // 检查消息类型
        if (message.getCategory() != 8 || message.getId() != 1) {
            return false;
        }
        
        // 转换为具体消息类型
        auto& login_msg = static_cast<const next_gen::message::LoginRequestMessage&>(message);
        
        // 处理消息
        std::cout << "Processing login request for account: " << login_msg.getAccount() << std::endl;
        
        // 创建响应
        auto response = std::make_unique<next_gen::message::LoginResponseMessage>();
        response->setResult(0);  // 0表示成功
        response->setToken("session_token_123");
        response->setServerTime(std::time(nullptr));
        
        // 发送响应
        // ...
        
        return true;
    }
    
    // 实现其他接口方法
    std::string getName() const override { return "LoginHandler"; }
    MessageCategoryType getCategory() const override { return 8; }
    MessageIdType getId() const override { return 1; }
};
```

### 5. 与旧系统集成

使用兼容层适配器将旧系统消息转换为新系统消息，反之亦然：

```cpp
#include "message/include/legacy/adapter.h"

// 从旧格式转换为新格式
void* old_msg_ptr = /* 从旧系统获取的消息指针 */;
auto new_msg = next_gen::message::legacy::MessageAdapter::fromLegacyFormat(old_msg_ptr, 8, 1);

// 从新格式转换为旧格式
auto new_msg = std::make_unique<next_gen::message::LoginRequestMessage>();
// 设置消息内容...
void* old_msg_ptr = next_gen::message::legacy::MessageAdapter::toLegacyFormat(*new_msg);

// 注册旧系统消息处理器
auto handler = std::make_unique<LegacyLoginHandler>();  // 旧系统处理器
auto adapter = next_gen::message::legacy::createLegacyHandler(
    "LegacyLoginHandler", 8, 1, 
    [handler](void* msg) -> bool { return handler->process(msg); }
);
```

## 扩展和自定义

### 添加新的消息类型

1. 在 `definition/` 目录下创建新的 Lua 文件或修改现有文件
2. 运行消息生成工具重新生成代码
3. 重新编译项目

### 修改模板

如果需要修改生成的代码风格或添加新功能，可以编辑 `generator/templates/` 目录下的模板文件：

- `message_header.template`: 消息头文件模板
- `message_source.template`: 消息源文件模板
- `factory_registration.template`: 工厂注册模板
- `legacy_adapters.template`: 兼容层适配器模板

## 性能注意事项

- 对于频繁发送的消息，建议预分配内存以减少动态分配
- 考虑使用对象池来管理消息实例，减少内存分配和释放的开销
- 对于大型数组字段，序列化和反序列化可能是性能瓶颈，考虑使用自定义的优化策略

## 与网络层集成

应用层消息系统与网络层是解耦的，但它们可以无缝集成：

```cpp
// 网络层收到消息后转发给应用层
void NetworkSession::onMessage(const NetworkMessage& net_msg) {
    // 从网络消息中提取类别和ID
    MessageCategoryType category = net_msg.getHeader().category;
    MessageIdType id = net_msg.getHeader().id;
    
    // 创建应用层消息
    auto factory = next_gen::message::DefaultMessageFactory::instance();
    auto app_msg = factory.createMessage(category, id);
    
    if (app_msg) {
        // 从网络消息中提取数据并反序列化
        ByteStream stream(net_msg.getData(), net_msg.getSize());
        app_msg->deserialize(stream);
        
        // 设置会话ID和时间戳
        app_msg->setSessionId(getSessionId());
        app_msg->setTimestamp(std::time(nullptr));
        
        // 分发消息给应用层处理器
        message_dispatcher_.dispatchMessage(*app_msg);
    }
}
```

## 贡献

欢迎对NextGen消息系统提交改进和扩展。请遵循项目的代码风格和贡献指南。
