-- ============================================================
-- charge-station-dgttz 建库脚本 schema.sql  (v1)
-- 依据: docs/content/spec-数据库.md / DATA_STRUCTURE.md (冻结真相源)
-- 位置: server/sql/schema.sql
-- 用法: sqlite3 charge.db < schema.sql   或服务端启动时执行
-- 注意: SQLite 外键默认不生效, 每条连接需先执行 PRAGMA foreign_keys=ON;
--       (Qt 中: query.exec("PRAGMA foreign_keys=ON");)
-- 约定: 时间 TEXT 'YYYY-MM-DD HH:MM:SS'; JSON 存 TEXT; 布尔 INTEGER 0/1
-- ============================================================

PRAGMA foreign_keys = ON;

-- ---------- 核心表 (16) ----------

-- 1. user 用户
CREATE TABLE IF NOT EXISTS user (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    phone           TEXT    NOT NULL UNIQUE,            -- 11位手机号
    nickname        TEXT    NOT NULL DEFAULT '',        -- 默认"用户+后四位", 由服务端生成
    avatar_path     TEXT    NOT NULL DEFAULT '',        -- 空=灰色占位图
    balance         REAL    NOT NULL DEFAULT 0,         -- 余额(元)
    points          INTEGER NOT NULL DEFAULT 0,         -- 可用积分
    level           TEXT    NOT NULL DEFAULT 'normal'
                    CHECK (level IN ('normal','vip','enterprise')),
    status          TEXT    NOT NULL DEFAULT 'normal'
                    CHECK (status IN ('normal','frozen')),
    register_time   TEXT    NOT NULL DEFAULT (datetime('now','localtime')),
    last_login_time TEXT                                 -- 可空
);

-- 2. admin 管理员
CREATE TABLE IF NOT EXISTS admin (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    account  TEXT NOT NULL UNIQUE,
    password TEXT NOT NULL
);

-- 3. merchant 商户/合作站点 (被 station 外键引用, 需先建)
CREATE TABLE IF NOT EXISTS merchant (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    name             TEXT NOT NULL,
    contact_name     TEXT NOT NULL DEFAULT '',
    contact_phone    TEXT NOT NULL DEFAULT '',
    cooperation_type TEXT NOT NULL DEFAULT 'partner'
                     CHECK (cooperation_type IN ('franchise','partner','third_party')),
    status           TEXT NOT NULL DEFAULT 'active'
                     CHECK (status IN ('active','paused')),
    order_count      INTEGER NOT NULL DEFAULT 0,        -- 汇总, 定时刷新
    settle_amount    REAL    NOT NULL DEFAULT 0,        -- 累计结算金额(元)
    service_score    REAL    NOT NULL DEFAULT 0,        -- 评分均值 0-5
    remark           TEXT    NOT NULL DEFAULT '',
    create_time      TEXT    NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 4. station 充电站
CREATE TABLE IF NOT EXISTS station (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT NOT NULL,
    address        TEXT NOT NULL DEFAULT '',
    area           TEXT NOT NULL DEFAULT '',            -- 区域(区县/商圈)
    longitude      REAL NOT NULL DEFAULT 0,
    latitude       REAL NOT NULL DEFAULT 0,
    total_chargers INTEGER NOT NULL DEFAULT 0,
    online_rate    REAL NOT NULL DEFAULT 100,           -- 在线率%, 服务端动态计算
    service_fee    REAL NOT NULL DEFAULT 0,             -- 服务费(元/kWh)
    parking_fee    REAL NOT NULL DEFAULT 0,             -- 停车费(元/小时, 0=免费)
    business_hours TEXT NOT NULL DEFAULT '00:00-24:00',
    facilities     TEXT NOT NULL DEFAULT '[]',          -- JSON数组
    owner_type     TEXT NOT NULL DEFAULT 'self_run'
                   CHECK (owner_type IN ('self_run','franchise','partner','third_party')),
    merchant_id    INTEGER REFERENCES merchant(id),     -- 自营为空
    has_swap       INTEGER NOT NULL DEFAULT 0           -- 是否支持换电 0/1
);

-- 5. charger 充电桩
CREATE TABLE IF NOT EXISTS charger (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    code                  TEXT NOT NULL,                -- 站内编号 如 A-023
    station_id            INTEGER NOT NULL REFERENCES station(id),
    type                  TEXT NOT NULL DEFAULT 'fast'
                          CHECK (type IN ('fast','slow')),
    power                 REAL NOT NULL DEFAULT 60,     -- 额定功率 kW
    status                TEXT NOT NULL DEFAULT 'idle'
                          CHECK (status IN ('idle','charging','reserved','fault','offline','rebooting')),
    voltage               REAL NOT NULL DEFAULT 0,      -- 当前电压 V
    current               REAL NOT NULL DEFAULT 0,      -- 当前电流 A
    temperature           REAL NOT NULL DEFAULT 25,     -- 当前温度 ℃
    fault_code            TEXT NOT NULL DEFAULT '',     -- 空=正常
    comm_status           TEXT NOT NULL DEFAULT 'normal'
                          CHECK (comm_status IN ('normal','abnormal')),
    health_score          INTEGER NOT NULL DEFAULT 100, -- 0-100
    total_charge_count    INTEGER NOT NULL DEFAULT 0,
    total_charge_duration INTEGER NOT NULL DEFAULT 0,   -- 分钟
    created_time          TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (station_id, code)                           -- 编号站内唯一
);

-- 6. vehicle 我的车辆 (被 charging_order 引用, 先建)
CREATE TABLE IF NOT EXISTS vehicle (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id        INTEGER NOT NULL REFERENCES user(id),
    name           TEXT NOT NULL DEFAULT '',
    type           TEXT NOT NULL DEFAULT 'car'
                   CHECK (type IN ('car','light_truck','two_wheeler','three_wheeler')),
    battery_kwh    REAL NOT NULL DEFAULT 60,
    connector_type TEXT NOT NULL DEFAULT 'dc_gb'
                   CHECK (connector_type IN ('ac_gb','dc_gb','other')),
    max_power_kw   REAL NOT NULL DEFAULT 120,
    is_default     INTEGER NOT NULL DEFAULT 0,          -- 每用户至多一辆, 业务层保证
    created_time   TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 7. coupon 优惠券模板 (被 user_coupon 引用, 先建)
CREATE TABLE IF NOT EXISTS coupon (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    title           TEXT NOT NULL,
    type            TEXT NOT NULL DEFAULT 'full_reduction'
                    CHECK (type IN ('new_user','full_reduction','night','site')),
    discount_amount REAL NOT NULL DEFAULT 0,
    min_amount      REAL NOT NULL DEFAULT 0,            -- 0=无门槛
    station_id      INTEGER REFERENCES station(id),     -- 空=通用
    time_range      TEXT NOT NULL DEFAULT '',           -- 空=不限
    valid_days      INTEGER NOT NULL DEFAULT 30,
    total           INTEGER NOT NULL DEFAULT -1,        -- -1 不限量
    status          TEXT NOT NULL DEFAULT 'active'
                    CHECK (status IN ('active','inactive')),
    create_time     TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 8. user_coupon 用户持有券
-- 注: user_coupon 与 charging_order 互相引用(核销闭环), used_order_id 不加外键约束以避免建表循环, 由业务层保证
CREATE TABLE IF NOT EXISTS user_coupon (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL REFERENCES user(id),
    coupon_id     INTEGER NOT NULL REFERENCES coupon(id),
    status        TEXT NOT NULL DEFAULT 'unused'
                  CHECK (status IN ('unused','used','expired')),
    receive_time  TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    used_order_id INTEGER                               -- 可空, 逻辑指向 charging_order.id
);

-- 9. charging_order 充电订单
CREATE TABLE IF NOT EXISTS charging_order (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id         INTEGER NOT NULL REFERENCES user(id),
    station_id      INTEGER NOT NULL REFERENCES station(id),
    charger_id      INTEGER NOT NULL REFERENCES charger(id),
    vehicle_id      INTEGER REFERENCES vehicle(id),     -- 可空
    status          TEXT NOT NULL DEFAULT 'reserved'
                    CHECK (status IN ('reserved','charging','pending_settle','completed','cancelled')),
    start_soc       REAL,                               -- 开始充电时写入
    target_soc      REAL,
    end_soc         REAL,
    start_time      TEXT,
    end_time        TEXT,
    duration_min    INTEGER NOT NULL DEFAULT 0,
    energy_kwh      REAL NOT NULL DEFAULT 0,
    price_level     TEXT DEFAULT 'flat'
                    CHECK (price_level IN ('valley','flat','peak')),
    amount          REAL NOT NULL DEFAULT 0,            -- 应收
    discount_amount REAL NOT NULL DEFAULT 0,            -- 券+积分抵扣合计
    pay_amount      REAL NOT NULL DEFAULT 0,            -- 实付 = amount - discount_amount
    points_used     INTEGER NOT NULL DEFAULT 0,
    coupon_id       INTEGER REFERENCES user_coupon(id), -- 核销券, 可空
    points_earned   INTEGER NOT NULL DEFAULT 0,
    create_time     TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    settle_time     TEXT                                -- 可空
);

-- 10. reservation 预约/排队
CREATE TABLE IF NOT EXISTS reservation (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id             INTEGER NOT NULL REFERENCES user(id),
    station_id          INTEGER NOT NULL REFERENCES station(id),
    charger_id          INTEGER REFERENCES charger(id), -- 轮到时回填, 可空
    queue_no            INTEGER NOT NULL DEFAULT 0,     -- 站内排队序号
    reserve_time        TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    estimate_start_time TEXT,
    notified            INTEGER NOT NULL DEFAULT 0,     -- 防重复推送
    status              TEXT NOT NULL DEFAULT 'waiting'
                        CHECK (status IN ('waiting','matched','cancelled'))
);

-- 11. price_rule 分时电价
CREATE TABLE IF NOT EXISTS price_rule (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER REFERENCES station(id),          -- 空=全站通用
    level      TEXT NOT NULL CHECK (level IN ('valley','flat','peak')),
    price      REAL NOT NULL,                           -- 元/kWh
    time_range TEXT NOT NULL                            -- 如 "00:00-08:00"
);

-- 12. review 站点评价
CREATE TABLE IF NOT EXISTS review (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL REFERENCES user(id),
    station_id    INTEGER NOT NULL REFERENCES station(id),
    order_id      INTEGER REFERENCES charging_order(id),-- 可空
    overall_score REAL NOT NULL DEFAULT 5,
    speed_score   REAL NOT NULL DEFAULT 5,
    device_score  REAL NOT NULL DEFAULT 5,
    parking_score REAL NOT NULL DEFAULT 5,
    hygiene_score REAL NOT NULL DEFAULT 5,
    service_score REAL NOT NULL DEFAULT 5,
    tags          TEXT NOT NULL DEFAULT '[]',           -- JSON数组
    content       TEXT NOT NULL DEFAULT '',
    useful_count  INTEGER NOT NULL DEFAULT 0,
    status        TEXT NOT NULL DEFAULT 'normal'
                  CHECK (status IN ('normal','hidden')),
    create_time   TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 13. work_order 工单
CREATE TABLE IF NOT EXISTS work_order (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    type        TEXT NOT NULL
                CHECK (type IN ('user_complaint','device_fault','refund','maintenance','abnormal_order')),
    priority    TEXT NOT NULL DEFAULT 'medium'
                CHECK (priority IN ('low','medium','high')),
    user_id     INTEGER REFERENCES user(id),            -- 设备类可空
    station_id  INTEGER REFERENCES station(id),         -- 可空
    charger_id  INTEGER REFERENCES charger(id),         -- 可空
    title       TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'pending'
                CHECK (status IN ('pending','processing','completed','closed')),
    handler     TEXT NOT NULL DEFAULT '',               -- 处理管理员账号
    result      TEXT NOT NULL DEFAULT '',
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    handle_time TEXT                                    -- 可空
);

-- 14. device_log 设备操作日志
CREATE TABLE IF NOT EXISTS device_log (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    charger_id INTEGER NOT NULL REFERENCES charger(id),
    action     TEXT NOT NULL CHECK (action IN ('restart','pause','repair')),
    operator   TEXT NOT NULL DEFAULT '',                -- 管理员账号
    op_time    TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    result     TEXT NOT NULL DEFAULT 'success'
               CHECK (result IN ('success','failed'))
);

-- 15. alarm 告警
CREATE TABLE IF NOT EXISTS alarm (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    charger_id    INTEGER REFERENCES charger(id),       -- 空=站点级告警
    station_id    INTEGER NOT NULL REFERENCES station(id),
    type          TEXT NOT NULL
                  CHECK (type IN ('comm_abnormal','overheat','power_drop','offline','user_behavior')),
    level         TEXT NOT NULL DEFAULT 'warning'
                  CHECK (level IN ('info','warning','critical')),
    occur_time    TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    status        TEXT NOT NULL DEFAULT 'open'
                  CHECK (status IN ('open','handled')),
    handle_action TEXT DEFAULT ''
                  CHECK (handle_action IN ('','restart','pause','repair','ignore'))
);

-- 16. point_record 积分记录
CREATE TABLE IF NOT EXISTS point_record (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL REFERENCES user(id),
    change      INTEGER NOT NULL,                       -- 正获得/负抵扣
    reason      TEXT NOT NULL CHECK (reason IN ('charge','redeem')),
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 17. charging_measure 充电时序测量 (ML 数据源, 量大)
CREATE TABLE IF NOT EXISTS charging_measure (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    charger_id       INTEGER NOT NULL REFERENCES charger(id),
    station_id       INTEGER NOT NULL REFERENCES station(id),
    measure_time     TEXT NOT NULL,
    power_kw         REAL NOT NULL DEFAULT 0,
    soc              REAL NOT NULL DEFAULT 0,
    energy_delta_kwh REAL NOT NULL DEFAULT 0,
    temperature      REAL                                -- 可空
);

-- ---------- 支撑表 (5) ----------

-- 18. order_timeline 订单时间轴
CREATE TABLE IF NOT EXISTS order_timeline (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    order_id   INTEGER NOT NULL REFERENCES charging_order(id),
    node       TEXT NOT NULL
               CHECK (node IN ('reserved','arrived','started','soc_50','target_reached','finished','settled')),
    label      TEXT NOT NULL DEFAULT '',
    event_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    detail     TEXT                                     -- 可空
);

-- 19. wallet_transaction 钱包流水
CREATE TABLE IF NOT EXISTS wallet_transaction (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL REFERENCES user(id),
    type          TEXT NOT NULL
                  CHECK (type IN ('recharge','consume','refund','other')),
    amount        REAL NOT NULL,                        -- 正入账/负支出
    balance_after REAL NOT NULL,
    order_id      INTEGER REFERENCES charging_order(id),-- 可空
    remark        TEXT NOT NULL DEFAULT '',
    create_time   TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 20. notification 站内消息
CREATE TABLE IF NOT EXISTS notification (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL REFERENCES user(id),
    type        TEXT NOT NULL
                CHECK (type IN ('reservation','order','point','coupon','work_order','system')),
    title       TEXT NOT NULL DEFAULT '',
    content     TEXT NOT NULL DEFAULT '',
    related_id  INTEGER,                                -- 关联业务ID, 可空
    is_read     INTEGER NOT NULL DEFAULT 0,
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 21. weather 模拟天气
CREATE TABLE IF NOT EXISTS weather (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    area        TEXT NOT NULL DEFAULT '',               -- 空=全局默认
    condition   TEXT NOT NULL DEFAULT 'sunny'
                CHECK (condition IN ('sunny','cloudy','rain','hot','extreme')),
    temperature REAL NOT NULL DEFAULT 25,
    forecast    TEXT,                                   -- JSON数组, 可空
    update_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 22. holiday 节假日
CREATE TABLE IF NOT EXISTS holiday (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    date        TEXT NOT NULL UNIQUE,                   -- YYYY-MM-DD
    name        TEXT NOT NULL,
    is_workday  INTEGER NOT NULL DEFAULT 0,             -- 0休息日/1调休上班
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- ---------- 扩展表 (其余9, merchant已建) ----------

-- 23. rule_config 规则引擎配置
CREATE TABLE IF NOT EXISTS rule_config (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT NOT NULL,
    object_type   TEXT NOT NULL
                  CHECK (object_type IN ('station','charger','user')),
    condition     TEXT NOT NULL,                        -- JSON
    action        TEXT NOT NULL
                  CHECK (action IN ('redirect','issue_coupon','create_alarm','create_work_order','limit_reservation')),
    action_params TEXT,                                 -- JSON, 可空
    enabled       INTEGER NOT NULL DEFAULT 1,
    priority      INTEGER NOT NULL DEFAULT 0,           -- 高→低
    description   TEXT NOT NULL DEFAULT '',
    create_time   TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    update_time   TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 24. member_plan 会员套餐模板
CREATE TABLE IF NOT EXISTS member_plan (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    name                 TEXT NOT NULL,
    price                REAL NOT NULL DEFAULT 0,
    valid_days           INTEGER NOT NULL DEFAULT 30,
    service_fee_discount REAL NOT NULL DEFAULT 1,       -- 0.8=8折
    night_discount       REAL DEFAULT 1,                -- 可空
    points_multiplier    REAL DEFAULT 1,                -- 可空
    status               TEXT NOT NULL DEFAULT 'active'
                         CHECK (status IN ('active','inactive')),
    description          TEXT NOT NULL DEFAULT '',
    create_time          TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 25. user_plan 用户套餐订阅
CREATE TABLE IF NOT EXISTS user_plan (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL REFERENCES user(id),
    plan_id     INTEGER NOT NULL REFERENCES member_plan(id),
    start_time  TEXT NOT NULL,
    end_time    TEXT NOT NULL,
    status      TEXT NOT NULL DEFAULT 'active'
                CHECK (status IN ('active','expired','cancelled')),
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 26. station_post 站点社区帖
CREATE TABLE IF NOT EXISTS station_post (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id  INTEGER NOT NULL REFERENCES station(id),
    user_id     INTEGER NOT NULL REFERENCES user(id),
    content     TEXT NOT NULL DEFAULT '',
    like_count  INTEGER NOT NULL DEFAULT 0,
    reply_count INTEGER NOT NULL DEFAULT 0,
    status      TEXT NOT NULL DEFAULT 'normal'
                CHECK (status IN ('normal','hidden')),
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 27. post_reply 帖子回复
CREATE TABLE IF NOT EXISTS post_reply (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    post_id     INTEGER NOT NULL REFERENCES station_post(id),
    user_id     INTEGER NOT NULL REFERENCES user(id),
    parent_id   INTEGER NOT NULL DEFAULT 0,             -- 0=直接回复帖子
    content     TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'normal'
                CHECK (status IN ('normal','hidden')),
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 28. favorite 站点收藏
CREATE TABLE IF NOT EXISTS favorite (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL REFERENCES user(id),
    station_id  INTEGER NOT NULL REFERENCES station(id),
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (user_id, station_id)                        -- 同一站不能重复收藏
);

-- 29. faq 客服机器人问答库
CREATE TABLE IF NOT EXISTS faq (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    category    TEXT NOT NULL DEFAULT '',
    question    TEXT NOT NULL,
    answer      TEXT NOT NULL,
    sort        INTEGER NOT NULL DEFAULT 0,
    enabled     INTEGER NOT NULL DEFAULT 1,
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- 30. battery 换电电池包
CREATE TABLE IF NOT EXISTS battery (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    code         TEXT NOT NULL,
    station_id   INTEGER NOT NULL REFERENCES station(id),
    soc          REAL NOT NULL DEFAULT 100,
    health_score INTEGER NOT NULL DEFAULT 100,
    temperature  REAL NOT NULL DEFAULT 25,
    status       TEXT NOT NULL DEFAULT 'idle'
                 CHECK (status IN ('idle','swapping','charging','fault','offline')),
    swap_count   INTEGER NOT NULL DEFAULT 0,
    create_time  TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (station_id, code)                           -- 编号站内唯一
);

-- 31. swap_order 换电订单
CREATE TABLE IF NOT EXISTS swap_order (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id        INTEGER NOT NULL REFERENCES user(id),
    station_id     INTEGER NOT NULL REFERENCES station(id),
    battery_in_id  INTEGER REFERENCES battery(id),      -- 归还的旧电池
    battery_out_id INTEGER REFERENCES battery(id),      -- 换入的新电池
    amount         REAL NOT NULL DEFAULT 0,
    status         TEXT NOT NULL DEFAULT 'completed'
                   CHECK (status IN ('completed','cancelled')),
    create_time    TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- ---------- 索引 (高频查询路径) ----------

CREATE INDEX IF NOT EXISTS idx_charger_station        ON charger(station_id);
CREATE INDEX IF NOT EXISTS idx_charger_status         ON charger(status);
CREATE INDEX IF NOT EXISTS idx_order_user             ON charging_order(user_id, status);
CREATE INDEX IF NOT EXISTS idx_order_station_time     ON charging_order(station_id, create_time);
CREATE INDEX IF NOT EXISTS idx_order_settle_time      ON charging_order(settle_time);
CREATE INDEX IF NOT EXISTS idx_reservation_station    ON reservation(station_id, status);
CREATE INDEX IF NOT EXISTS idx_measure_charger_time   ON charging_measure(charger_id, measure_time);
CREATE INDEX IF NOT EXISTS idx_measure_station_time   ON charging_measure(station_id, measure_time);
CREATE INDEX IF NOT EXISTS idx_timeline_order         ON order_timeline(order_id);
CREATE INDEX IF NOT EXISTS idx_wallet_user            ON wallet_transaction(user_id, create_time);
CREATE INDEX IF NOT EXISTS idx_notification_user      ON notification(user_id, is_read);
CREATE INDEX IF NOT EXISTS idx_alarm_station          ON alarm(station_id, status);
CREATE INDEX IF NOT EXISTS idx_review_station         ON review(station_id, status);
CREATE INDEX IF NOT EXISTS idx_point_user             ON point_record(user_id);
CREATE INDEX IF NOT EXISTS idx_user_coupon_user       ON user_coupon(user_id, status);
CREATE INDEX IF NOT EXISTS idx_work_order_status      ON work_order(status, priority);
CREATE INDEX IF NOT EXISTS idx_device_log_charger     ON device_log(charger_id);
CREATE INDEX IF NOT EXISTS idx_favorite_user          ON favorite(user_id);
