# 🎨 工业级UI设计参考 - 基于现有系统

## 📷 参考系统分析

### 系统信息

```
产品名称: 两通电磁阀在线检测台 (MJRFR)
制造商: 螺丝市信新特自动化设备有限公司
特点: 工业级检测系统
```

### 参考界面的设计特点

#### 1️⃣ **色彩方案** 🎨

```
主色调:
├─ 背景深色: #3a3a3a 或 #4a4a4a (深灰色)
├─ 控制区背景: #6b5a4a (棕色/土黄色)
├─ 边框线: #8b8b8b (浅灰)
├─ 文字: 白色 #ffffff 和 浅灰 #e0e0e0

强调色:
├─ 按钮正常: #1e3a5f (深蓝)
├─ 按钮激活: #2563eb (亮蓝)
├─ 按钮禁用: #888888 (灰色)
├─ 成功/OK: #4CAF50 (绿色)
├─ 正在运行: #FFC107 (黄色/橙色)
├─ 错误/异常: #F44336 (红色)

对比度设计:
• 深色背景 + 白色文字 = 高对比度，工业环保
• 避免刺眼的纯白，使用略微偏黄的白色 #f5f5f0
• 关键数据用绿色或黄色突出
```

#### 2️⃣ **布局结构** 📐

```
参考系统的布局:
┌────────────────────────────────────────────────────┐
│  标题栏: 产品名称 + 时间戳                         │
├────────────────────────────────────────────────────┤
│  状态栏: 数据统计 (OK数/NG数/总数) | 当前状态     │
├────────────────────────────────────────────────────┤
│ ┌──────────────┐ ┌──────────────┐ ┌───────────┐  │
│ │ 左侧功能区1  │ │ 中央主功能   │ │ 右侧功能区│  │
│ │ • 控制项1   │ │ • 显示关键   │ │ • 辅助   │  │
│ │ • 控制项2   │ │   数据       │ │ • 其他   │  │
│ │ • 控制项3   │ │ • 实时曲线  │ │          │  │
│ │              │ │ • 状态指示  │ │          │  │
│ └──────────────┘ └──────────────┘ └───────────┘  │
├────────────────────────────────────────────────────┤
│  底部控制栏: [启动] [停止] [正常检测] [系统自检]  │
└────────────────────────────────────────────────────┘
```

#### 3️⃣ **信息卡片设计** 🃏

```
每个控制区 (卡片) 特点:
┌─ 标题 ─────────────────────────────┐
│                                     │
│ ┌─ 子区域1 ─────┐ ┌─ 子区域2 ────┐│
│ │ ● 状态指示    │ │ 数值: 12.34  ││
│ │ ● 文字标签    │ │ 单位: bar    ││
│ │ [按钮1] [按2] │ │ [状态指示]   ││
│ └───────────────┘ └──────────────┘│
│                                     │
└─────────────────────────────────────┘

特点:
• 标题栏: 棕色或深蓝色背景，白色加粗文字
• 内部背景: 稍微浅一些的颜色或透明+半透明
• 边框: 1-2像素的浅灰色线条
• 间距: 内部padding 10-15px
• 圆角: 可选，角度4-8px (显得更现代)
• 阴影: 可选，深色阴影增加层次感
```

#### 4️⃣ **按钮和控件设计** 🔘

```
按钮样式:
┌─────────────────────────────────┐
│ 标准按钮                        │
├─────────────────────────────────┤
│ 背景: #2563eb (蓝色)           │
│ 文字: #ffffff (白色加粗)       │
│ 边框: 1px #1e40af (更深的蓝)   │
│ 圆角: 4-6px                     │
│ Padding: 8-10px 水平          │
│ Padding: 6-8px 竖直            │
│ Font: 11pt 粗体                 │
└─────────────────────────────────┘

按钮状态:
• 正常: 蓝色背景, 白色文字
• 悬停: 背景稍微变亮 (#2f7ef4)
• 按下: 背景变暗 (#1e3a8a), 向下移动1px
• 禁用: 灰色背景 (#888888), 文字变浅

状态指示灯 (LED灯):
┌─────┐
│ ⬤ ║  OK / NG 指示
│ ⬤ ║  正在运行
│ ⬤ ║  告警
└─────┘

• 绿色⬤: 正常/成功 (#4CAF50)
• 黄色⬤: 运行中/警告 (#FFC107)
• 红色⬤: 错误/告警 (#F44336)
• 灰色⬤: 未激活 (#888888)

大小: 12-16px 直径
发光效果: 可选, 用阴影模拟 (box-shadow)
```

#### 5️⃣ **数据显示设计** 📊

```
关键数据显示:
┌─────────────────────┐
│ 参数名: 压力(bar)   │
│ 数值: 17.321        │
│ 单位: bar           │
│ 状态: OK ✓          │
└─────────────────────┘

特点:
• 参数名: 11pt 灰色文字, 略微变淡
• 数值: 14-16pt 绿色(正常)或红色(异常) 加粗等宽字体
• 单位: 10pt 灰色文字
• 状态: 12pt 绿色加粗, 带✓或✗符号

数值颜色规则:
✅ 正常范围: 绿色 #4CAF50
⚠️ 超过范围: 黄色 #FFC107
❌ 故障/异常: 红色 #F44336
❓ 未初始化: 灰色 #888888

表格设计:
┌─────────┬────────┬────────┐
│ 时间    │ 压力1  │ 温度1  │
├─────────┼────────┼────────┤
│ 14:00   │ 2.34   │ 25.1   │
│ 14:01   │ 2.35   │ 25.3   │  ← 交替行颜色 (深灰+浅灰)
│ 14:02   │ 2.36   │ 25.5   │
└─────────┴────────┴────────┘

• 表头: 深蓝或棕色背景, 白色加粗
• 行颜色: 交替显示 (深灰 #3a3a3a 和 #4a4a4a)
• 选中行: 蓝色高亮 (半透明)
• 行高: 28-32px
• 字体: 等宽字体 10-11pt
```

#### 6️⃣ **字体和排版** 📝

```
字体选择:
• 中文: Microsoft YaHei (微软雅黑) / SimHei (黑体)
• 英文/数字: Courier New (等宽) / Arial
• 参数值: Courier New 12-14pt (等宽便于对齐)

字体大小:
• 标题: 16-18pt 加粗
• 子标题: 12pt 加粗
• 正文: 11pt 正常
• 数值: 14-16pt 加粗 (等宽)
• 标签: 10pt 正常
• 单位: 9-10pt 变淡

行距: 1.4-1.6倍 (增加可读性)
字间距: 0-0.5pt (等宽字体不调整)
```

#### 7️⃣ **空间和对齐** 📏

```
间距规则 (8px 栅栏系统):
• 极小: 4px (元素间)
• 小: 8px (容器内部)
• 中: 16px (不同section间)
• 大: 24px (主要分区)
• 极大: 32px (顶部/底部)

对齐:
• 所有控件都严格对齐 (网格对齐)
• 水平对齐: 左对齐或居中对齐
• 竖直对齐: 顶部对齐或居中对齐

区域分隔:
• 使用细线条 1px #8b8b8b 分隔
• 或使用背景颜色变化分隔
• 不使用粗重的黑线
```

---

## 🎯 应用到水质测试系统的方案

### Terminal主控台应用

#### Dashboard仪表板参考

```
┌──────────────────────────────────────────────────────────┐
│  水质测试系统 - 主控台                   时间: 14:23:45  │
├──────────────────────────────────────────────────────────┤
│  [OK: 288] [NG: 80] 总数: 368  |  状态: 测试中...  清零   │
├──────────────────────────────────────────────────────────┤
│                                                           │
│ ┌─ PLC连接状态 ────────┐  ┌─ Station状态 ────────────┐ │
│ │ ● 已连接 ✓           │  │ ● Station1: 在线 ✓      │ │
│ │ IP: 192.168.33.1    │  │ ● Station2: 在线 ✓      │ │
│ │ 延迟: 1.2ms         │  │ ● Station3: 在线 ✓      │ │
│ │ 上次更新: 14:23:45  │  │ ● Station4: 离线 ✗      │ │
│ │                      │  │ 在线数: 3/4             │ │
│ │ [重新连接]          │  │                          │ │
│ └──────────────────────┘  └──────────────────────────┘ │
│                                                           │
│ ┌─ 实时数据 (最新一帧) ────────────────────────────────┐ │
│ │ 压力: P1=2.34 P2=2.45 P3=1.67 P4=1.71 bar          │ │
│ │ 温度: T1=25.3 T2=26.1 T3=24.1 T4=23.8 °C          │ │
│ │ 流量: 12.5 L/min   时间戳: 2026-02-03 14:23:45  │ │
│ │                                                       │ │
│ └───────────────────────────────────────────────────────┘ │
│                                                           │
│ ┌─ 采样统计 ─────────────────────────────────────────────┐ │
│ │ 采样率: 10Hz  总采样: 45,230帧  运行时长: 1h 23m 45s│ │
│ │ 丢包率: 0.00%  网络占用: 80Kbps                    │ │
│ └───────────────────────────────────────────────────────┘ │
│                                                           │
├──────────────────────────────────────────────────────────┤
│ [启动] [停止] [正常检测] [系统自检] [暂停] [重置] [帮助]  │
└──────────────────────────────────────────────────────────┘
```

#### 颜色应用

```
主控台配色:
• 背景: #3a3a3a (深灰)
• 卡片背景: #4a4a4a (稍浅灰)
• 标题背景: #2c3e50 (深蓝) 或 #6b5a4a (棕色)
• 文字: #f5f5f0 (微白)
• 重点数据: #4CAF50 (绿色) 或 #FFC107 (黄色)
• 按钮: #2563eb (蓝色)
• 错误: #F44336 (红色)
```

### 操作台应用

```
操作台同样使用相同的色彩方案和布局逻辑，但重点放在:
• 核心控制按钮 (启动/停止/紧急停止)
• 实时数据显示
• 状态指示灯
```

---

## 🎨 CSS/样式表实现

### Qt样式表 (.qss) 示例

```qss
/* 主窗口背景 */
QMainWindow {
    background-color: #3a3a3a;
}

/* 标签页样式 */
QTabWidget::pane {
    border: 1px solid #8b8b8b;
    background-color: #3a3a3a;
}

QTabBar::tab {
    background-color: #4a4a4a;
    color: #f5f5f0;
    padding: 8px 20px;
    border: 1px solid #8b8b8b;
    border-bottom: none;
}

QTabBar::tab:selected {
    background-color: #2c3e50;
    color: #ffffff;
    font-weight: bold;
}

/* 按钮样式 */
QPushButton {
    background-color: #2563eb;
    color: #ffffff;
    border: 1px solid #1e40af;
    border-radius: 4px;
    padding: 8px 16px;
    font-weight: bold;
    font-size: 11pt;
}

QPushButton:hover {
    background-color: #2f7ef4;
}

QPushButton:pressed {
    background-color: #1e3a8a;
    padding-top: 9px;
    padding-left: 17px;
}

QPushButton:disabled {
    background-color: #888888;
    color: #cccccc;
}

/* 标签样式 */
QLabel {
    color: #f5f5f0;
}

QLabel#title {
    font-size: 16pt;
    font-weight: bold;
    color: #4CAF50;
}

QLabel#value {
    font-size: 14pt;
    font-weight: bold;
    font-family: "Courier New";
    color: #4CAF50;
}

QLabel#warning {
    color: #FFC107;
}

QLabel#error {
    color: #F44336;
}

/* 表格样式 */
QTableWidget {
    background-color: #3a3a3a;
    alternate-background-color: #4a4a4a;
    color: #f5f5f0;
    gridline-color: #8b8b8b;
    border: 1px solid #8b8b8b;
}

QTableWidget::item {
    padding: 4px;
    height: 30px;
}

QTableWidget::item:selected {
    background-color: #2563eb;
}

QHeaderView::section {
    background-color: #2c3e50;
    color: #ffffff;
    padding: 4px;
    border: none;
    font-weight: bold;
}

/* 输入框样式 */
QLineEdit, QSpinBox, QComboBox {
    background-color: #4a4a4a;
    color: #f5f5f0;
    border: 1px solid #8b8b8b;
    padding: 6px;
    border-radius: 3px;
}

QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
    border: 2px solid #2563eb;
    background-color: #555555;
}

/* 状态指示灯 */
QLabel#statusOK {
    color: #4CAF50;
    font-weight: bold;
}

QLabel#statusNG {
    color: #F44336;
    font-weight: bold;
}

QLabel#statusRunning {
    color: #FFC107;
    font-weight: bold;
}

/* 卡片容器 */
QGroupBox {
    border: 1px solid #8b8b8b;
    border-radius: 4px;
    margin-top: 8px;
    padding-top: 8px;
    color: #f5f5f0;
    font-weight: bold;
    background-color: #4a4a4a;
}

QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 3px 0 3px;
    color: #ffffff;
}
```

---

## 🖼️ 实际应用指南

### 1. 创建样式文件

```
项目结构:
├── src/
│   └── gui/
│       ├── styles/
│       │   ├── dark_theme.qss      (深色主题)
│       │   ├── colors.qss          (颜色定义)
│       │   └── components.qss      (组件样式)
│       └── MainWindow.cpp
└── resources/
    └── styles.qrc                   (资源文件)
```

### 2. 在代码中加载样式

```cpp
// MainWindow.cpp
#include <QFile>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 加载样式表
    QFile styleFile(":/styles/dark_theme.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        qApp->setStyle("Fusion");  // 使用Fusion样式作为基础
        qApp->setStyleSheet(style);
        styleFile.close();
    }
    
    // ... 其他初始化代码
}
```

### 3. 为特殊控件设置objectName

```cpp
// 这样可以在QSS中用#id选择器来单独控制

QLabel *titleLabel = new QLabel("水质测试系统", this);
titleLabel->setObjectName("title");  // 对应 QLabel#title

QLabel *valueLabel = new QLabel("2.34", this);
valueLabel->setObjectName("value");  // 对应 QLabel#value
```

---

## 📐 布局建议

### 使用网格布局对齐元素

```cpp
// 使用QGridLayout确保对齐和间距一致

QGridLayout *layout = new QGridLayout(container);
layout->setSpacing(8);      // 8px 栅栏系统
layout->setContentsMargins(16, 16, 16, 16);

// 添加控件
layout->addWidget(label1, 0, 0);
layout->addWidget(value1, 0, 1);
layout->addWidget(label2, 1, 0);
layout->addWidget(value2, 1, 1);

// 设置列宽度一致
layout->setColumnStretch(0, 0);  // 标签列固定宽度
layout->setColumnStretch(1, 1);  // 值列自动扩展
```

---

## ✨ 高级特效

### 1. 状态指示灯发光效果

```cpp
// 创建发光的LED指示灯

class LEDIndicator : public QWidget {
    Q_OBJECT
public:
    void setState(bool on, const QString &color) {
        m_on = on;
        m_color = color;
        update();
    }
    
protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 绘制圆形LED
        QColor color = m_color;
        painter.fillEllipse(rect(), color);
        
        if (m_on) {
            // 添加发光效果
            QRadialGradient gradient(width()/2, height()/2, width()/2);
            gradient.setColorAt(0, color.lighter(150));
            gradient.setColorAt(1, color.darker(200));
            painter.setBrush(gradient);
            painter.drawEllipse(rect());
        }
    }
    
private:
    bool m_on = false;
    QString m_color = "#4CAF50";
};
```

### 2. 实时数据更新动画

```cpp
// 当数据变化时，用浅色闪烁表示更新

void updateValue(double newValue) {
    m_valueLabel->setText(QString::number(newValue, 'f', 2));
    
    // 创建一个颜色变化动画
    QPropertyAnimation *anim = new QPropertyAnimation(m_valueLabel, "palette");
    anim->setStartValue(QColor("#4CAF50"));
    anim->setEndValue(QColor("#ffff00"));
    anim->setDuration(200);
    anim->start();
}
```

### 3. 平滑过渡

```cpp
// 标签页切换时的平滑过渡

void onTabChanged(int index) {
    QWidget *newTab = m_tabWidget->widget(index);
    
    // 淡入效果
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect();
    newTab->setGraphicsEffect(effect);
    
    QPropertyAnimation *anim = new QPropertyAnimation(effect, "opacity");
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setDuration(300);
    anim->start();
}
```

---

## 📋 检查清单

实施样式时检查:

- [ ] 深色背景应用正确
- [ ] 所有文字可读性检查 (对比度 > 4.5:1)
- [ ] 所有按钮可点击性测试
- [ ] 状态指示灯清晰可见
- [ ] 数据显示准确和对齐
- [ ] 表格行交替颜色正确
- [ ] 响应式布局测试 (不同分辨率)
- [ ] 在实际工业平板上测试外观
- [ ] 触屏操作的按钮大小检查 (最小44×44px)
- [ ] 不同操作系统上的字体显示

---

## 🚀 实施步骤

1. **第1天**: 创建样式文件和颜色定义
2. **第2天**: 应用到MainWindow框架
3. **第3天**: 在各标签页中使用样式
4. **第4天**: 微调和优化
5. **第5天**: 在不同分辨率上测试

---

**现在可以开始实施工业级UI设计了！** ✨
