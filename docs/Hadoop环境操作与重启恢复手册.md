# Hadoop 大数据环境：操作与重启恢复手册

> 适用环境：CentOS 7 (3.10.0-1160.el7)，用户 `hadoop`，虚拟机 `node100`（IP `192.168.176.100`）
> 本文覆盖「整套大数据演示怎么跑起来」「关机重启后怎么一键恢复到可演示状态」「关键命令释义」，方便组员复制操作。

---

## 0. 从零开始：只有 Windows 仓库 → 完整的 Hadoop 演示（组员必读）

> 老师发的镜像是同一套，但**如果你 clone 仓库时虚拟机还是空白**（没做过任何迁移），走这一章。
> 已经有人按此搭建过一次的，跳过本章，直接看「第 3 章 一次性启动流程」。

### 0.1 两条机器各自的"料"

| 位置 | 有什么 | 要干什么 |
| --- | --- | --- |
| **Windows 仓库** | `server/sql/hive_sync/*.sql+tsv`、`screen/`、`_export_hive.py`、`charge.db` | 提供造数据脚本 + 演示文件 |
| **CentOS 虚拟机** | Java/Hadoop/Hive/Python/MySQL | 装/启动这套平台 + 跑演示 |

### 0.2 检查虚拟机依赖够不够（30 秒摸底）

SSH 登录后跑这段，逐条确认**结果不是空的**：

```bash
java -version 2>&1 | head -1        # 需 1.8.x
echo "JH=$JAVA_HOME HD=$HADOOP_HOME HIVE=$HIVE_HOME"
which hadoop hive beeline python3    # 需都能输出路径
python3 --version                   # 需 3.x
mysqladmin -uroot -p123456 ping 2>/dev/null | grep -o alive || echo "MySQL需确认密码"
```

**预期**（和正确环境一致）：
```
java version "1.8.0_144"
JH=/opt/module/jdk1.8.0_144 HD=/opt/module/hadoop-3.3.0 ...
/opt/module/hadoop-3.3.0/bin/hadoop
/opt/module/apache-hive-2.1.1-bin/bin/hive
~/.local/bin/beeline
/usr/bin/python3
Python 3.10.X
alive
```

**如果哪条是空的**，见下表补：

| 缺的东西 | 补救 | 备注 |
| --- | --- | --- |
| `java` / `JAVA_HOME` | 老师镜像一般自带，确认 `/opt/module/jdk1.8.0_144` 存在且 `export JAVA_HOME=...` | 缺了要先装 JDK8 |
| `hadoop` / `HADOOP_HOME` | 确认 `/opt/module/hadoop-3.3.0` 存在 | 缺了整套 Hadoop 都要装 |
| `beeline` | 它是 Hive 自带的，确认 `HIVE_HOME/bin` 加了 PATH | 缺了就是 Hive 没装 |
| `python3` | 用 `/usr/bin/python3`（CentOS 7 自带 python2 只有2.7，需确认有 python3） | 没有就用 `pip3 install` 装或 yum 装 |

### 0.3 首次从零部署（一次性，约 20 分钟）

依次执行（对应主流程的章节）：

1. **装/确认依赖**（0.2）→ 缺就补上。
2. **启动 Hadoop**：`start-all.sh` → `jps` 见 5 个进程。（第 3.1 节）
3. **初始化 Hive 元库到 MySQL**（**仅首次**，之后别重复跑）：
   ```bash
   beeline -u "jdbc:hive2://node100:10000" -n hadoop --silent=true -e "create database if not exists chargestation;"
   ```
   > 若报元库没初始化/`schemaTool` 错误，用 MySQL 初始化（见第 4.6 节）。**别用 `-dbType derby`**，本机是 MySQL 元库。
4. **修权限**（如 beeline 报 `impersonate`）：补 `core-site.xml` 的 proxyuser 并重启（第 4.2 节）。
5. **启动 HiveServer2**（第 3.2）。
6. **传文件**：把仓库的 `screen/` 和 `server/sql/hive_sync/` 传到 `/home/hadoop/`（Windows 上 scp，见下方）。
7. **灌数**：`bash ~/screen/load_hive.sh`（第 4.5 的建库+灌数）。
8. **起网关**：`cd ~/screen && nohup python3 app.py 8080 ...`（第 3.3）。
9. **验证**：第 3.4 的 curl 三连。

### 0.4 从 Windows 传文件到虚拟机（两种方式任选）

**方式 A：Windows 自带 scp（推荐）**
在 Windows PowerShell 跑，输密码 `hadoop`：
```powershell
scp -r "你clone的路径\screen" "你clone的路径\server\sql\hive_sync" hadoop@192.168.176.100:/home/hadoop/
```
把两份分别传到 `/home/hadoop/screen` 和 `/home/hadoop/hive_sync`。传完用 `ls ~/screen ~/hive_sync` 确认。

**方式 B：Xftp 拖拽**
在 Xftp 里从仓库把 `screen/`、`server/sql/hive_sync/` 拖到 `/home/hadoop/` 即可。（慢，但直观）

> **无论哪种，传完后 `server/sql/hive_sync/` 必须落在 `/home/hadoop/hive_sync`**，且 `screen/` 落在 `/home/hadoop/screen`，`load_hive.sh` 默认从 `$HOME/hive_sync` 读数据。

---

## 1. 这套环境装了些什么

虚拟机预置的组件都在 `/opt/module` 下：

| 组件 | 路径 | 用途 |
| --- | --- | --- |
| JDK 1.8 | `/opt/module/jdk1.8.0_144` | 所有 Java 组件运行基础 |
| Hadoop 3.3.0 | `/opt/module/hadoop-3.3.0` | HDFS 分布式文件系统 + YARN 调度 |
| Hive 2.1.1 | `/opt/module/apache-hive-2.1.1-bin` | 数据仓库，SQL 查 HDFS 数据 |
| Spark 3.4.1 | `/opt/module/spark-3.4.1` | 大计算引擎（本次未用） |
| Python 3.10.13 | `/opt/module/python` | 跑我们的演示网关 |
| MySQL（系统自带） | `/usr/sbin/mysqld` | 存 Hive 的元数据（表结构信息） |

**数据流向**：浏览器 → Python 网关 → HiveServer2 → Hive（数据在 HDFS，元数据在 MySQL）。

---

## 2. 我们做的演示系统（位于 `~/screen`）

```
/home/hadoop/screen/
├── index.html            # 充电运营数据大屏（自带 mock 数据展示）
├── app.py                # Python 网关（托管网页 + 对外查 Hive）
├── load_hive.sh          # 一键把 hive_sync 数据灌入 Hive
├── vendor/echarts.min.js # 本地 ECharts（离线可用）
└── /home/hadoop/hive_sync/   # 30 张表的 .sql(建表) + .tsv(数据)，来自 SQLite 导出
```

**三个演示入口：**
- 大屏：`http://192.168.176.100:8080/index.html`
- Hadoop 查询演示页：`http://192.168.176.100:8080/hadoop`
- Hadoop 官方 UI（加分）：HDFS `:9870`、YARN `:8088`

---

## 3. 一次性启动流程（正常状态下，开机后跑这个）

> Hadoop 和 hiveserver2 需要手动起，演示网关也要手动起。三条都用 `nohup` 挂后台，关掉 SecureCRT 也不影响。

### 3.1 启动 Hadoop（HDFS + YARN）

```bash
start-all.sh
sleep 10
jps
```

`jps` 应看到 5 个进程：`NameNode` `DataNode` `SecondaryNameNode` `ResourceManager` `NodeManager` 🚦。

### 3.2 启动 HiveServer2（查询服务，端口 10000）

```bash
nohup hiveserver2 > ~/hs2.log 2>&1 &
sleep 60          # 首次启动较慢，多等一会
netstat -tlnp 2>/dev/null | grep ':10000'
```

出现 `:::10000 LISTEN` 即成功。日志在 `~/hs2.log`。

### 3.3 启动演示网关（端口 8080）

```bash
cd ~/screen
nohup python3 app.py 8080 /home/hadoop/screen > ~/gateway.log 2>&1 &
sleep 3
netstat -tlnp 2>/dev/null | grep ':8080'
```

出现 `0.0.0.0:8080 LISTEN` 即成功。

### 3.4 快速验证三件事

```bash
echo "Hive查询:"; beeline -u "jdbc:hive2://node100:10000" -n hadoop --silent=true -e "select count(*) from \`chargestation\`.\`charger\`;"
echo "大屏:"; curl -s -o /dev/null -w "index HTTP %{http_code}\n" http://127.0.0.1:8080/index.html
echo "Hadoop页:"; curl -s -o /dev/null -w "hadoop HTTP %{http_code}\n" http://127.0.0.1:8080/hadoop
```

- Hive 查询返回 `288` 正常
- 两个页面 HTTP 200 正常

---

## 4. 常见问题排查

### 4.1 关机重启后 Kafka/RPC 连不上，`start-all.sh` 后 `jps` 进程不全
先停干净再起：
```bash
stop-all.sh
jps                      # 应只剩 Jps
start-all.sh
```

### 4.2 `beeline` 报 `User: hadoop is not allowed to impersonate ...`
原因是 `core-site.xml` 只允许 root 冒充，我们已补了 hadoop 的授权。若被覆盖/恢复成镜像，需要重新加：
```bash
grep -n 'proxyuser' /opt/module/hadoop-3.3.0/etc/hadoop/core-site.xml
```
若缺少 `hadoop.proxyuser.hadoop.hosts` / `groups`，手工补：
```
<property>
  <name>hadoop.proxyuser.hadoop.hosts</name>
  <value>*</value>
</property>
<property>
  <name>hadoop.proxyuser.hadoop.groups</name>
  <value>*</value>
</property>
```
然后 `stop-all.sh && start-all.sh`，并**重启 hiveserver2**（见 4.3）。

### 4.3 改了 Hadoop 配置后，hiveserver2 必须重启
```bash
ps -ef | grep HiveServer2 | grep -v grep | awk '{print $2}' | xargs -r kill -9
sleep 3
nohup hiveserver2 > ~/hs2.log 2>&1 &
sleep 60 && netstat -tlnp 2>/dev/null | grep ':10000'
```

### 4.4 `select count(*)` 之类卡很久
`charging_measure` 表有 **8 万行**，count 是**全表扫**，在本地 beeline 模式下会慢（可能几十秒），属正常。演示时建议选小表或限量（`limit`）查询；`/hadoop` 页的快捷查询都已选小表。

### 4.5 重新灌 Hive 数据（重来一遍）
```bash
beeline -u "jdbc:hive2://node100:10000" -n hadoop --silent=true -e "drop database chargestation cascade;"
bash ~/screen/load_hive.sh
```
> `load_hive.sh` 会重新建库建表灌数。注意它最后会逐表 count，大表慢，看到进度卡住别急。

### 4.6 Hive 元库初始化（MySQL）——仅首次搭建
本机 Hive 用 **MySQL 存元数据**（不是内置 Derby）。`hive-site.xml` 里配置：
```
jdbc:mysql://node100:3306/metastore , 用户 root, 密码 123456, 驱动 com.mysql.cj.jdbc.Driver
```
- **正常**：metastore 库已被初始化过（`mysql -uroot -p123456 -e "show tables" metastore` 非空）。此场合只建 `chargestation` 数据库即可。
- **若重建元库**（表丢了）：用 `schematool -dbType mysql -initSchema`，**别用 `-dbType derby`**（会报 1064 语法错，因为连的是 MySQL）。若报"table exists/Failed"，通常是已被初始化过，别强制重跑，直接进入数据灌入。

### 4.7 组员在本机（Windows）要有什么
**做这件事你可以用：**
- Windows 自带 **Python 3**（`python --version` 有 3.x）→ 用来跑 `_export_hive.py` 重新生成 tsv。
- Windows 自带 **scp/ssh** → 传文件（不需要额外装）。
- Git 客户端 → clone 仓库。

**生成/有这些文件就够**（都在仓库里）：
- `server/sql/_export_hive.py`（造数脚本，改了 `charge.db` 后重跑可刷新 `hive_sync/`）
- `server/sql/hive_sync/*.sql`（建表）+ `*.tsv`（数据）
- `screen/`（网关 + 大屏 + load_hive.sh）
- `charge.db`（数据源，若 clone 后没有，说明被 .gitignore 忽略了，可从组里拿或自己跑 `make_seed.py` 生成）

---

## 5. 给组员的分工交接说明

- 这套「大屏 + Hadoop 查询演示」是**一条独立、随时能跑的演示线**，数据链真实从 SQLite 迁到了 Hive（HDFS 存储、MySQL 元数据）。
- 30 张表已进 Hive 库 `chargestation`，表名与项目 SQLite 一致（`user` 是保留字，访问要写 `` `chargestation`.`user` ``）。
- 想扩展新表：在 Windows 侧 `server/sql/_export_hive.py` 重跑 → 把生成的 tsv/sql 传到 `~/hive_sync` → 重灌。
- **不要在服务器上随手改 `core-site.xml` 忘了同步 `stop-all` + 重启 hiveserver2**，否则 beeline 连不上。

---

## 6. 安全起见：导出备份的 `charge.db`

原始业务库在 Windows 侧仓库 `server/sql/charge.db`，Hive 里的数据就是从那导出的。若要重导最新数据，用仓库里的 `_export_hive.py` 重新生成 `hive_sync/` 再上传。