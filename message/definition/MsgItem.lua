-- 物品相关消息定义

messages = {
    -- 物品信息消息
    ItemInfo = {
        category = 3,   -- 物品类别
        id = 1,         -- 消息ID
        desc = "物品基本信息",
        version = 1,    -- 版本号
        fields = {
            item_id = {
                type = "uint32",
                desc = "物品唯一ID",
                required = true
            },
            template_id = {
                type = "uint32",
                desc = "物品模板ID",
                required = true
            },
            count = {
                type = "uint32",
                desc = "数量",
                required = true,
                default = "1"
            },
            quality = {
                type = "uint8",
                desc = "品质",
                required = true,
                default = "0"
            },
            bind_status = {
                type = "uint8",
                desc = "绑定状态: 0-未绑定, 1-已绑定",
                required = true,
                default = "0"
            },
            expire_time = {
                type = "uint64",
                desc = "过期时间戳（0表示永不过期）",
                required = true,
                default = "0"
            },
            attrs = {
                type = "array<string>",
                desc = "属性列表（JSON格式）",
                required = false
            }
        }
    },
    
    -- 添加物品消息
    AddItem = {
        category = 3,   -- 物品类别
        id = 2,         -- 消息ID
        desc = "服务器通知客户端添加物品",
        version = 1,    -- 版本号
        fields = {
            reason = {
                type = "uint16",
                desc = "物品获得原因",
                required = true,
                default = "0"
            },
            items = {
                type = "array<ItemInfo>",
                desc = "添加的物品列表",
                required = true
            }
        }
    },
    
    -- 移除物品消息
    RemoveItem = {
        category = 3,   -- 物品类别
        id = 3,         -- 消息ID
        desc = "服务器通知客户端移除物品",
        version = 1,    -- 版本号
        fields = {
            reason = {
                type = "uint16",
                desc = "物品移除原因",
                required = true,
                default = "0"
            },
            items = {
                type = "array<uint32>",
                desc = "移除的物品ID列表",
                required = true
            }
        }
    },
    
    -- 使用物品请求
    UseItemRequest = {
        category = 3,   -- 物品类别
        id = 4,         -- 消息ID
        desc = "客户端请求使用物品",
        version = 1,    -- 版本号
        fields = {
            item_id = {
                type = "uint32",
                desc = "物品唯一ID",
                required = true
            },
            count = {
                type = "uint32",
                desc = "使用数量",
                required = true,
                default = "1"
            },
            target_id = {
                type = "uint64",
                desc = "目标ID（如果有）",
                required = false,
                default = "0"
            },
            extra_data = {
                type = "string",
                desc = "额外数据（JSON格式）",
                required = false,
                default = "{}"
            }
        }
    },
    
    -- 使用物品响应
    UseItemResponse = {
        category = 3,   -- 物品类别
        id = 5,         -- 消息ID
        desc = "服务器对使用物品请求的响应",
        version = 1,    -- 版本号
        fields = {
            result = {
                type = "int32",
                desc = "结果: 0-成功, 非0-失败码",
                required = true
            },
            message = {
                type = "string",
                desc = "结果描述",
                required = false,
                default = ""
            },
            item_id = {
                type = "uint32",
                desc = "物品唯一ID",
                required = true
            },
            remain_count = {
                type = "uint32",
                desc = "物品剩余数量",
                required = true,
                default = "0"
            },
            rewards = {
                type = "string",
                desc = "使用物品后获得的奖励（JSON格式）",
                required = false,
                default = "{}"
            }
        }
    }
}

return messages
