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
            },
            client_version = {
                type = "string",
                desc = "客户端版本号",
                required = true
            },
            device_info = {
                type = "string",
                desc = "设备信息",
                required = false,
                default = ""
            },
            login_ip = {
                type = "string",
                desc = "登录IP地址",
                required = false,
                default = "0.0.0.0"
            }
        }
    },
    
    -- 登录响应消息
    LoginResponse = {
        category = 8,   -- 登录类别
        id = 2,         -- 消息ID
        desc = "服务器对登录请求的响应",
        version = 1,    -- 版本号
        fields = {
            result = {
                type = "int32",
                desc = "登录结果: 0-成功, 非0-失败码",
                required = true
            },
            error_msg = {
                type = "string",
                desc = "错误信息（如果登录失败）",
                required = false,
                default = ""
            },
            token = {
                type = "string",
                desc = "登录成功后的会话令牌",
                required = false,
                default = ""
            },
            server_time = {
                type = "uint64",
                desc = "服务器当前时间戳",
                required = true
            },
            account_info = {
                type = "string", -- 在实际应用中可能是另一个消息类型
                desc = "账号信息（JSON格式）",
                required = false,
                default = "{}"
            },
            server_list = {
                type = "array<string>",
                desc = "可用服务器列表",
                required = false
            }
        }
    },
    
    -- 心跳消息
    Heartbeat = {
        category = 8,   -- 登录类别
        id = 3,         -- 消息ID
        desc = "客户端定期发送的心跳消息",
        version = 1,    -- 版本号
        fields = {
            client_time = {
                type = "uint64",
                desc = "客户端当前时间戳",
                required = true
            },
            session_id = {
                type = "uint32",
                desc = "会话ID",
                required = true
            }
        }
    },
    
    -- 心跳响应消息
    HeartbeatResponse = {
        category = 8,   -- 登录类别
        id = 4,         -- 消息ID
        desc = "服务器对心跳消息的响应",
        version = 1,    -- 版本号
        fields = {
            server_time = {
                type = "uint64",
                desc = "服务器当前时间戳",
                required = true
            }
        }
    }
}

return messages
