#include "ExploreData.h"

#include <QHash>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace {

// 便捷构造
QVariantMap S(const std::initializer_list<std::pair<QString, QVariant>>& list) {
    QVariantMap m;
    for (const auto& p : list) m.insert(p.first, p.second);
    return m;
}

// —— 省-市-区 三级（每个区县给一个中心经纬度，用于「选择后跳转到对应位置」）——
// 覆盖直辖市 + 2 个热门省，数据精简保证可展示。
QVariantList buildRegions() {
    using P = QPair<double, double>; // lng, lat
    using D = std::tuple<QString, double, double>;
    auto districts = [](const QList<D>& list) -> QVariantList {
        QVariantList out;
        for (const auto& d : list)
            out.append(S({
                {"name",     std::get<0>(d)},
                {"lng",      std::get<1>(d)},
                {"lat",      std::get<2>(d)}
            }));
        return out;
    };
    auto city = [&](const QString& name, const QList<D>& dists) {
        return S({{"name", name}, {"districts", districts(dists)}});
    };

    return {
        S({
            {"name",       QStringLiteral("北京市")},
            {"type",       QStringLiteral("municipality")},
            {"cities",     QVariantList{
                city(QStringLiteral("北京市"), {
                    {QStringLiteral("东城区"),   116.42, 39.93},
                    {QStringLiteral("西城区"),   116.37, 39.92},
                    {QStringLiteral("朝阳区"),   116.45, 39.92},
                    {QStringLiteral("海淀区"),   116.30, 39.96},
                    {QStringLiteral("丰台区"),   116.29, 39.85},
                    {QStringLiteral("石景山区"), 116.22, 39.91},
                    {QStringLiteral("通州区"),   116.65, 39.91},
                    {QStringLiteral("昌平区"),   116.23, 40.22}
                })
            }}
        }),
        S({
            {"name",       QStringLiteral("上海市")},
            {"type",       QStringLiteral("municipality")},
            {"cities",     QVariantList{
                city(QStringLiteral("上海市"), {
                    {QStringLiteral("黄浦区"),   121.48, 31.23},
                    {QStringLiteral("徐汇区"),   121.44, 31.19},
                    {QStringLiteral("长宁区"),   121.43, 31.22},
                    {QStringLiteral("静安区"),   121.45, 31.23},
                    {QStringLiteral("浦东新区"), 121.54, 31.22},
                    {QStringLiteral("闵行区"),   121.38, 31.11}
                })
            }}
        }),
        S({
            {"name",       QStringLiteral("广东省")},
            {"type",       QStringLiteral("province")},
            {"cities",     QVariantList{
                city(QStringLiteral("广州市"), {
                    {QStringLiteral("天河区"),   113.36, 23.13},
                    {QStringLiteral("越秀区"),   113.27, 23.13},
                    {QStringLiteral("海珠区"),   113.32, 23.09},
                    {QStringLiteral("番禺区"),   113.39, 22.94}
                }),
                city(QStringLiteral("深圳市"), {
                    {QStringLiteral("福田区"),   114.05, 22.52},
                    {QStringLiteral("南山区"),   113.93, 22.54},
                    {QStringLiteral("罗湖区"),   114.13, 22.55},
                    {QStringLiteral("宝安区"),   113.89, 22.56}
                })
            }}
        }),
        S({
            {"name",       QStringLiteral("浙江省")},
            {"type",       QStringLiteral("province")},
            {"cities",     QVariantList{
                city(QStringLiteral("杭州市"), {
                    {QStringLiteral("上城区"),   120.17, 30.25},
                    {QStringLiteral("西湖区"),   120.13, 30.26},
                    {QStringLiteral("滨江区"),   120.21, 30.21},
                    {QStringLiteral("余杭区"),   120.30, 30.40}
                })
            }}
        })
    };
}

// —— 商户（station.merchant_id 引用）——
QVariantList buildMerchants() {
    return {
        S({
            {"id",                1},
            {"name",              QStringLiteral("国家电网（自营合作）")},
            {"contact_name",      QStringLiteral("郑经理")},
            {"contact_phone",     QStringLiteral("010-6001-0001")},
            {"cooperation_type",  QStringLiteral("partner")},
            {"status",            QStringLiteral("active")},
            {"order_count",       1822},
            {"settle_amount",     28903.4},
            {"service_score",     4.4}
        }),
        S({
            {"id",                2},
            {"name",              QStringLiteral("特来电特许经营")},
            {"contact_name",      QStringLiteral("李店长")},
            {"contact_phone",     QStringLiteral("021-5002-1000")},
            {"cooperation_type",  QStringLiteral("franchise")},
            {"status",            QStringLiteral("active")},
            {"order_count",       986},
            {"settle_amount",     14720.0},
            {"service_score",     4.2}
        }),
        S({
            {"id",                3},
            {"name",              QStringLiteral("星星充电·万达商圈")},
            {"contact_name",      QStringLiteral("王女士")},
            {"contact_phone",     QStringLiteral("020-3100-0088")},
            {"cooperation_type",  QStringLiteral("third_party")},
            {"status",            QStringLiteral("active")},
            {"order_count",       612},
            {"settle_amount",     9102.5},
            {"service_score",     3.9}
        })
    };
}

// —— 充电站 + 电桩 + 评价 + 电价 ——
// 覆盖：北京(朝阳/海淀/东城/西城/通州)、上海(浦东/徐汇)、深圳(南山/福田)、杭州(西湖)
// owner_type: self_run(自营，merchant 空) / partner / franchise / third_party
// 评分：用 review 聚合的 average overall_score；mock 时直接塞 station.rating

struct ChargerSpec { const char* code; const char* type; double power; const char* status; double voltage; double current; double temp; const char* fault; };

QVariantList makeChargers(int stationId, const QList<ChargerSpec>& specs) {
    QVariantList out;
    for (const auto& c : specs) {
        out.append(S({
            {"id",                      stationId * 100 + (int)(out.size() + 1)},
            {"code",                    QString::fromUtf8(c.code)},
            {"station_id",              stationId},
            {"type",                    QString::fromUtf8(c.type)},
            {"power",                   c.power},
            {"status",                  QString::fromUtf8(c.status)},
            {"voltage",                 c.voltage},
            {"current",                 c.current},
            {"temperature",             c.temp},
            {"fault_code",              QString::fromUtf8(c.fault)},
            {"comm_status",             QString(c.status == "offline" ? "abnormal" : "normal")},
            {"health_score",            85 + ((int)out.size() * 3) % 15},
            {"total_charge_count",      100 + (int)out.size() * 17},
            {"total_charge_duration",   2000 + (int)out.size() * 110},
            {"created_time",            QStringLiteral("2024-06-01 10:00:00")}
        }));
    }
    return out;
}

using R = std::tuple<QString, QString, QString, double, double, double, double, double, double, QString, QString>;
// nickname, content, tags[], overall, speed, device, parking, hygiene, service, useful_cnt, create_time
QVariantList makeReviews(const QList<R>& list) {
    QVariantList out;
    for (const auto& r : list) {
        out.append(S({
            {"nickname",           std::get<0>(r)},
            {"content",            std::get<1>(r)},
            {"tags",               QStringList{std::get<2>(r).split(',', Qt::SkipEmptyParts)}},
            {"overall_score",      std::get<3>(r)},
            {"speed_score",        std::get<4>(r)},
            {"device_score",       std::get<5>(r)},
            {"parking_score",      std::get<6>(r)},
            {"hygiene_score",      std::get<7>(r)},
            {"service_score",      std::get<8>(r)},
            {"useful_count",       std::get<9>(r).toInt()},
            {"create_time",        std::get<10>(r)}
        }));
    }
    return out;
}

struct StationSeed {
    int id;
    const char* name;
    const char* address;
    const char* area;
    double lng; double lat;
    const char* ownerType;
    int merchantId; // 0 表示无（self_run）
    int hasSwap;
    double serviceFee;
    double parkingFee;
    const char* businessHours;
    QStringList facilities;
    double onlineRate;
    double rating;
    int ratingCount;
    QList<ChargerSpec> chargers;
    QVariantList priceRules;
    QVariantList reviews;
    QString weatherArea;
};

QList<StationSeed> buildSeeds() {
    using CS = ChargerSpec;
    return {
        // —— 北京市 朝阳区 ——
        { 1, "星星充·国贸中心旗舰站", "北京市朝阳区建国门外大街 1 号国贸地下停车场 B2-18",
          "北京市/北京市/朝阳区", 116.461, 39.909, "self_run", 0, 1,
          0.80, 6.0, "00:00–24:00",
          {"washroom","convenience_store","wifi","rain_shelter"},
          0.97, 4.8, 328,
          { CS{"A-01","fast",180,"idle",750,120,32,""}, CS{"A-02","fast",180,"charging",720,168,41,""},
            CS{"A-03","fast",180,"fault",0,0,48,"E013"}, CS{"A-04","slow", 42,"idle",220,15,28,""},
            CS{"B-01","slow", 42,"idle",220,17,29,""},  CS{"B-02","slow", 42,"charging",230,14,35,""}},
          { S({{"level","valley"},{"price",0.38},{"time_range","00:00–08:00"}}),
            S({{"level","flat"},  {"price",0.72},{"time_range","08:00–10:00,12:00–18:00"}}),
            S({{"level","peak"},  {"price",1.15},{"time_range","10:00–12:00,18:00–21:00"}}),
            S({{"level","flat"},  {"price",0.72},{"time_range","21:00–24:00"}}) },
          makeReviews({
            R{"用户0982","位置好 4G 信号稳，但周六下午经常排队。","桩多,位置好,有雨棚",4.7,5.0,4.5,4.0,4.6,4.8,"18","2025-08-09 19:20"},
            R{"用户1211","下班常来充，有便利店买水","便利店,速度快",4.6,4.8,4.5,4.5,4.6,4.6,"9","2025-07-22 18:44"}
          }),
          "北京市/北京市/朝阳区"
        },
        { 2, "国网·朝阳公园南门站", "北京市朝阳区朝阳公园南路 1 号南门停车场",
          "北京市/北京市/朝阳区", 116.481, 39.937, "partner", 1, 0,
          0.60, 0.0, "06:00–23:00",
          {"rest_area","underground_parking","wifi"},
          0.95, 4.3, 182,
          { CS{"1号","fast",120,"idle",720,100,30,""}, CS{"2号","fast",120,"reserved",0,0,28,""},
            CS{"3号","slow", 42,"idle",220,14,27,""},  CS{"4号","slow", 42,"charging",225,15,33,""}},
          { S({{"level","valley"},{"price",0.32},{"time_range","00:00–07:00"}}),
            S({{"level","flat"},  {"price",0.60},{"time_range","07:00–10:00,11:30–17:30"}}),
            S({{"level","peak"},  {"price",0.98},{"time_range","17:30–21:30"}}),
            S({{"level","flat"},  {"price",0.60},{"time_range","21:30–24:00"}}) },
          makeReviews({
            R{"用户7011","免费停车 3 小时，有长椅歇脚","免费停车,有长椅",4.4,4.2,4.4,4.5,4.3,4.4,"6","2025-08-12 15:00"}
          }),
          "北京市/北京市/朝阳区"
        },
        { 3, "特来电·望京SOHO站", "北京市朝阳区望京街 10 号 B1 层 C 区",
          "北京市/北京市/朝阳区", 116.485, 39.995, "franchise", 2, 0,
          1.0, 8.0, "00:00–24:00",
          {"convenience_store","washroom"},
          0.88, 3.6, 95,
          { CS{"C-01","fast",160,"charging",700,150,40,""}, CS{"C-02","fast",160,"offline",0,0,0,"MISSING"},
            CS{"D-01","slow", 42,"idle",220,16,29,""}},
          { S({{"level","valley"},{"price",0.45},{"time_range","23:00–07:00"}}),
            S({{"level","flat"},  {"price",0.80},{"time_range","07:00–09:00,11:00–17:00,21:00–23:00"}}),
            S({{"level","peak"},  {"price",1.30},{"time_range","09:00–11:00,17:00–21:00"}}) },
          makeReviews({
            R{"用户3342","1 号桩很快，2 号桩经常离线","1号桩好,2号离线",3.2,3.0,3.0,4.0,3.0,3.8,"3","2025-08-03 10:30"}
          }),
          "北京市/北京市/朝阳区"
        },
        // —— 北京市 海淀区 ——
        { 4, "自营·中关村软件园旗舰站", "北京市海淀区东北旺西路 8 号院 中关村软件园地下车库",
          "北京市/北京市/海淀区", 116.305, 40.040, "self_run", 0, 1,
          0.75, 0.0, "00:00–24:00",
          {"wifi","rest_area","rain_shelter","convenience_store"},
          0.98, 4.9, 411,
          { CS{"A1","fast",250,"idle",800,200,30,""}, CS{"A2","fast",250,"charging",780,215,45,""},
            CS{"A3","fast",250,"idle",790,0,31,""},   CS{"A4","fast",250,"idle",800,0,29,""},
            CS{"B1","slow", 42,"idle",220,14,27,""},   CS{"B2","slow", 42,"charging",225,15,34,""}},
          { S({{"level","valley"},{"price",0.35},{"time_range","00:00–07:00"}}),
            S({{"level","flat"},  {"price",0.68},{"time_range","07:00–10:00,11:30–17:00"}}),
            S({{"level","peak"},  {"price",1.05},{"time_range","10:00–11:30,17:00–21:00"}}),
            S({{"level","flat"},  {"price",0.68},{"time_range","21:00–24:00"}}) },
          makeReviews({
            R{"用户6610","园区里很安静，A1-A4 都是 250kW，很快","250kW,快充,园区",5.0,5.0,5.0,4.8,4.9,5.0,"29","2025-08-15 07:10"},
            R{"用户9921","雨棚够用，下雨天也不怕。价格很合理","有雨棚,价格低",4.8,4.8,4.9,4.7,4.8,4.9,"12","2025-07-30 18:20"}
          }),
          "北京市/北京市/海淀区"
        },
        { 5, "国网·五道口地铁站", "北京市海淀区成府路 28 号华清嘉园地下一层",
          "北京市/北京市/海淀区", 116.338, 39.994, "partner", 1, 0,
          0.6, 5.0, "06:30–23:30",
          {"wifi","underground_parking"},
          0.9, 4.1, 123,
          { CS{"C1","fast",120,"idle",700,110,29,""},  CS{"C2","fast",120,"fault",0,0,42,"COMM_ERR"},
            CS{"D1","slow", 42,"charging",220,14,32,""}, CS{"D2","slow", 42,"idle",220,16,28,""}},
          { S({{"level","valley"},{"price",0.30},{"time_range","00:00–06:30"}}),
            S({{"level","flat"},  {"price",0.62},{"time_range","06:30–10:00,11:30–17:00"}}),
            S({{"level","peak"},  {"price",1.00},{"time_range","10:00–11:30,17:00–21:00"}}),
            S({{"level","flat"},  {"price",0.62},{"time_range","21:00–24:00"}}) },
          makeReviews({
            R{"用户5120","地铁换乘很方便，但 C2 坏了快 2 周没人修","换乘方便,C2损坏",3.9,4.2,3.5,4.3,4.0,4.0,"4","2025-08-01 20:00"}
          }),
          "北京市/北京市/海淀区"
        },
        // —— 北京市 东城区/西城区 ——
        { 6, "自营·王府井百货站", "北京市东城区王府井大街 255 号 王府井百货 B3 停车场",
          "北京市/北京市/东城区", 116.412, 39.914, "self_run", 0, 0,
          1.2, 12.0, "09:00–23:00",
          {"underground_parking","convenience_store"},
          0.93, 4.4, 78,
          { CS{"K1","fast",180,"charging",730,170,43,""}, CS{"K2","fast",180,"idle",740,0,30,""},
            CS{"M1","slow",42,"idle",220,15,28,""},    CS{"M2","slow",42,"reserved",0,0,26,""}},
          { S({{"level","flat"},  {"price",0.88},{"time_range","09:00–17:00"}}),
            S({{"level","peak"},  {"price",1.45},{"time_range","17:00–22:00"}}),
            S({{"level","flat"},  {"price",0.88},{"time_range","22:00–23:00"}}) },
          makeReviews({
            R{"用户8820","逛王府井顺便充，停车贵","商圈,停车贵",4.3,4.2,4.3,3.8,4.5,4.5,"2","2025-07-28 20:12"}
          }),
          "北京市/北京市/东城区"
        },
        { 7, "特来电·金融街站", "北京市西城区金融大街 7 号英蓝国际金融中心 B2",
          "北京市/北京市/西城区", 116.359, 39.916, "franchise", 2, 0,
          1.0, 10.0, "07:00–22:00",
          {"washroom","rest_area","wifi"},
          0.91, 4.0, 55,
          { CS{"X1","fast",160,"idle",720,120,30,""}, CS{"X2","fast",160,"idle",720,0,29,""},
            CS{"X3","slow",42,"charging",220,14,33,""}, CS{"X4","slow",42,"idle",220,15,28,""}},
          { S({{"level","flat"},  {"price",0.85},{"time_range","07:00–11:00,13:00–17:00"}}),
            S({{"level","peak"},  {"price",1.28},{"time_range","11:00–13:00,17:00–20:00"}}),
            S({{"level","flat"},  {"price",0.85},{"time_range","20:00–22:00"}}) },
          makeReviews({
            R{"用户0071","金融街办公楼里，B2 入口不太好找","办公区,入口难找",3.8,4.0,4.0,3.5,4.1,3.8,"1","2025-08-14 14:30"}
          }),
          "北京市/北京市/西城区"
        },
        // —— 北京 通州 ——
        { 8, "自营·城市副中心枢纽站", "北京市通州区新华东街 256 号",
          "北京市/北京市/通州区", 116.659, 39.909, "self_run", 0, 1,
          0.5, 0.0, "00:00–24:00",
          {"wifi","rain_shelter","convenience_store","washroom"},
          0.96, 4.5, 144,
          { CS{"T1","fast",200,"idle",760,140,29,""}, CS{"T2","fast",200,"charging",750,170,40,""},
            CS{"T3","fast",200,"idle",760,0,30,""},    CS{"T4","slow",42,"idle",220,15,28,""},
            CS{"T5","slow",42,"charging",225,14,34,""}},
          { S({{"level","valley"},{"price",0.33},{"time_range","00:00–07:00"}}),
            S({{"level","flat"},  {"price",0.65},{"time_range","07:00–10:00,11:30–17:30"}}),
            S({{"level","peak"},  {"price",1.00},{"time_range","10:00–11:30,17:30–21:00"}}),
            S({{"level","flat"},  {"price",0.65},{"time_range","21:00–24:00"}}) },
          makeReviews({
            R{"用户1123","副中心新站，设备都很新","新站,设备好",4.6,4.6,4.7,4.5,4.5,4.6,"5","2025-08-11 09:15"}
          }),
          "北京市/北京市/通州区"
        },
        // —— 上海 浦东 徐汇 ——
        { 9, "自营·陆家嘴滨江旗舰站", "上海市浦东新区滨江大道 2967 号 地下停车库 B2",
          "上海市/上海市/浦东新区", 121.505, 31.240, "self_run", 0, 0,
          0.95, 15.0, "00:00–24:00",
          {"rest_area","convenience_store","wifi","rain_shelter"},
          0.94, 4.6, 212,
          { CS{"LJZ-1","fast",180,"charging",740,160,39,""}, CS{"LJZ-2","fast",180,"idle",740,0,30,""},
            CS{"LJZ-3","fast",180,"idle",740,0,29,""},   CS{"Y-1","slow",42,"idle",220,15,28,""},
            CS{"Y-2","slow",42,"charging",225,14,35,""}},
          { S({{"level","valley"},{"price",0.36},{"time_range","22:00–06:00"}}),
            S({{"level","flat"},  {"price",0.72},{"time_range","06:00–08:00,11:00–17:00,21:00–22:00"}}),
            S({{"level","peak"},  {"price",1.18},{"time_range","08:00–11:00,17:00–21:00"}}) },
          makeReviews({
            R{"用户5500","江景真不错，晚上过来可以散步","江景,夜景",4.7,4.6,4.8,4.0,4.7,4.7,"14","2025-08-08 21:40"}
          }),
          "上海市/上海市/浦东新区"
        },
        { 10, "万达商圈·徐家汇站", "上海市徐汇区漕溪北路 88 号港汇恒隆广场 B3 停车场",
          "上海市/上海市/徐汇区", 121.437, 31.195, "third_party", 3, 0,
          1.10, 12.0, "09:30–22:30",
          {"underground_parking","convenience_store"},
          0.86, 3.8, 62,
          { CS{"A-1","fast",160,"idle",720,130,31,""}, CS{"A-2","fast",160,"charging",700,160,42,""},
            CS{"B-1","slow",42,"idle",220,14,28,""}},
          { S({{"level","flat"},  {"price",0.85},{"time_range","09:30–16:00,20:00–22:30"}}),
            S({{"level","peak"},  {"price",1.30},{"time_range","16:00–20:00"}}) },
          makeReviews({
            R{"用户7100","商场车位紧张，建议提前来","车位少,商场",3.6,4.0,3.8,3.0,3.8,3.8,"0","2025-07-26 17:10"}
          }),
          "上海市/上海市/徐汇区"
        },
        // —— 深圳 南山/福田 ——
        { 11, "自营·深圳湾科技园站", "广东省深圳市南山区科苑南路 2666 号 深圳湾创业投资大厦 B2",
          "广东省/深圳市/南山区", 113.943, 22.541, "self_run", 0, 1,
          0.7, 0.0, "00:00–24:00",
          {"wifi","rest_area","washroom","rain_shelter","convenience_store"},
          0.97, 4.7, 253,
          { CS{"N1","fast",240,"idle",800,200,30,""}, CS{"N2","fast",240,"charging",790,210,40,""},
            CS{"N3","fast",240,"idle",800,0,29,""},    CS{"N4","fast",240,"reserved",0,0,28,""},
            CS{"S1","slow",42,"idle",220,14,28,""},     CS{"S2","slow",42,"charging",225,15,34,""}},
          { S({{"level","valley"},{"price",0.34},{"time_range","00:00–07:00"}}),
            S({{"level","flat"},  {"price",0.66},{"time_range","07:00–10:00,11:30–17:00"}}),
            S({{"level","peak"},  {"price",1.08},{"time_range","10:00–11:30,17:00–21:00"}}),
            S({{"level","flat"},  {"price",0.66},{"time_range","21:00–24:00"}}) },
          makeReviews({
            R{"用户4450","科技园园区，240kW 真的快","240kW,园区,快",4.8,5.0,4.7,4.5,4.8,4.9,"19","2025-08-13 08:25"},
            R{"用户2209","便利店有咖啡","便利店,咖啡",4.6,4.6,4.6,4.5,4.7,4.6,"6","2025-07-29 09:00"}
          }),
          "广东省/深圳市/南山区"
        },
        { 12, "特来电·福田高铁站", "广东省深圳市福田区深南大道与益田路交汇处 福田高铁站 B1",
          "广东省/深圳市/福田区", 114.058, 22.541, "franchise", 2, 0,
          0.9, 10.0, "06:00–23:30",
          {"wifi","underground_parking","washroom"},
          0.89, 4.1, 83,
          { CS{"F1","fast",180,"idle",740,120,30,""}, CS{"F2","fast",180,"fault",0,0,45,"E011"},
            CS{"F3","slow",42,"charging",220,15,32,""}, CS{"F4","slow",42,"idle",220,14,29,""}},
          { S({{"level","valley"},{"price",0.40},{"time_range","00:00–06:00"}}),
            S({{"level","flat"},  {"price",0.70},{"time_range","06:00–10:00,11:30–17:30"}}),
            S({{"level","peak"},  {"price",1.15},{"time_range","10:00–11:30,17:30–21:00"}}),
            S({{"level","flat"},  {"price",0.70},{"time_range","21:00–24:00"}}) },
          makeReviews({
            R{"用户0038","换乘高铁很方便，F2 一直坏没人修","换乘高铁,F2坏",4.0,4.0,3.5,4.2,4.0,4.0,"2","2025-08-05 12:30"}
          }),
          "广东省/深圳市/福田区"
        },
        // —— 杭州 西湖 ——
        { 13, "自营·西湖文化广场站", "浙江省杭州市西湖区文三路 478 号 华星时代广场地下",
          "浙江省/杭州市/西湖区", 120.124, 30.283, "self_run", 0, 0,
          0.7, 5.0, "06:30–23:30",
          {"wifi","rain_shelter","rest_area"},
          0.93, 4.5, 165,
          { CS{"H1","fast",180,"idle",740,130,30,""}, CS{"H2","fast",180,"charging",730,160,41,""},
            CS{"H3","slow",42,"idle",220,14,28,""},  CS{"H4","slow",42,"charging",225,14,34,""},
            CS{"H5","slow",42,"idle",220,15,29,""}},
          { S({{"level","valley"},{"price",0.32},{"time_range","22:00–06:00"}}),
            S({{"level","flat"},  {"price",0.66},{"time_range","06:00–08:00,11:00–17:00,20:00–22:00"}}),
            S({{"level","peak"},  {"price",1.05},{"time_range","08:00–11:00,17:00–20:00"}}) },
          makeReviews({
            R{"用户3301","杭州的早晨在这里充电很舒服","杭州,舒服",4.6,4.6,4.5,4.6,4.7,4.7,"7","2025-08-10 06:50"}
          }),
          "浙江省/杭州市/西湖区"
        }
    };
}

// 天气按区域
QVariantMap weatherMock() {
    QVariantMap m;
    m["北京市/北京市/朝阳区"]   = S({{"condition",QStringLiteral("sunny")},{"temperature",28.5},{"update_time",QStringLiteral("2025-08-20 10:00")},
                                      {"forecast",QStringLiteral("[{\"hour\":2,\"condition\":\"cloudy\",\"t\":26},{\"hour\":4,\"condition\":\"rain\",\"t\":24}]")}});
    m["北京市/北京市/海淀区"]   = S({{"condition",QStringLiteral("cloudy")},{"temperature",27.0},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    m["北京市/北京市/东城区"]   = S({{"condition",QStringLiteral("sunny")},{"temperature",29.2},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    m["北京市/北京市/西城区"]   = S({{"condition",QStringLiteral("hot")},{"temperature",33.1},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    m["北京市/北京市/通州区"]   = S({{"condition",QStringLiteral("cloudy")},{"temperature",27.8},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    m["上海市/上海市/浦东新区"] = S({{"condition",QStringLiteral("rain")},{"temperature",26.2},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    m["上海市/上海市/徐汇区"]   = S({{"condition",QStringLiteral("rain")},{"temperature",25.9},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    m["广东省/深圳市/南山区"]   = S({{"condition",QStringLiteral("cloudy")},{"temperature",30.4},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    m["广东省/深圳市/福田区"]   = S({{"condition",QStringLiteral("hot")},{"temperature",34.8},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    m["浙江省/杭州市/西湖区"]   = S({{"condition",QStringLiteral("extreme")},{"temperature",39.2},{"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
    return m;
}

}  // namespace

// —— ExploreData 实现 ——
ExploreData::ExploreData(QObject* parent) : QObject(parent) {}

QVariantList ExploreData::regionsTree() const {
    static const QVariantList s = buildRegions();
    return s;
}

QVariantMap ExploreData::districtCenter(const QString& province,
                                        const QString& city,
                                        const QString& district) const {
    for (const auto& pv : regionsTree()) {
        QVariantMap p = pv.toMap();
        if (p.value("name").toString() != province) continue;
        for (const auto& cv : p.value("cities").toList()) {
            QVariantMap c = cv.toMap();
            if (c.value("name").toString() != city) continue;
            for (const auto& dv : c.value("districts").toList()) {
                QVariantMap d = dv.toMap();
                if (d.value("name").toString() == district) return d;
            }
        }
    }
    return {};
}

QVariantList ExploreData::merchants() const {
    static const QVariantList s = buildMerchants();
    return s;
}

QString ExploreData::merchantNameFor(int merchantId) const {
    for (const auto& m : merchants()) {
        auto mm = m.toMap();
        if (mm.value("id").toInt() == merchantId) return mm.value("name").toString();
    }
    if (merchantId == 0) return QStringLiteral("平台自营");
    return {};
}

QVariantList ExploreData::stations() const {
    static const auto seeds = buildSeeds();
    static const auto merchantsLocal = buildMerchants();
    QHash<int, QString> merchantName;
    merchantName.insert(0, QStringLiteral("平台自营"));
    for (const auto& m : merchantsLocal) {
        auto mm = m.toMap();
        merchantName.insert(mm.value("id").toInt(), mm.value("name").toString());
    }
    QVariantList out;
    for (const auto& s : seeds) {
        int fastIdle = 0, slowIdle = 0, fastCount = 0, slowCount = 0, offline = 0, fault = 0, charging = 0;
        for (const auto& c : s.chargers) {
            if (qstrcmp(c.type, "fast") == 0) fastCount++; else slowCount++;
            if (qstrcmp(c.status, "idle") == 0) {
                if (qstrcmp(c.type,"fast")==0) fastIdle++; else slowIdle++;
            } else if (qstrcmp(c.status,"offline")==0) offline++;
              else if (qstrcmp(c.status,"fault")==0)   fault++;
              else if (qstrcmp(c.status,"charging")==0) charging++;
        }
        QStringList areas = QString(s.area).split('/', Qt::SkipEmptyParts);
        out.append(S({
            {"id",                s.id},
            {"name",              QString::fromUtf8(s.name)},
            {"address",           QString::fromUtf8(s.address)},
            {"area",              QString::fromUtf8(s.area)},
            {"area_province",     areas.size() > 0 ? areas[0] : QString()},
            {"area_city",         areas.size() > 1 ? areas[1] : QString()},
            {"area_district",     areas.size() > 2 ? areas[2] : (areas.size()==2?areas[1]:QString())},
            {"longitude",         s.lng},
            {"latitude",          s.lat},
            {"total_chargers",    (int)s.chargers.size()},
            {"fast_count",        fastCount},
            {"slow_count",        slowCount},
            {"fast_idle",         fastIdle},
            {"slow_idle",         slowIdle},
            {"online_rate",       s.onlineRate},
            {"service_fee",       s.serviceFee},
            {"parking_fee",       s.parkingFee},
            {"business_hours",    QString::fromUtf8(s.businessHours)},
            {"facilities",        s.facilities},
            {"owner_type",        QString::fromUtf8(s.ownerType)},
            {"owner_label",
                QString(s.ownerType)=="self_run" ? QStringLiteral("自营") :
                QString(s.ownerType)=="partner"  ? QStringLiteral("合作商户") :
                QString(s.ownerType)=="franchise"? QStringLiteral("特许经营") :
                QString(s.ownerType)=="third_party"?QStringLiteral("第三方商圈"):
                QString::fromUtf8(s.ownerType) },
            {"merchant_id",       s.merchantId},
            {"merchant_name",     merchantName.value(s.merchantId)},
            {"has_swap",          s.hasSwap},
            {"rating",            s.rating},
            {"rating_count",      s.ratingCount},
            {"offline_count",     offline},
            {"fault_count",       fault},
            {"charging_count",    charging},
            {"weather_area",      s.weatherArea}
        }));
    }
    return out;
}

QVariantMap ExploreData::stationById(int stationId) const {
    for (const auto& s : stations()) {
        auto mm = s.toMap();
        if (mm.value("id").toInt() == stationId) return mm;
    }
    return {};
}

QVariantList ExploreData::chargersForStation(int stationId) const {
    static const auto seeds = buildSeeds();
    for (const auto& s : seeds)
        if (s.id == stationId)
            return makeChargers(stationId, s.chargers);
    return {};
}

QVariantList ExploreData::priceRulesForStation(int stationId) const {
    static const auto seeds = buildSeeds();
    for (const auto& s : seeds)
        if (s.id == stationId)
            return s.priceRules;
    return {};
}

QVariantList ExploreData::reviewsForStation(int stationId) const {
    static const auto seeds = buildSeeds();
    for (const auto& s : seeds)
        if (s.id == stationId)
            return s.reviews;
    return {};
}

QVariantMap ExploreData::weatherForArea(const QString& area) const {
    static const auto w = weatherMock();
    if (w.contains(area)) return w.value(area).toMap();
    // 默认
    return S({{"condition",QStringLiteral("sunny")},{"temperature",25.0},
              {"update_time",QStringLiteral("2025-08-20 10:00")},{"forecast",QString()}});
}

QStringList ExploreData::ownerTypeOptions() const {
    return {
        QStringLiteral("不限"),
        QStringLiteral("仅自营"),
        QStringLiteral("商户: 国家电网（自营合作）"),
        QStringLiteral("商户: 特来电特许经营"),
        QStringLiteral("商户: 星星充电·万达商圈")
    };
}

QStringList ExploreData::ratingThresholdOptions() const {
    return { QStringLiteral("不限"), QStringLiteral("≥ 4.5"), QStringLiteral("≥ 4.0"), QStringLiteral("≥ 3.0") };
}
