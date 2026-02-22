# Invoice Manager Desktop (发票管理桌面版)

基于Qt Widgets实现的macOS风格发票管理桌面应用程序。

## 项目结构

```
invoice_app/
├── main.cpp              # 主程序代码
├── CMakeLists.txt        # CMake构建配置
└── docs/
    └── plans/
        └── 2026-02-21-invoice-manager-desktop-light-design.md
```

## 构建方法

### 方式一：使用CMake

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译
cmake --build .

# 运行
./invoice_manager
```

### 方式二：使用Qt Creator

1. 打开Qt Creator
2. 打开 `CMakeLists.txt` 文件
3. 点击构建并运行

### 方式三：直接使用qmake

```bash
# 创建.pro文件
cat > invoice_manager.pro << 'EOF'
QT += widgets
CONFIG += c++17
SOURCES += main.cpp
EOF

# 构建
qmake invoice_manager.pro
make

# 运行
./invoice_manager
```

## 功能特性

- ✓ macOS风格浅色主题界面
- ✓ 三栏式布局：侧边栏 + 发票列表 + 详情预览
- ✓ 模拟窗口控制按钮（红黄绿圆点）
- ✓ 发票列表展示（含状态标签）
- ✓ 发票详情预览（含项目表格和金额统计）
- ✓ 响应式布局

## 环境要求

- Qt 6.x 或 Qt 5.x
- CMake 3.16+ (如使用CMake方式)
- C++17 兼容编译器
