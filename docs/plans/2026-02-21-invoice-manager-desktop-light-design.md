# Invoice Manager Desktop - Light Mode (发票管理桌面版-浅色)

## Project Overview

- **Project Type**: Desktop Application (Qt Widgets)
- **UI Style**: macOS Light Mode
- **Language**: Chinese (中文)
- **Implementation**: Single-file Qt Widgets application

## UI Layout (Three-Column)

```
┌─────────────────────────────────────────────────────────────────────┐
│ [●][●][●]                        发票管理 - macOS                 │
├────────────┬─────────────────────┬────────────────────────────────┤
│            │  🔍 搜索发票...      │  INV-2023-001    [已支付]      │
│  主菜单    │ ─────────────────────│  [+添加] [导入] [导出] [打印]  │
│            │                     │                                │
│  📄 发票   │  极客科技 10月24日   │  ┌──────────────────────────┐  │
│  👥 客户   │  INV-2023-001 已支付 │  │  像素流工作室             │  │
│  📊 报告   │  网站重新设计...     │  │  上海市浦东新区...        │  │
│            │ ─────────────────────│  │                          │  │
│  分类      │  全球通  10月22日    │  │  发票号: INV-2023-001    │  │
│  ⭐ 星标   │  INV-2023-002 已逾期 │  │  日期: 2023年10月24日    │  │
│  ⚠ 已逾期  │  Q3营销咨询...       │  │  截止: 2023年11月24日    │  │
│            │ ─────────────────────│  └──────────────────────────┘  │
│  ─────────│  索伦特  10月20日    │                                │
│            │  INV-2023-003 草稿  │  ┌──────────────────────────┐  │
│  [管理员]  │  年度软件授权...     │  │ 付款方: 极客科技股份...  │  │
│            │                     │  │ 收件人: 王小明           │  │
│            │                     │  └──────────────────────────┘  │
│            │                     │                                │
│            │                     │  项目描述      数量   单价   金额│
│            │                     │  ─────────────────────────────────│
│            │                     │  网站UI/UX设计  1    ¥4,500 ¥4,500│
│            │                     │  前端开发(React)40时 ¥150  ¥6,000│
│            │                     │  服务器配置...  5时  ¥180  ¥900 │
│            │                     │                                │
│            │                     │        小计:    ¥11,400        │
│            │                     │        增值税(6%): ¥684         │
│            │                     │        总计:     ¥12,084       │
│            │                     │                                │
│            │                     │        感谢您的惠顾！           │
└────────────┴─────────────────────┴────────────────────────────────┘
```

## Color Palette

| Element | Color Code |
|---------|------------|
| Sidebar Background | `#F6F6F6` |
| List Background | `#FFFFFF` |
| Detail Background | `#FFFFFF` |
| Border | `#D1D1D1` |
| Active Item BG | `#E7E7E7` |
| Primary (Blue) | `#007AFF` |
| Status - Paid (Green) | `#34C759` |
| Status - Overdue (Red) | `#FF3B30` |
| Status - Draft (Gray) | `#8E8E93` |
| Text Primary | `#1D1D1F` |
| Text Secondary | `#86868B` |

## Components

| Component | Qt Widget |
|-----------|-----------|
| Main Window | `QMainWindow` |
| Sidebar | `QWidget` with `QVBoxLayout` |
| Navigation Items | `QPushButton` (flat style) |
| Invoice List | `QWidget` with vertical layout |
| Search Bar | `QLineEdit` |
| Detail Header | `QHBoxLayout` with labels |
| Toolbar | `QHBoxLayout` with `QPushButton` |
| Invoice Table | `QTableWidget` |
| Status Badge | `QLabel` with rounded style |

## Implementation Notes

- Single source file: `main.cpp`
- Use Qt Designer or code-based layout
- Static display only (no interactive logic)
- Chinese text content from design
- macOS-style window controls (red/yellow/green dots)
